#include "Sam2LabelCanvasCompat.h"

#include <QDebug>
#include <QPainterPath>
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QLineF>
#include <QWheelEvent>
#include <cmath>
#include <exception>
#include <iostream>
#include <utility>

#include <opencv2/imgproc.hpp>

namespace
{
const QString kIgnoreAddLabel = QStringLiteral("__yoloe_training_ignore__");
const QString kIgnoreSubtractLabel = QStringLiteral("__yoloe_training_ignore_subtract__");

bool isIgnoreLabel(const QString &labelName)
{
    return labelName == kIgnoreAddLabel || labelName == kIgnoreSubtractLabel;
}
} // namespace

Sam2LabelCanvasCompat::Sam2LabelCanvasCompat(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

bool Sam2LabelCanvasCompat::toInitSam2Model(const QString &modelDir)
{
    return toInitSamModel(0, modelDir);
}

bool Sam2LabelCanvasCompat::isSam2Ready() const
{
    return isSamReady();
}

QString Sam2LabelCanvasCompat::toGetSam2ModelDir() const
{
    return toGetSamModelDir();
}

bool Sam2LabelCanvasCompat::toInitSamModel(int samTypeIndex, const QString &modelDir)
{
    if (samTypeIndex != 0)
    {
        qWarning() << "Unsupported SAM type index for SAM2-only canvas:" << samTypeIndex;
        return false;
    }

    std::shared_ptr<TrtSam2> nextModel = std::make_shared<TrtSam2>();
    if (!nextModel->initialize(modelDir, 0))
    {
        qWarning() << "SAM2 init failed:" << modelDir;
        return false;
    }

    if (!m_currentImage.isNull())
    {
        nextModel->setCurrentImage(m_currentImage);
    }

    m_inferenceService = nextModel;
    m_samTypeIndex = 0;
    toClearCandidateRoi();
    update();
    return true;
}

bool Sam2LabelCanvasCompat::toSwitchSamType(int samTypeIndex)
{
    if (samTypeIndex != 0)
    {
        qWarning() << "Unsupported SAM type index for SAM2-only canvas:" << samTypeIndex;
        return false;
    }

    return m_inferenceService && m_inferenceService->isInitialized();
}

bool Sam2LabelCanvasCompat::isSamReady() const
{
    return m_inferenceService && m_inferenceService->isInitialized();
}

void Sam2LabelCanvasCompat::releaseSam2()
{
    m_inferenceService.reset();
    toClearCandidateRoi();
    update();
}

int Sam2LabelCanvasCompat::samTypeIndex() const
{
    return m_samTypeIndex;
}

QString Sam2LabelCanvasCompat::toGetSamModelDir() const
{
    if (!m_inferenceService)
    {
        return QString();
    }
    return m_inferenceService->modelDir();
}

void Sam2LabelCanvasCompat::toSetManualBoxMode(bool enabled)
{
    m_manualBoxMode = enabled;
    if (enabled)
    {
        m_manualPolygonMode = false;
        m_eraserMode = false;
        toCancelManualPolygon();
    }
    toClearCandidateRoi();
    update();
}

void Sam2LabelCanvasCompat::toSetManualPolygonMode(bool enabled)
{
    if (m_manualPolygonMode == enabled)
    {
        return;
    }
    m_manualPolygonMode = enabled;
    m_manualBoxMode = false;
    m_eraserMode = false;
    m_erasing = false;
    m_eraserStroke.clear();
    toCancelManualPolygon();
    toClearCandidateRoi();
    update();
}

void Sam2LabelCanvasCompat::toSetEraserMode(bool enabled)
{
    m_eraserMode = enabled;
    m_erasing = false;
    m_eraserStroke.clear();
    if (enabled)
    {
        m_manualBoxMode = false;
        m_manualPolygonMode = false;
        toCancelManualPolygon();
        toClearCandidateRoi();
    }
    update();
}

void Sam2LabelCanvasCompat::toSetEraserDiameter(qreal diameter)
{
    m_eraserDiameter = qMax<qreal>(2.0, diameter);
    update();
}

void Sam2LabelCanvasCompat::toSetIgnoreMaskVisible(bool visible)
{
    m_ignoreMaskVisible = visible;
    update();
}

bool Sam2LabelCanvasCompat::isManualPolygonMode() const
{
    return m_manualPolygonMode;
}

void Sam2LabelCanvasCompat::toSetImage(const QImage &image)
{
    toCancelManualPolygon();
    m_erasing = false;
    m_eraserStroke.clear();
    m_currentImage = image;
    m_hasMouseImagePos = false;
    m_lastMouseImagePos = QPointF();
    if (m_inferenceService)
    {
        m_inferenceService->setCurrentImage(image);
    }
    toClearCandidateRoi();
    toResetViewTransform();
    update();
}

void Sam2LabelCanvasCompat::toRemoveAllRoi()
{
    m_roiStore.clear();
    toCancelManualPolygon();
    m_erasing = false;
    m_eraserStroke.clear();
    toClearCandidateRoi();
    m_nextRoiKey = 1;
    m_highlightedRoiKey = -1;
    update();
}

void Sam2LabelCanvasCompat::toSetLabelNames(const QStringList &labelNames, const QVector<QColor> &labelColors)
{
    m_labelNames = labelNames;
    m_labelColors = labelColors;
}

void Sam2LabelCanvasCompat::toSetRoiDefaltType(int roiType)
{
    m_defaultRoiType = roiType;
}

void Sam2LabelCanvasCompat::toSetRoiDefaltName(const QString &labelName)
{
    m_defaultLabelName = labelName;
}

QVector<double> Sam2LabelCanvasCompat::getROI(const QString &key) const
{
    bool ok = false;
    const int intKey = key.toInt(&ok);
    if (!ok)
    {
        return QVector<double>();
    }
    return m_roiStore.value(intKey).roiData;
}

void Sam2LabelCanvasCompat::toAppItemLabel(const QString &labelName, int roiType, const QVector<double> &roiData)
{
    RoiEntry entry;
    entry.labelName = labelName;
    entry.roiType = roiType;
    entry.roiData = roiData;
    if (roiType == LpolygonROI)
    {
        for (int i = 0; i + 1 < roiData.size(); i += 2)
        {
            entry.maskContour.append(QPointF(roiData.at(i), roiData.at(i + 1)));
        }
    }

    m_roiStore.insert(m_nextRoiKey, entry);
    ++m_nextRoiKey;
    update();
}

void Sam2LabelCanvasCompat::toUpdateShow()
{
    update();
}

void Sam2LabelCanvasCompat::toRemoveRoiByKey(const QString &key)
{
    bool ok = false;
    const int intKey = key.toInt(&ok);
    if (!ok)
    {
        return;
    }

    m_roiStore.remove(intKey);
    toReindexRoiStore();
    if (!m_roiStore.contains(m_highlightedRoiKey))
    {
        m_highlightedRoiKey = -1;
    }
    update();
}

void Sam2LabelCanvasCompat::toSetHighlightedRoiKey(const QString &key)
{
    bool ok = false;
    const int intKey = key.toInt(&ok);
    const int nextHighlightedKey = ok && m_roiStore.contains(intKey) ? intKey : -1;
    if (m_highlightedRoiKey == nextHighlightedKey)
    {
        return;
    }

    m_highlightedRoiKey = nextHighlightedKey;
    update();
}

void Sam2LabelCanvasCompat::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(30, 30, 30));

    if (m_currentImage.isNull())
    {
        return;
    }

    const QRectF imageRect = toImageRectInWidget();
    if (imageRect.width() <= 0.0 || imageRect.height() <= 0.0)
    {
        return;
    }

    const qreal scale = imageRect.width() / qreal(m_currentImage.width());
    painter.drawImage(imageRect, m_currentImage);

    QPainterPath ignoreAddPath;
    QPainterPath ignoreSubtractPath;
    for (auto it = m_roiStore.constBegin(); it != m_roiStore.constEnd(); ++it)
    {
        const RoiEntry &entry = it.value();
        if (entry.maskContour.size() < 3)
        {
            continue;
        }
        if (entry.labelName == kIgnoreAddLabel)
        {
            ignoreAddPath = ignoreAddPath.united(toPolygonPath(entry.maskContour, imageRect, scale));
        }
        else if (entry.labelName == kIgnoreSubtractLabel)
        {
            ignoreSubtractPath = ignoreSubtractPath.united(toPolygonPath(entry.maskContour, imageRect, scale));
        }
    }
    if (m_ignoreMaskVisible && !ignoreAddPath.isEmpty())
    {
        const QPainterPath effectiveIgnorePath = ignoreAddPath.subtracted(ignoreSubtractPath);
        toFillCheckerboardPath(painter, effectiveIgnorePath, QColor(190, 70, 255));
        painter.save();
        painter.setPen(QPen(QColor(190, 70, 255), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(effectiveIgnorePath);
        painter.restore();
    }

    for (auto it = m_roiStore.constBegin(); it != m_roiStore.constEnd(); ++it)
    {
        const int roiIndex = it.key();
        const RoiEntry &entry = it.value();
        const QColor lineColor = toFindLabelColor(entry.labelName);
        const bool highlighted = roiIndex == m_highlightedRoiKey;
        const QColor drawColor = highlighted ? QColor(255, 230, 0) : lineColor;
        const int penWidth = highlighted ? 4 : 2;
        if (isIgnoreLabel(entry.labelName))
        {
            if (m_ignoreMaskVisible && highlighted && !entry.maskContour.isEmpty())
            {
                toDrawPolylineByImageData(painter, entry.maskContour, drawColor, imageRect, scale, true, penWidth);
            }
            continue;
        }
        if (entry.maskContour.isEmpty())
        {
            toDrawRoiByImageData(painter, entry.roiData, drawColor, imageRect, scale, false, penWidth);
        }
        else
        {
            toDrawPolylineByImageData(painter, entry.maskContour, drawColor, imageRect, scale, true, penWidth);
        }

        // Draw "index-label" at top-left of saved ROI.
        if (entry.roiData.size() >= 4)
        {
            qreal minX = entry.roiData[0];
            qreal minY = entry.roiData[1];
            for (int i = 2; i + 1 < entry.roiData.size(); i += 2)
            {
                minX = qMin(minX, entry.roiData[i]);
                minY = qMin(minY, entry.roiData[i + 1]);
            }

            const QPointF textPos(imageRect.left() + minX * scale + 3.0, imageRect.top() + minY * scale + 14.0);
            const QString text = QStringLiteral("%1-%2").arg(roiIndex).arg(entry.labelName);

            painter.save();
            QFont font = painter.font();
            font.setPointSize(10);
            painter.setFont(font);

            QFontMetrics fm(font);
            const QRect textRect = fm.boundingRect(text);
            const QRect bgRect(textPos.x() - 2,
                               textPos.y() - fm.ascent() - 1,
                               textRect.width() + 4,
                               textRect.height() + 2);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 120));
            painter.drawRect(bgRect);

            painter.setPen(QPen(Qt::white, 1));
            painter.drawText(textPos, text);
            painter.restore();
        }
    }

    if (!m_candidateMaskContour.isEmpty())
    {
        toDrawPolylineByImageData(painter, m_candidateMaskContour, QColor(0, 255, 120), imageRect, scale, true, 2);
    }

    if (m_candidateMaskContour.isEmpty() && !m_candidateMinRectRoi.isEmpty())
    {
        QVector<QPointF> points;
        for (int i = 0; i + 1 < m_candidateMinRectRoi.size(); i += 2)
        {
            points.push_back(QPointF(m_candidateMinRectRoi[i], m_candidateMinRectRoi[i + 1]));
        }
        toDrawPolylineByImageData(painter, points, QColor(0, 255, 255), imageRect, scale, true, 2);
    }

    if (m_candidateMaskContour.isEmpty() && !m_candidateRoi.isEmpty())
    {
        toDrawRoiByImageData(painter, m_candidateRoi, QColor(255, 215, 0), imageRect, scale, true);
    }

    if (m_draggingRectPrompt)
    {
        painter.setPen(QPen(QColor(255, 255, 255), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(m_dragStartPoint, m_dragCurrentPoint).normalized());
    }

    if (m_manualPolygonMode && !m_manualPolygonPoints.isEmpty())
    {
        QPolygonF widgetDraft;
        for (const QPointF &point : std::as_const(m_manualPolygonPoints))
        {
            widgetDraft << QPointF(imageRect.left() + point.x() * scale, imageRect.top() + point.y() * scale);
        }
        painter.setPen(QPen(QColor(98, 210, 162), 2.0, Qt::DashLine));
        painter.setBrush(QColor(98, 210, 162, 28));
        painter.drawPolyline(widgetDraft);
        if (m_hasManualPolygonCursor)
        {
            const QPointF cursorWidget(imageRect.left() + m_manualPolygonCursor.x() * scale,
                                       imageRect.top() + m_manualPolygonCursor.y() * scale);
            painter.drawLine(widgetDraft.last(), cursorWidget);
        }
        painter.setPen(QPen(QColor(8, 11, 16), 2.0));
        painter.setBrush(QColor(98, 210, 162));
        for (const QPointF &point : widgetDraft)
        {
            painter.drawEllipse(point, 4.5, 4.5);
        }
    }

    if (m_eraserMode && m_hasMouseImagePos)
    {
        const QPointF cursor(imageRect.left() + m_lastMouseImagePos.x() * scale,
                             imageRect.top() + m_lastMouseImagePos.y() * scale);
        painter.save();
        painter.setPen(QPen(QColor(255, 255, 255, 220), 1, Qt::DashLine));
        painter.setBrush(QColor(255, 255, 255, 35));
        painter.drawEllipse(cursor, m_eraserDiameter * scale * 0.5, m_eraserDiameter * scale * 0.5);
        if (m_eraserStroke.size() > 1)
        {
            QPainterPath strokePath;
            strokePath.moveTo(imageRect.left() + m_eraserStroke.first().x() * scale,
                              imageRect.top() + m_eraserStroke.first().y() * scale);
            for (int i = 1; i < m_eraserStroke.size(); ++i)
            {
                strokePath.lineTo(imageRect.left() + m_eraserStroke.at(i).x() * scale,
                                  imageRect.top() + m_eraserStroke.at(i).y() * scale);
            }
            painter.setPen(QPen(QColor(255, 255, 255, 70), m_eraserDiameter * scale, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(strokePath);
        }
        painter.restore();
    }

    toDrawImageInfoBar(painter);
}

void Sam2LabelCanvasCompat::mousePressEvent(QMouseEvent *event)
{
    if (m_currentImage.isNull())
    {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::RightButton)
    {
        m_draggingView = true;
        m_viewDragStartPoint = event->pos();
        m_viewDragStartCenter = m_viewCenterImage;
        toUpdateMouseImageInfo(event->pos());
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        toUpdateMouseImageInfo(event->pos());
        if (m_manualPolygonMode)
        {
            const QPointF imagePoint = toMapWidgetToImage(event->pos());
            if (imagePoint.x() >= 0.0)
            {
                m_manualPolygonPoints.append(imagePoint);
                m_manualPolygonCursor = imagePoint;
                m_hasManualPolygonCursor = true;
                setFocus(Qt::MouseFocusReason);
                update();
            }
            event->accept();
            return;
        }
        if (m_eraserMode)
        {
            const QPointF imagePoint = toMapWidgetToImage(event->pos());
            if (imagePoint.x() >= 0.0)
            {
                m_erasing = true;
                m_eraserStroke = {imagePoint};
                setFocus(Qt::MouseFocusReason);
                update();
            }
            event->accept();
            return;
        }
        if (m_defaultRoiType == LpointROI && !m_manualBoxMode)
        {
            toRunPointPrompt(event->pos());
            event->accept();
            return;
        }

        m_draggingRectPrompt = true;
        m_dragStartPoint = event->pos();
        m_dragCurrentPoint = event->pos();
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void Sam2LabelCanvasCompat::mouseMoveEvent(QMouseEvent *event)
{
    toUpdateMouseImageInfo(event->pos());
    if (m_manualPolygonMode)
    {
        const QPointF imagePoint = toMapWidgetToImage(event->pos());
        m_hasManualPolygonCursor = imagePoint.x() >= 0.0;
        if (m_hasManualPolygonCursor)
        {
            m_manualPolygonCursor = imagePoint;
        }
    }
    if (m_eraserMode && m_erasing)
    {
        const QPointF imagePoint = toMapWidgetToImage(event->pos());
        if (imagePoint.x() >= 0.0 &&
            (m_eraserStroke.isEmpty() || QLineF(m_eraserStroke.constLast(), imagePoint).length() >= 1.0))
        {
            m_eraserStroke.append(imagePoint);
        }
    }
    if (m_draggingView)
    {
        const qreal viewScale = toComputeFitScale() * m_zoomFactor;
        if (viewScale > 0.0)
        {
            const QPoint delta = event->pos() - m_viewDragStartPoint;
            m_viewCenterImage.setX(m_viewDragStartCenter.x() - delta.x() / viewScale);
            m_viewCenterImage.setY(m_viewDragStartCenter.y() - delta.y() / viewScale);
            toClampViewCenter();
        }
        update();
        event->accept();
        return;
    }

    if (m_draggingRectPrompt)
    {
        m_dragCurrentPoint = event->pos();
        update();
        event->accept();
        return;
    }

    update();
    QWidget::mouseMoveEvent(event);
}

void Sam2LabelCanvasCompat::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_draggingView)
    {
        toUpdateMouseImageInfo(event->pos());
        m_draggingView = false;
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_eraserMode && m_erasing)
    {
        m_erasing = false;
        const QVector<QPointF> stroke = m_eraserStroke;
        m_eraserStroke.clear();
        if (!stroke.isEmpty())
        {
            emit EraserStrokeCommitted(stroke, m_eraserDiameter);
        }
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_draggingRectPrompt)
    {
        toUpdateMouseImageInfo(event->pos());
        m_dragCurrentPoint = event->pos();
        m_draggingRectPrompt = false;
        toRunRectPrompt(m_dragStartPoint, m_dragCurrentPoint);
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void Sam2LabelCanvasCompat::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_manualPolygonMode)
    {
        toCommitManualPolygon();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton)
    {
        m_draggingView = false;
        toResetViewTransform();
        toUpdateMouseImageInfo(event->pos());
        update();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void Sam2LabelCanvasCompat::keyPressEvent(QKeyEvent *event)
{
    if (m_manualPolygonMode && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
    {
        toCommitManualPolygon();
        event->accept();
        return;
    }
    if (m_manualPolygonMode && event->key() == Qt::Key_Escape)
    {
        toCancelManualPolygon();
        update();
        event->accept();
        return;
    }
    if (m_eraserMode && event->key() == Qt::Key_Escape)
    {
        toSetEraserMode(false);
        emit EraserExitRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void Sam2LabelCanvasCompat::leaveEvent(QEvent *event)
{
    m_draggingView = false;
    m_hasMouseImagePos = false;
    update();
    QWidget::leaveEvent(event);
}

void Sam2LabelCanvasCompat::wheelEvent(QWheelEvent *event)
{
    if (m_currentImage.isNull())
    {
        QWidget::wheelEvent(event);
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        event->ignore();
        return;
    }

    const QPoint cursorPos = event->position().toPoint();
    const QPointF imagePosBefore = toMapWidgetToImage(cursorPos);

    const qreal step = 1.12;
    if (delta > 0)
    {
        m_zoomFactor *= step;
    }
    else
    {
        m_zoomFactor /= step;
    }
    m_zoomFactor = qBound<qreal>(0.1, m_zoomFactor, 20.0);

    if (imagePosBefore.x() >= 0.0)
    {
        const qreal fitScale = toComputeFitScale();
        const qreal viewScale = fitScale * m_zoomFactor;
        if (viewScale > 0.0)
        {
            const QPointF widgetCenter = rect().center();
            m_viewCenterImage.setX(imagePosBefore.x() - (cursorPos.x() - widgetCenter.x()) / viewScale);
            m_viewCenterImage.setY(imagePosBefore.y() - (cursorPos.y() - widgetCenter.y()) / viewScale);
        }
    }

    toClampViewCenter();
    update();
    event->accept();
}

QColor Sam2LabelCanvasCompat::toFindLabelColor(const QString &labelName) const
{
    const int idx = m_labelNames.indexOf(labelName);
    if (idx >= 0 && idx < m_labelColors.size())
    {
        return m_labelColors[idx];
    }
    return QColor(0, 255, 0);
}

QRectF Sam2LabelCanvasCompat::toImageRectInWidget() const
{
    if (m_currentImage.isNull())
    {
        return QRectF();
    }

    const qreal fitScale = toComputeFitScale();
    const qreal viewScale = fitScale * m_zoomFactor;
    const QSizeF drawSize(m_currentImage.width() * viewScale, m_currentImage.height() * viewScale);
    const QPointF widgetCenter = rect().center();

    const QPointF topLeft(widgetCenter.x() - m_viewCenterImage.x() * viewScale,
                          widgetCenter.y() - m_viewCenterImage.y() * viewScale);
    return QRectF(topLeft, drawSize);
}

QPointF Sam2LabelCanvasCompat::toMapWidgetToImage(const QPoint &pt) const
{
    const QRectF imageRect = toImageRectInWidget();
    if (imageRect.width() <= 0.0 || imageRect.height() <= 0.0)
    {
        return QPointF(-1.0, -1.0);
    }

    const qreal scaleX = m_currentImage.width() / imageRect.width();
    const qreal scaleY = m_currentImage.height() / imageRect.height();
    const qreal x = (pt.x() - imageRect.left()) * scaleX;
    const qreal y = (pt.y() - imageRect.top()) * scaleY;

    if (x < 0.0 || y < 0.0 || x >= m_currentImage.width() || y >= m_currentImage.height())
    {
        return QPointF(-1.0, -1.0);
    }

    return QPointF(x, y);
}

void Sam2LabelCanvasCompat::toRunPointPrompt(const QPoint &widgetPoint)
{
    if (m_manualBoxMode)
    {
        qDebug() << "Manual box mode ignores point prompt.";
        return;
    }
    const QPointF imagePoint = toMapWidgetToImage(widgetPoint);
    if (imagePoint.x() < 0.0)
    {
        return;
    }

    if (!isSamReady())
    {
        const QString errorMessage = QStringLiteral(
            "SAM point prompt ignored: SAM2 model is not initialized. Manual rectangle mode will be enabled.");
        qWarning() << errorMessage;
        emit SamRuntimeError(errorMessage);
        return;
    }

    try
    {
        const TrtSam2InferResult result = m_inferenceService->inferByPoint(imagePoint, toResolveCommitLabel());
        if (!result.success)
        {
            const QString errorMessage = result.errorMessage.isEmpty()
                                             ? QStringLiteral("SAM point prompt failed with empty error message.")
                                             : result.errorMessage;
            qWarning() << m_inferenceService->displayName() << "point prompt failed:" << errorMessage;
            toClearCandidateRoi();
            update();
            emit SamRuntimeError(errorMessage);
            return;
        }

        toCommitInferObjects(result);
    }
    catch (const std::exception &ex)
    {
        const QString errorMessage =
            QStringLiteral("SAM point prompt exception: %1").arg(QString::fromLocal8Bit(ex.what()));
        qWarning() << errorMessage;
        toClearCandidateRoi();
        update();
        emit SamRuntimeError(errorMessage);
    }
    catch (...)
    {
        const QString errorMessage = QStringLiteral("SAM point prompt failed with unknown exception.");
        qWarning() << errorMessage;
        toClearCandidateRoi();
        update();
        emit SamRuntimeError(errorMessage);
    }
}

void Sam2LabelCanvasCompat::toRunRectPrompt(const QPoint &beginPt, const QPoint &endPt)
{
    const QPointF p1 = toMapWidgetToImage(beginPt);
    const QPointF p2 = toMapWidgetToImage(endPt);

    if (p1.x() < 0.0 || p2.x() < 0.0)
    {
        update();
        return;
    }

    const QRectF promptRect = QRectF(p1, p2).normalized();
    if (m_manualBoxMode)
    {
        toCommitManualRect(promptRect);
        return;
    }

    if (!isSamReady())
    {
        const QString errorMessage = QStringLiteral(
            "SAM box prompt ignored: SAM2 model is not initialized. Manual rectangle mode will be enabled.");
        qWarning() << errorMessage;
        update();
        emit SamRuntimeError(errorMessage);
        return;
    }

    try
    {
        const TrtSam2InferResult result = m_inferenceService->inferByRect(promptRect, toResolveCommitLabel());
        if (!result.success)
        {
            const QString errorMessage = result.errorMessage.isEmpty()
                                             ? QStringLiteral("SAM box prompt failed with empty error message.")
                                             : result.errorMessage;
            qWarning() << m_inferenceService->displayName() << "box prompt failed:" << errorMessage;
            toClearCandidateRoi();
            update();
            emit SamRuntimeError(errorMessage);
            return;
        }

        toCommitInferObjects(result);
    }
    catch (const std::exception &ex)
    {
        const QString errorMessage =
            QStringLiteral("SAM box prompt exception: %1").arg(QString::fromLocal8Bit(ex.what()));
        qWarning() << errorMessage;
        toClearCandidateRoi();
        update();
        emit SamRuntimeError(errorMessage);
    }
    catch (...)
    {
        const QString errorMessage = QStringLiteral("SAM box prompt failed with unknown exception.");
        qWarning() << errorMessage;
        toClearCandidateRoi();
        update();
        emit SamRuntimeError(errorMessage);
    }
}

void Sam2LabelCanvasCompat::toCommitManualRect(const QRectF &rect)
{
    const QRectF normalizedRect = rect.normalized();
    if (normalizedRect.width() < 2.0 || normalizedRect.height() < 2.0)
    {
        qWarning() << "Manual box prompt ignored: rectangle is too small.";
        update();
        return;
    }

    QVector<double> roiData;
    roiData << normalizedRect.left() << normalizedRect.top() << normalizedRect.right() << normalizedRect.top()
            << normalizedRect.right() << normalizedRect.bottom() << normalizedRect.left() << normalizedRect.bottom();

    RoiEntry entry;
    entry.labelName = toResolveCommitLabel();
    entry.roiType = LrectangleROI;
    entry.roiData = roiData;

    const int commitKey = m_nextRoiKey;
    m_roiStore.insert(commitKey, entry);
    ++m_nextRoiKey;

    toClearCandidateRoi();
    m_candidateRoi = roiData;
    emit SigGetDataUpdate(QString::number(commitKey), entry.labelName, entry.roiType);
    update();
}

void Sam2LabelCanvasCompat::toCommitManualPolygon()
{
    if (m_defaultLabelName.isEmpty())
    {
        const QString errorMessage = QString(u8"请先选择目标类别");
        qWarning() << errorMessage;
        emit ManualPolygonError(errorMessage);
        return;
    }
    QVector<QPointF> points;
    points.reserve(m_manualPolygonPoints.size());
    for (const QPointF &point : std::as_const(m_manualPolygonPoints))
    {
        if (points.isEmpty() ||
            std::hypot(point.x() - points.constLast().x(), point.y() - points.constLast().y()) >= 0.5)
        {
            points.append(point);
        }
    }
    if (points.size() > 1 &&
        std::hypot(points.first().x() - points.constLast().x(), points.first().y() - points.constLast().y()) < 0.5)
    {
        points.removeLast();
    }
    if (points.size() < 3)
    {
        const QString errorMessage = QString(u8"多边形至少需要3个不同顶点");
        qWarning() << errorMessage;
        emit ManualPolygonError(errorMessage);
        return;
    }

    double twiceArea = 0.0;
    for (int i = 0; i < points.size(); ++i)
    {
        const QPointF &current = points.at(i);
        const QPointF &next = points.at((i + 1) % points.size());
        twiceArea += current.x() * next.y() - next.x() * current.y();
    }
    if (std::abs(twiceArea) < 2.0)
    {
        const QString errorMessage = QString(u8"多边形面积必须大于0");
        qWarning() << errorMessage;
        emit ManualPolygonError(errorMessage);
        return;
    }

    QVector<double> roiData;
    roiData.reserve(points.size() * 2);
    for (const QPointF &point : std::as_const(points))
    {
        roiData << point.x() << point.y();
    }

    RoiEntry entry;
    entry.labelName = toResolveCommitLabel();
    entry.roiType = LpolygonROI;
    entry.roiData = roiData;
    entry.maskContour = points;

    const int commitKey = m_nextRoiKey;
    m_roiStore.insert(commitKey, entry);
    ++m_nextRoiKey;
    toCancelManualPolygon();
    emit SigGetDataUpdate(QString::number(commitKey), entry.labelName, entry.roiType);
    update();
}

void Sam2LabelCanvasCompat::toCancelManualPolygon()
{
    m_manualPolygonPoints.clear();
    m_manualPolygonCursor = QPointF();
    m_hasManualPolygonCursor = false;
}

void Sam2LabelCanvasCompat::toDrawRoiByImageData(QPainter &painter,
                                                 const QVector<double> &roiData,
                                                 const QColor &color,
                                                 const QRectF &imageRect,
                                                 qreal scale,
                                                 bool dashed,
                                                 int penWidth) const
{
    if (roiData.size() < 4)
    {
        return;
    }

    qreal minX = 0.0;
    qreal minY = 0.0;
    qreal maxX = 0.0;
    qreal maxY = 0.0;
    bool firstPoint = true;

    for (int i = 0; i + 1 < roiData.size(); i += 2)
    {
        const qreal sx = imageRect.left() + roiData[i] * scale;
        const qreal sy = imageRect.top() + roiData[i + 1] * scale;
        if (firstPoint)
        {
            minX = maxX = sx;
            minY = maxY = sy;
            firstPoint = false;
        }
        else
        {
            minX = qMin(minX, sx);
            minY = qMin(minY, sy);
            maxX = qMax(maxX, sx);
            maxY = qMax(maxY, sy);
        }
    }

    if (firstPoint)
    {
        return;
    }

    QPen pen(color, penWidth);
    if (dashed)
    {
        pen.setStyle(Qt::DashLine);
    }

    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(QPointF(minX, minY), QPointF(maxX, maxY)));
    //    std::cout<<"toDrawRoiByImageData: "<<minX<< ","<<minY<<","<<maxX<<","<<maxY<<std::endl;
}

void Sam2LabelCanvasCompat::toDrawPolylineByImageData(QPainter &painter,
                                                      const QVector<QPointF> &points,
                                                      const QColor &color,
                                                      const QRectF &imageRect,
                                                      qreal scale,
                                                      bool closed,
                                                      int penWidth) const
{
    if (points.size() < 2)
    {
        return;
    }

    QPen pen(color, penWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QPolygonF polygon;
    polygon.reserve(points.size() + (closed ? 1 : 0));
    for (const QPointF &pt : points)
    {
        polygon.push_back(QPointF(imageRect.left() + pt.x() * scale, imageRect.top() + pt.y() * scale));
    }
    if (closed)
    {
        polygon.push_back(polygon.first());
    }

    painter.drawPolyline(polygon);
    //    std::cout<<"toDrawPolylineByImageData: polygon.size():"<<polygon.size();
    //    std::cout<<"toDrawPolylineByImageData: "<<polygon[0].x()<< ","<<polygon[0].y()<<","<<polygon[1].x()<<
    //    ","<<polygon[1].y()<<std::endl;
}

QPainterPath
Sam2LabelCanvasCompat::toPolygonPath(const QVector<QPointF> &points, const QRectF &imageRect, qreal scale) const
{
    if (points.size() < 3)
    {
        return QPainterPath();
    }

    QPolygonF polygon;
    polygon.reserve(points.size());
    for (const QPointF &point : points)
    {
        polygon.append(QPointF(imageRect.left() + point.x() * scale, imageRect.top() + point.y() * scale));
    }

    QPainterPath path;
    path.addPolygon(polygon);
    path.closeSubpath();
    return path;
}

void Sam2LabelCanvasCompat::toFillCheckerboardPath(QPainter &painter,
                                                   const QPainterPath &path,
                                                   const QColor &color) const
{
    if (path.isEmpty())
    {
        return;
    }

    constexpr int tileSize = 12;
    const QRect bounds = path.boundingRect().toAlignedRect();
    const int beginX = (bounds.left() / tileSize) * tileSize;
    const int beginY = (bounds.top() / tileSize) * tileSize;
    QColor firstColor = color;
    QColor secondColor = color.darker(145);
    firstColor.setAlpha(58);
    secondColor.setAlpha(25);

    painter.save();
    painter.setClipPath(path, Qt::IntersectClip);
    painter.setPen(Qt::NoPen);
    for (int y = beginY; y <= bounds.bottom(); y += tileSize)
    {
        for (int x = beginX; x <= bounds.right(); x += tileSize)
        {
            const bool firstTile = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            painter.fillRect(QRect(x, y, tileSize, tileSize), firstTile ? firstColor : secondColor);
        }
    }
    painter.restore();
}

void Sam2LabelCanvasCompat::toUpdateMouseImageInfo(const QPoint &widgetPoint)
{
    const QPointF imagePoint = toMapWidgetToImage(widgetPoint);
    if (imagePoint.x() < 0.0 || m_currentImage.isNull())
    {
        m_hasMouseImagePos = false;
        return;
    }

    m_hasMouseImagePos = true;
    m_lastMouseImagePos = imagePoint;
}

QString Sam2LabelCanvasCompat::toImageInfoText() const
{
    if (m_currentImage.isNull())
    {
        return QString();
    }

    QString text = QStringLiteral("W:%1 H:%2 |").arg(m_currentImage.width()).arg(m_currentImage.height());
    if (!m_hasMouseImagePos)
    {
        text += QStringLiteral(" X:- Y:- | R:- G:- B:-");
        return text;
    }

    const int x = qBound(0, static_cast<int>(m_lastMouseImagePos.x()), m_currentImage.width() - 1);
    const int y = qBound(0, static_cast<int>(m_lastMouseImagePos.y()), m_currentImage.height() - 1);
    const QColor pixelColor = m_currentImage.pixelColor(x, y);
    text += QStringLiteral(" X:%1 Y:%2 | R:%3 G:%4 B:%5")
                .arg(x)
                .arg(y)
                .arg(pixelColor.red())
                .arg(pixelColor.green())
                .arg(pixelColor.blue());
    return text;
}

void Sam2LabelCanvasCompat::toDrawImageInfoBar(QPainter &painter) const
{
    const QString text = toImageInfoText();
    if (text.isEmpty())
    {
        return;
    }

    painter.save();
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(false);
    painter.setFont(font);

    const QFontMetrics fm(font);
    const int margin = 3;
    const int paddingX = 3;
    const int paddingY = 1;
    const int textWidth = fm.horizontalAdvance(text);
    const int barWidth = qMin(width() - margin * 2, textWidth + paddingX * 2);
    const int barHeight = fm.height() + paddingY * 2;
    const QRect barRect(margin, height() - barHeight - margin, qMax(0, barWidth), barHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 190));
    painter.drawRect(barRect);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawText(barRect.adjusted(paddingX, paddingY, -paddingX, -paddingY),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     fm.elidedText(text, Qt::ElideRight, qMax(0, barRect.width() - paddingX * 2)));
    painter.restore();
}
void Sam2LabelCanvasCompat::toCommitInferObjects(const TrtSam2InferResult &result)
{
    if (!result.success || result.objects.isEmpty())
    {
        return;
    }

    toClearCandidateRoi();
    for (const TrtSam2ObjectResult &objectResult : result.objects)
    {
        const QVector<QPointF> contour = toPostProcessMaskContour(objectResult.maskContour);
        if (contour.size() < 3)
        {
            const QString errorMessage = QStringLiteral("SAM2 mask has no valid contour after YOLOE post-processing.");
            qWarning() << errorMessage;
            emit SamRuntimeError(errorMessage);
            continue;
        }

        QVector<double> commitRoi;
        commitRoi.reserve(contour.size() * 2);
        for (const QPointF &point : contour)
        {
            commitRoi << point.x() << point.y();
        }

        RoiEntry entry;
        entry.labelName = objectResult.labelName.isEmpty() ? toResolveCommitLabel() : objectResult.labelName;
        entry.roiType = LpolygonROI;
        entry.roiData = commitRoi;
        entry.maskContour = contour;

        const int commitKey = m_nextRoiKey;
        m_roiStore.insert(commitKey, entry);
        ++m_nextRoiKey;

        m_candidateMinRectRoi.clear();
        m_candidateRoi = commitRoi;
        m_candidateMaskContour = contour;

        emit SigGetDataUpdate(QString::number(commitKey), entry.labelName, entry.roiType);
    }
    update();
}

QVector<QPointF> Sam2LabelCanvasCompat::toPostProcessMaskContour(const QVector<QPointF> &contour) const
{
    QVector<QPointF> result;
    if (contour.size() < 3 || m_currentImage.isNull())
    {
        return result;
    }

    std::vector<cv::Point> source;
    source.reserve(static_cast<size_t>(contour.size()));
    for (const QPointF &point : contour)
    {
        source.emplace_back(qRound(point.x()), qRound(point.y()));
    }

    cv::Mat mask = cv::Mat::zeros(m_currentImage.height(), m_currentImage.width(), CV_8UC1);
    cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{source}, cv::Scalar(255));
    const double maskArea = cv::countNonZero(mask);
    if (maskArea < 10.0)
    {
        return result;
    }

    const int kernelSize = maskArea < 64.0 * 64.0 ? 3 : 7;
    cv::GaussianBlur(mask, mask, cv::Size(kernelSize, kernelSize), 0.0);
    cv::threshold(mask, mask, 127.0, 255.0, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty())
    {
        return result;
    }

    size_t largestIndex = 0;
    double largestArea = 0.0;
    for (size_t index = 0; index < contours.size(); ++index)
    {
        const double area = std::abs(cv::contourArea(contours[index]));
        if (area > largestArea)
        {
            largestArea = area;
            largestIndex = index;
        }
    }
    if (largestArea / maskArea < 0.05)
    {
        return result;
    }

    double ratio = 0.001;
    if (largestArea < 32.0 * 32.0)
    {
        ratio = 0.01;
    }
    else if (largestArea < 64.0 * 64.0)
    {
        ratio = 0.005;
    }
    else if (largestArea < 96.0 * 96.0)
    {
        ratio = 0.003;
    }
    else if (largestArea < 128.0 * 128.0)
    {
        ratio = 0.002;
    }

    std::vector<cv::Point> simplified;
    const double perimeter = cv::arcLength(contours[largestIndex], true);
    while (ratio > 0.00001)
    {
        cv::approxPolyDP(contours[largestIndex], simplified, ratio * perimeter, true);
        if (simplified.size() >= 3)
        {
            break;
        }
        ratio *= 0.5;
    }
    if (simplified.size() < 3)
    {
        return result;
    }

    result.reserve(static_cast<qsizetype>(simplified.size()));
    for (const cv::Point &point : simplified)
    {
        result.append(QPointF(point.x, point.y));
    }
    return result;
}

void Sam2LabelCanvasCompat::toClearCandidateRoi()
{
    m_candidateRoi.clear();
    m_candidateMinRectRoi.clear();
    m_candidateMaskContour.clear();
}

void Sam2LabelCanvasCompat::toReindexRoiStore()
{
    QMap<int, RoiEntry> compactMap;
    int idx = 1;
    for (auto it = m_roiStore.constBegin(); it != m_roiStore.constEnd(); ++it)
    {
        compactMap.insert(idx, it.value());
        ++idx;
    }
    m_roiStore = compactMap;
    m_nextRoiKey = idx;
}

QString Sam2LabelCanvasCompat::toResolveCommitLabel() const
{
    if (!m_defaultLabelName.isEmpty())
    {
        return m_defaultLabelName;
    }
    if (!m_labelNames.isEmpty())
    {
        return m_labelNames.first();
    }
    return QStringLiteral("default");
}

void Sam2LabelCanvasCompat::toResetViewTransform()
{
    m_zoomFactor = 1.0;
    if (m_currentImage.isNull())
    {
        m_viewCenterImage = QPointF(0.0, 0.0);
    }
    else
    {
        m_viewCenterImage = QPointF((m_currentImage.width() - 1) / 2.0, (m_currentImage.height() - 1) / 2.0);
    }
}

qreal Sam2LabelCanvasCompat::toComputeFitScale() const
{
    if (m_currentImage.isNull())
    {
        return 1.0;
    }

    const QSize widgetSize = size();
    if (widgetSize.width() <= 0 || widgetSize.height() <= 0)
    {
        return 1.0;
    }

    return qMin(widgetSize.width() / qreal(m_currentImage.width()),
                widgetSize.height() / qreal(m_currentImage.height()));
}

void Sam2LabelCanvasCompat::toClampViewCenter()
{
    if (m_currentImage.isNull())
    {
        m_viewCenterImage = QPointF(0.0, 0.0);
        return;
    }

    const qreal maxX = qMax(0, m_currentImage.width() - 1);
    const qreal maxY = qMax(0, m_currentImage.height() - 1);
    m_viewCenterImage.setX(qBound<qreal>(0.0, m_viewCenterImage.x(), maxX));
    m_viewCenterImage.setY(qBound<qreal>(0.0, m_viewCenterImage.y(), maxY));
}
