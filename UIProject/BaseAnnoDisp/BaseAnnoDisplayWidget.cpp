#include "BaseAnnoDisplayWidget.h"

#include <QDebug>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

namespace
{
QColor colorFromInt(int value)
{
    return QColor((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

void drawRoundedLabel(QPainter *painter, const QRect &rect, const QColor &color, const QString &text)
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(10, 14, 20, 210));
    painter->drawRoundedRect(rect, 5, 5);

    painter->setBrush(color);
    painter->drawRoundedRect(QRect(rect.left(), rect.top(), 4, rect.height()), 2, 2);

    painter->setPen(QColor(235, 240, 248));
    painter->drawText(rect.adjusted(9, 0, -5, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}
} // namespace

BaseAnnoDisplayWidget::BaseAnnoDisplayWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 380);
}

bool BaseAnnoDisplayWidget::loadImage(const QString &imagePath, QString *errorMessage)
{
    QImage image;
    if (!image.load(imagePath))
    {
        const QString message = QString(u8"无法加载图像文件: %1").arg(imagePath);
        qCritical().noquote() << message;
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    }

    return setImage(image, errorMessage);
}

bool BaseAnnoDisplayWidget::setImage(const QImage &image, QString *errorMessage)
{
    if (image.isNull())
    {
        const QString message = QString(u8"图像为空，无法显示");
        qCritical().noquote() << message;
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    }

    m_image = image;
    m_zoomFactor = 1.0;
    m_viewOffsetWidget = QPointF(0.0, 0.0);
    m_tempPreview = BaseAnnoTempPreview{};
    m_polygonDraftImage.clear();
    m_selectedAnnotationIndex = -1;
    update();
    emitViewportStatus();
    return true;
}

void BaseAnnoDisplayWidget::clearImage()
{
    m_image = QImage();
    m_tempPreview = BaseAnnoTempPreview{};
    m_polygonDraftImage.clear();
    m_selectedAnnotationIndex = -1;
    update();
    emitViewportStatus();
}

void BaseAnnoDisplayWidget::clearTempPreview()
{
    m_tempPreview = BaseAnnoTempPreview{};
    update();
}

void BaseAnnoDisplayWidget::setTempPreview(const BaseAnnoTempPreview &preview)
{
    m_tempPreview = preview;
    update();
}

void BaseAnnoDisplayWidget::setAnnotations(const BaseAnnoAnnotationList &annotations)
{
    m_annotations = annotations;
    if (m_selectedAnnotationIndex >= m_annotations.size())
    {
        m_selectedAnnotationIndex = -1;
    }
    update();
    emitViewportStatus();
}

const BaseAnnoAnnotationList &BaseAnnoDisplayWidget::annotations() const noexcept
{
    return m_annotations;
}

void BaseAnnoDisplayWidget::clearAnnotations()
{
    if (m_annotations.isEmpty())
    {
        return;
    }

    m_annotations.clear();
    m_selectedAnnotationIndex = -1;
    update();
    emitViewportStatus();
}

bool BaseAnnoDisplayWidget::addAnnotation(const BaseAnnoAnnotation &annotation, QString *errorMessage)
{
    if (!validateAnnotation(annotation, errorMessage))
    {
        return false;
    }

    m_annotations.append(annotation);
    update();
    emitViewportStatus();
    return true;
}

bool BaseAnnoDisplayWidget::updateAnnotation(const int index,
                                             const BaseAnnoAnnotation &annotation,
                                             QString *errorMessage)
{
    if (index < 0 || index >= m_annotations.size())
    {
        const QString message = QString(u8"标注索引超出范围: %1").arg(index);
        qCritical().noquote() << message;
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    }
    if (!validateAnnotation(annotation, errorMessage))
    {
        return false;
    }

    m_annotations[index] = annotation;
    update();
    emit annotationChanged(index, annotation);
    return true;
}

bool BaseAnnoDisplayWidget::removeAnnotation(const int index, QString *errorMessage)
{
    if (index < 0 || index >= m_annotations.size())
    {
        const QString message = QString(u8"标注索引超出范围: %1").arg(index);
        qCritical().noquote() << message;
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    }

    m_annotations.removeAt(index);
    if (m_selectedAnnotationIndex == index)
    {
        m_selectedAnnotationIndex = -1;
    }
    else if (m_selectedAnnotationIndex > index)
    {
        --m_selectedAnnotationIndex;
    }
    update();
    emitViewportStatus();
    return true;
}

void BaseAnnoDisplayWidget::setPendingPromptRects(const QVector<QRectF> &rects)
{
    m_pendingPromptRects = rects;
    update();
}

void BaseAnnoDisplayWidget::clearPendingPromptRects()
{
    if (m_pendingPromptRects.isEmpty())
    {
        return;
    }

    m_pendingPromptRects.clear();
    update();
}

void BaseAnnoDisplayWidget::setSelectedAnnotationIndex(int index)
{
    if (index < -1 || index >= m_annotations.size())
    {
        m_selectedAnnotationIndex = -1;
    }
    else
    {
        m_selectedAnnotationIndex = index;
    }
    update();
}

void BaseAnnoDisplayWidget::setShowAnnotationLabels(bool show)
{
    if (m_showAnnotationLabels == show)
    {
        return;
    }

    m_showAnnotationLabels = show;
    update();
}

void BaseAnnoDisplayWidget::setPolygonDrawingEnabled(bool enabled)
{
    if (m_polygonDrawingEnabled == enabled)
    {
        return;
    }

    m_polygonDrawingEnabled = enabled;
    clearPolygonDraft();
}

void BaseAnnoDisplayWidget::setDrawingShape(const BaseAnnoShapeType shapeType)
{
    m_drawingShape = shapeType;
    setPolygonDrawingEnabled(shapeType == BaseAnnoShapeType::Polygon);
}

void BaseAnnoDisplayWidget::setAnnotationDrawingEnabled(const bool enabled)
{
    m_annotationDrawingEnabled = enabled;
}

void BaseAnnoDisplayWidget::setDefaultAnnotation(const QString &label, const int colorValue)
{
    m_defaultLabel = label;
    m_defaultColorValue = colorValue;
}

void BaseAnnoDisplayWidget::clearPolygonDraft()
{
    if (m_polygonDraftImage.isEmpty())
    {
        return;
    }

    m_polygonDraftImage.clear();
    update();
}

QPointF BaseAnnoDisplayWidget::mapImagePointToWidget(const QPointF &imagePoint) const
{
    return imageToWidget(imagePoint);
}

QString BaseAnnoDisplayWidget::viewportStatusText() const
{
    if (m_image.isNull())
    {
        return QString(u8"未加载图像");
    }

    return QString(u8"图像: %1 x %2 | 缩放: %3% | 标注: %4")
        .arg(m_image.width())
        .arg(m_image.height())
        .arg(qRound(m_zoomFactor * 100.0))
        .arg(m_annotations.size());
}

void BaseAnnoDisplayWidget::emitViewportStatus()
{
    emit viewportStatusChanged(viewportStatusText());
}

void BaseAnnoDisplayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawCanvasBackground(&painter);

    if (m_image.isNull())
    {
        drawEmptyState(&painter);
        return;
    }

    const QRect display = imageDisplayRect();
    const QRect shadowRect = display.adjusted(-8, -8, 8, 8);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(QRectF(shadowRect), 10, 10);
    painter.fillPath(shadowPath, QColor(0, 0, 0, 65));

    painter.setPen(QPen(QColor(63, 78, 102), 1.0));
    painter.setBrush(QColor(8, 11, 16));
    painter.drawRoundedRect(display.adjusted(-1, -1, 1, 1), 4, 4);
    painter.drawImage(display, m_image);

    for (int index = 0; index < m_annotations.size(); ++index)
    {
        const bool selected = index == m_selectedAnnotationIndex;
        const BaseAnnoAnnotation &annotation = m_annotations.at(index);
        const QColor baseColor = colorFromInt(annotation.colorValue);
        const QColor drawColor = selected ? QColor(255, 214, 74) : baseColor;
        const QPolygonF imagePolygon = polygonForAnnotation(annotation);
        const QPolygonF widgetPolygon = imageToWidgetPolygon(imagePolygon);

        painter.setPen(QPen(drawColor, selected ? 3.0 : 2.0));
        QColor fillColor = baseColor;
        fillColor.setAlpha(selected ? 58 : 30);
        painter.setBrush(annotation.shapeType == BaseAnnoShapeType::Point || annotation.shapeType == BaseAnnoShapeType::Line
                             ? Qt::NoBrush
                             : fillColor);
        if (annotation.shapeType == BaseAnnoShapeType::Circle && annotation.pointsImage.size() == 2)
        {
            const QPointF center = imageToWidget(annotation.pointsImage.at(0));
            const QPointF edge = imageToWidget(annotation.pointsImage.at(1));
            painter.drawEllipse(center, QLineF(center, edge).length(), QLineF(center, edge).length());
        }
        else if (annotation.shapeType == BaseAnnoShapeType::Point && !widgetPolygon.isEmpty())
        {
            painter.setBrush(drawColor);
            painter.drawEllipse(widgetPolygon.first(), 5.0, 5.0);
        }
        else if (annotation.shapeType == BaseAnnoShapeType::Line)
        {
            painter.drawPolyline(widgetPolygon);
        }
        else
        {
            painter.drawPolygon(widgetPolygon);
        }

        const QRectF box = widgetPolygon.boundingRect();
        if (selected)
        {
            painter.setPen(QPen(QColor(255, 214, 74, 210), 1.5));
            painter.setBrush(QColor(255, 214, 74));
            const QVector<QPointF> handles = {box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight()};
            for (const QPointF &point : handles)
            {
                painter.drawRoundedRect(QRectF(point.x() - 4.0, point.y() - 4.0, 8.0, 8.0), 2, 2);
            }
        }

        if (m_showAnnotationLabels)
        {
            const QString tag = annotation.caption.isEmpty()
                                    ? QStringLiteral("%1  %2").arg(index + 1).arg(annotation.label)
                                    : QStringLiteral("%1  %2").arg(index + 1).arg(annotation.caption);
            const QFontMetrics fontMetrics(painter.font());
            const QRect textBounds = fontMetrics.boundingRect(tag).adjusted(-10, -4, 10, 5);
            QPoint textPosition(static_cast<int>(qRound(box.left())), static_cast<int>(qRound(box.top())) - 5);
            if (textPosition.y() - textBounds.height() < 0)
            {
                textPosition.setY(static_cast<int>(qRound(box.top())) + textBounds.height() + 6);
            }
            if (textPosition.x() + textBounds.width() > width())
            {
                textPosition.setX(qMax(0, width() - textBounds.width() - 2));
            }

            const QRect backgroundRect(textPosition.x(),
                                       textPosition.y() - textBounds.height(),
                                       textBounds.width(),
                                       textBounds.height());
            drawRoundedLabel(&painter, backgroundRect, drawColor, tag);
        }
    }

    if (m_tempPreview.valid)
    {
        if (!m_tempPreview.contourImage.isEmpty())
        {
            painter.setPen(QPen(QColor(255, 96, 112), 2.0));
            painter.setBrush(QColor(255, 96, 112, 45));
            painter.drawPolygon(imageToWidgetPolygon(m_tempPreview.contourImage));
        }

        if (!m_tempPreview.minAreaRectImage.isEmpty())
        {
            painter.setPen(QPen(QColor(255, 215, 0), 2.0, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(imageToWidgetPolygon(m_tempPreview.minAreaRectImage));
        }
    }

    if (m_tempPreview.hasClick)
    {
        const QPointF point = imageToWidget(m_tempPreview.clickPointImage);
        painter.setPen(QPen(QColor(8, 11, 16), 2.0));
        painter.setBrush(QColor(98, 210, 162));
        painter.drawEllipse(point, 5.0, 5.0);
    }

    for (int index = 0; index < m_pendingPromptRects.size(); ++index)
    {
        const QRectF promptRect = m_pendingPromptRects.at(index).normalized();
        const QPointF topLeft = imageToWidget(promptRect.topLeft());
        const QPointF bottomRight = imageToWidget(promptRect.bottomRight());
        QRectF widgetRect(topLeft, bottomRight);
        widgetRect = widgetRect.normalized();
        painter.setPen(QPen(QColor(98, 210, 162), 2.0, Qt::DashLine));
        painter.setBrush(QColor(98, 210, 162, 28));
        painter.drawRoundedRect(widgetRect, 3, 3);

        const QString tag = QStringLiteral("R%1").arg(index + 1);
        const QFontMetrics fontMetrics(painter.font());
        const QRect textBounds = fontMetrics.boundingRect(tag).adjusted(-8, -3, 8, 4);
        const QPoint textPosition(static_cast<int>(qRound(widgetRect.left())),
                                  static_cast<int>(qRound(widgetRect.top())) - 4);
        const QRect backgroundRect(textPosition.x(),
                                   textPosition.y() - textBounds.height(),
                                   textBounds.width(),
                                   textBounds.height());
        drawRoundedLabel(&painter, backgroundRect, QColor(98, 210, 162), tag);
    }

    if (m_polygonDrawingEnabled && !m_polygonDraftImage.isEmpty())
    {
        const QPolygonF widgetDraft = imageToWidgetPolygon(m_polygonDraftImage);
        painter.setPen(QPen(QColor(98, 210, 162), 2.0, Qt::DashLine));
        painter.setBrush(QColor(98, 210, 162, 28));
        painter.drawPolyline(widgetDraft);

        if (m_polygonDraftImage.size() >= 2)
        {
            QPointF currentImagePoint;
            if (widgetToImage(m_currentWidgetPos, &currentImagePoint))
            {
                painter.drawLine(widgetDraft.last(), imageToWidget(currentImagePoint));
            }
        }

        painter.setPen(QPen(QColor(8, 11, 16), 2.0));
        painter.setBrush(QColor(98, 210, 162));
        for (const QPointF &point : widgetDraft)
        {
            painter.drawEllipse(point, 4.5, 4.5);
        }
    }

    if (m_draggingRect && m_mousePressed)
    {
        QRect dragRect(m_pressWidgetPos, m_currentWidgetPos);
        dragRect = dragRect.normalized();
        painter.setPen(QPen(QColor(77, 141, 255), 2.0, Qt::DashLine));
        painter.setBrush(QColor(77, 141, 255, 36));
        painter.drawRoundedRect(dragRect, 3, 3);
    }
    else if (m_tempPreview.hasRect && m_tempPreview.promptRectImage.isValid())
    {
        const QPointF topLeft = imageToWidget(m_tempPreview.promptRectImage.topLeft());
        const QPointF bottomRight = imageToWidget(m_tempPreview.promptRectImage.bottomRight());
        QRectF widgetRect(topLeft, bottomRight);
        widgetRect = widgetRect.normalized();
        painter.setPen(QPen(QColor(77, 141, 255), 2.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(widgetRect, 3, 3);
    }

    if (rect().contains(m_currentWidgetPos))
    {
        painter.setPen(QPen(QColor(143, 183, 255, 70), 1.0));
        painter.drawLine(QPoint(0, m_currentWidgetPos.y()), QPoint(width(), m_currentWidgetPos.y()));
        painter.drawLine(QPoint(m_currentWidgetPos.x(), 0), QPoint(m_currentWidgetPos.x(), height()));
    }

    drawHud(&painter, display);
}

void BaseAnnoDisplayWidget::drawEmptyState(QPainter *painter)
{
    const int centerY = height() / 2 - 20;

    painter->setPen(QColor(218, 226, 240));
    QFont titleFont = painter->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(QRect(24, centerY + 12, width() - 48, 30), Qt::AlignCenter, QString(u8"打开图像文件夹开始标注"));

    QFont hintFont = painter->font();
    hintFont.setPointSize(qMax(9, hintFont.pointSize() - 2));
    hintFont.setBold(false);
    painter->setFont(hintFont);
    painter->setPen(QColor(135, 147, 168));
    painter->drawText(QRect(24, centerY + 46, width() - 48, 48),
                      Qt::AlignCenter,
                      QString(u8"左键点击用于点提示, 拖拽可创建矩形标注, 滚轮缩放, 右键重置视图"));
}

void BaseAnnoDisplayWidget::drawCanvasBackground(QPainter *painter)
{
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor(13, 17, 24));
    background.setColorAt(1.0, QColor(9, 12, 18));
    painter->fillRect(rect(), background);

    painter->setPen(QPen(QColor(42, 51, 66, 85), 1.0));
    constexpr int gridSize = 32;
    for (int x = 0; x < width(); x += gridSize)
    {
        painter->drawLine(x, 0, x, height());
    }
    for (int y = 0; y < height(); y += gridSize)
    {
        painter->drawLine(0, y, width(), y);
    }
}

void BaseAnnoDisplayWidget::drawHud(QPainter *painter, const QRect &display)
{
    const QString hudText = viewportStatusText();
    const QFontMetrics fontMetrics(painter->font());
    const QRect textRect = fontMetrics.boundingRect(hudText).adjusted(-10, -5, 10, 6);
    const QRect hudRect(14, 14, qMin(width() - 28, textRect.width()), textRect.height());

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(10, 14, 20, 190));
    painter->drawRoundedRect(hudRect, 6, 6);
    painter->setPen(QColor(197, 210, 229));
    painter->drawText(hudRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, hudText);

    if (!display.isEmpty())
    {
        const QString sizeText = QStringLiteral("%1 x %2").arg(m_image.width()).arg(m_image.height());
        const QRect sizeBounds = fontMetrics.boundingRect(sizeText).adjusted(-10, -5, 10, 6);
        const QRect sizeRect(display.right() - sizeBounds.width() - 10,
                             display.bottom() - sizeBounds.height() - 10,
                             sizeBounds.width(),
                             sizeBounds.height());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(10, 14, 20, 175));
        painter->drawRoundedRect(sizeRect, 6, 6);
        painter->setPen(QColor(197, 210, 229));
        painter->drawText(sizeRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, sizeText);
    }
}

void BaseAnnoDisplayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && !m_image.isNull())
    {
        m_zoomFactor = 1.0;
        m_viewOffsetWidget = QPointF(0.0, 0.0);
        update();
        emitViewportStatus();
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || m_image.isNull())
    {
        QWidget::mousePressEvent(event);
        return;
    }

    QPointF imagePoint;
    if (!widgetToImage(event->position().toPoint(), &imagePoint))
    {
        QWidget::mousePressEvent(event);
        return;
    }

    setFocus(Qt::MouseFocusReason);

    if (m_polygonDrawingEnabled)
    {
        m_polygonDraftImage << imagePoint;
        m_mousePressed = false;
        m_draggingRect = false;
        m_currentWidgetPos = event->position().toPoint();
        update();
        event->accept();
        return;
    }

    m_mousePressed = true;
    m_draggingRect = false;
    m_pressWidgetPos = event->position().toPoint();
    m_currentWidgetPos = event->position().toPoint();
    m_pressImagePos = imagePoint;

    const int hitIndex = findAnnotationAtImagePoint(imagePoint);
    if (hitIndex != -1)
    {
        m_selectedAnnotationIndex = hitIndex;
        emit annotationSelectionChanged(hitIndex);
        update();
    }

    QWidget::mousePressEvent(event);
}

void BaseAnnoDisplayWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull())
    {
        QWidget::wheelEvent(event);
        return;
    }

    const QPoint delta = event->angleDelta();
    if (delta.y() == 0)
    {
        QWidget::wheelEvent(event);
        return;
    }

    QPointF anchorImage;
    QPoint anchorWidget = event->position().toPoint();
    if (!widgetToImage(anchorWidget, &anchorImage))
    {
        anchorImage = QPointF((m_image.width() - 1) * 0.5, (m_image.height() - 1) * 0.5);
        anchorWidget = rect().center();
    }

    const double steps = static_cast<double>(delta.y()) / 120.0;
    constexpr double scalePerStep = 1.15;
    m_zoomFactor *= qPow(scalePerStep, steps);
    m_zoomFactor = qBound(0.1, m_zoomFactor, 20.0);

    const QPointF anchorWidgetAfterZoom = imageToWidget(anchorImage);
    m_viewOffsetWidget += QPointF(anchorWidget) - anchorWidgetAfterZoom;

    update();
    emitViewportStatus();
    event->accept();
}

void BaseAnnoDisplayWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_currentWidgetPos = event->position().toPoint();

    if (!m_mousePressed || m_image.isNull())
    {
        update();
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int manhattanLength = (m_currentWidgetPos - m_pressWidgetPos).manhattanLength();
    if (manhattanLength >= 6)
    {
        m_draggingRect = true;
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void BaseAnnoDisplayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_mousePressed || m_image.isNull())
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    QPointF releaseImagePoint;
    const bool releaseValid = widgetToImage(event->position().toPoint(), &releaseImagePoint);
    const bool emitRect = m_draggingRect && releaseValid;

    m_mousePressed = false;
    m_draggingRect = false;

    if (emitRect)
    {
        const QRectF imageRect = normalizedImageRect(m_pressImagePos, releaseImagePoint);
        if (imageRect.width() >= 2.0 && imageRect.height() >= 2.0)
        {
            if (m_annotationDrawingEnabled)
            {
                QPolygonF points;
                if (m_drawingShape == BaseAnnoShapeType::Circle)
                {
                    points << m_pressImagePos << releaseImagePoint;
                }
                else
                {
                    points << imageRect.topLeft() << imageRect.topRight() << imageRect.bottomRight()
                           << imageRect.bottomLeft();
                }
                createAnnotationFromPoints(points);
            }
            else
            {
                emit rectPromptRequested(imageRect);
            }
        }
    }
    else
    {
        QPointF clickImagePoint;
        if (widgetToImage(event->position().toPoint(), &clickImagePoint))
        {
            if (m_annotationDrawingEnabled && m_drawingShape == BaseAnnoShapeType::Point)
            {
                createAnnotationFromPoints(QPolygonF{clickImagePoint});
            }
            else
            {
                emit pointPromptRequested(clickImagePoint);
            }
        }
    }

    update();
    QWidget::mouseReleaseEvent(event);
}

void BaseAnnoDisplayWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_polygonDrawingEnabled && !m_image.isNull())
    {
        finishPolygonDraft();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
}

void BaseAnnoDisplayWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_polygonDrawingEnabled && !m_polygonDraftImage.isEmpty())
    {
        clearPolygonDraft();
        emit polygonDraftRejected(QString(u8"已取消多边形草稿"));
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

QRect BaseAnnoDisplayWidget::imageDisplayRect() const
{
    if (m_image.isNull() || width() <= 0 || height() <= 0)
    {
        return {};
    }

    const QSize imageSize = m_image.size();
    const QSize areaSize = size();
    const double scaleX = static_cast<double>(areaSize.width()) / imageSize.width();
    const double scaleY = static_cast<double>(areaSize.height()) / imageSize.height();
    const double scale = qMin(scaleX, scaleY) * m_zoomFactor;

    const int drawWidth = qMax(1, static_cast<int>(qRound(imageSize.width() * scale)));
    const int drawHeight = qMax(1, static_cast<int>(qRound(imageSize.height() * scale)));
    const int x = static_cast<int>(qRound((areaSize.width() - drawWidth) / 2.0 + m_viewOffsetWidget.x()));
    const int y = static_cast<int>(qRound((areaSize.height() - drawHeight) / 2.0 + m_viewOffsetWidget.y()));
    return QRect(x, y, drawWidth, drawHeight);
}

bool BaseAnnoDisplayWidget::widgetToImage(const QPoint &widgetPoint, QPointF *imagePoint) const
{
    if (m_image.isNull() || imagePoint == nullptr)
    {
        return false;
    }

    const QRect display = imageDisplayRect();
    if (!display.contains(widgetPoint))
    {
        return false;
    }

    const double normalizedX = static_cast<double>(widgetPoint.x() - display.left()) / display.width();
    const double normalizedY = static_cast<double>(widgetPoint.y() - display.top()) / display.height();
    const double imageX = normalizedX * (m_image.width() - 1);
    const double imageY = normalizedY * (m_image.height() - 1);
    *imagePoint = QPointF(imageX, imageY);
    return true;
}

QPointF BaseAnnoDisplayWidget::imageToWidget(const QPointF &imagePoint) const
{
    const QRect display = imageDisplayRect();
    if (m_image.isNull() || display.isEmpty())
    {
        return {};
    }

    const double normalizedX = imagePoint.x() / qMax(1, m_image.width() - 1);
    const double normalizedY = imagePoint.y() / qMax(1, m_image.height() - 1);
    const double widgetX = display.left() + normalizedX * display.width();
    const double widgetY = display.top() + normalizedY * display.height();
    return QPointF(widgetX, widgetY);
}

QPolygonF BaseAnnoDisplayWidget::imageToWidgetPolygon(const QPolygonF &imagePolygon) const
{
    QPolygonF widgetPolygon;
    widgetPolygon.reserve(imagePolygon.size());
    for (const QPointF &imagePoint : imagePolygon)
    {
        widgetPolygon.push_back(imageToWidget(imagePoint));
    }
    return widgetPolygon;
}

int BaseAnnoDisplayWidget::findAnnotationAtImagePoint(const QPointF &imagePoint) const
{
    for (int index = m_annotations.size() - 1; index >= 0; --index)
    {
        const BaseAnnoAnnotation &annotation = m_annotations.at(index);
        const QPolygonF imagePolygon = polygonForAnnotation(annotation);
        if (annotation.shapeType == BaseAnnoShapeType::Point && !imagePolygon.isEmpty()
            && QLineF(imagePoint, imagePolygon.first()).length() <= 8.0)
        {
            return index;
        }
        if (annotation.shapeType == BaseAnnoShapeType::Line && imagePolygon.boundingRect().adjusted(-5, -5, 5, 5).contains(imagePoint))
        {
            return index;
        }
        if (imagePolygon.containsPoint(imagePoint, Qt::OddEvenFill))
        {
            return index;
        }
    }

    return -1;
}

QRectF BaseAnnoDisplayWidget::normalizedImageRect(const QPointF &firstPoint, const QPointF &secondPoint) const
{
    QRectF imageRect(firstPoint, secondPoint);
    imageRect = imageRect.normalized();

    const double maximumX = qMax(0, m_image.width() - 1);
    const double maximumY = qMax(0, m_image.height() - 1);
    imageRect.setLeft(qBound(0.0, imageRect.left(), maximumX));
    imageRect.setRight(qBound(0.0, imageRect.right(), maximumX));
    imageRect.setTop(qBound(0.0, imageRect.top(), maximumY));
    imageRect.setBottom(qBound(0.0, imageRect.bottom(), maximumY));
    return imageRect;
}

void BaseAnnoDisplayWidget::finishPolygonDraft()
{
    if (m_polygonDraftImage.size() < 3)
    {
        emit polygonDraftRejected(QString(u8"多边形至少需要 3 个点"));
        return;
    }

    const QPolygonF polygon = m_polygonDraftImage;
    m_polygonDraftImage.clear();
    update();
    if (m_annotationDrawingEnabled)
    {
        createAnnotationFromPoints(polygon);
    }
    else
    {
        emit polygonPromptRequested(polygon);
    }
}

QPolygonF BaseAnnoDisplayWidget::polygonForAnnotation(const BaseAnnoAnnotation &annotation) const
{
    if (annotation.shapeType == BaseAnnoShapeType::Circle && annotation.pointsImage.size() == 2)
    {
        const QPointF center = annotation.pointsImage.at(0);
        const double radius = QLineF(center, annotation.pointsImage.at(1)).length();
        QPolygonF polygon;
        constexpr int segments = 32;
        polygon.reserve(segments);
        for (int index = 0; index < segments; ++index)
        {
            constexpr double pi = 3.14159265358979323846;
            const double angle = 2.0 * pi * index / segments;
            polygon << QPointF(center.x() + radius * qCos(angle), center.y() + radius * qSin(angle));
        }
        return polygon;
    }
    return annotation.pointsImage;
}

bool BaseAnnoDisplayWidget::validateAnnotation(const BaseAnnoAnnotation &annotation, QString *errorMessage) const
{
    int minimumPoints = 4;
    switch (annotation.shapeType)
    {
    case BaseAnnoShapeType::Point:
        minimumPoints = 1;
        break;
    case BaseAnnoShapeType::Circle:
    case BaseAnnoShapeType::Line:
        minimumPoints = 2;
        break;
    case BaseAnnoShapeType::Polygon:
        minimumPoints = 3;
        break;
    case BaseAnnoShapeType::Rectangle:
    case BaseAnnoShapeType::RotatedRectangle:
        minimumPoints = 4;
        break;
    }
    if (annotation.label.trimmed().isEmpty() || annotation.pointsImage.size() < minimumPoints)
    {
        const QString message = QString(u8"标注标签或几何数据无效");
        qCritical().noquote() << message;
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    }
    return true;
}

void BaseAnnoDisplayWidget::createAnnotationFromPoints(const QPolygonF &points)
{
    BaseAnnoAnnotation annotation;
    annotation.shapeIndex = m_annotations.size();
    annotation.label = m_defaultLabel;
    annotation.colorValue = m_defaultColorValue;
    annotation.shapeType = m_drawingShape;
    annotation.pointsImage = points;
    QString errorMessage;
    if (!addAnnotation(annotation, &errorMessage))
    {
        emit polygonDraftRejected(errorMessage);
        return;
    }
    m_selectedAnnotationIndex = m_annotations.size() - 1;
    emit annotationCreated(annotation);
    emit annotationSelectionChanged(m_selectedAnnotationIndex);
}
