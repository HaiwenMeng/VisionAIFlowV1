#include "baseannolegacycanvas.h"

#include <QDebug>
#include <QPainter>

BaseAnnoLegacyCanvas::BaseAnnoLegacyCanvas(QWidget *parent) : BaseAnnoDisplayWidget(parent)
{
    connect(this,
            &BaseAnnoDisplayWidget::annotationCreated,
            this,
            [this](const BaseAnnoAnnotation &annotation)
            {
                emit SigGetDataUpdate(QString::number(annotation.shapeIndex + 1),
                                      annotation.label,
                                      toLegacyType(annotation.shapeType));
            });
}

void BaseAnnoLegacyCanvas::toSetImage(const QImage &image)
{
    if (image.isNull())
    {
        clearImage();
        return;
    }

    QString errorMessage;
    if (!setImage(image, &errorMessage))
    {
        qCritical().noquote() << errorMessage;
    }
}

void BaseAnnoLegacyCanvas::toRemoveAllRoi()
{
    clearAnnotations();
}

void BaseAnnoLegacyCanvas::toSetLabelNames(const QStringList &labels, const QVector<QColor> &colors)
{
    m_labels = labels;
    m_colors = colors;
}

void BaseAnnoLegacyCanvas::toSetRoiDefaltType(const int roiType)
{
    setDrawingShape(toShapeType(roiType));
    setAnnotationDrawingEnabled(true);
}

void BaseAnnoLegacyCanvas::toSetRoiDefaltName(const QString &label)
{
    setDefaultAnnotation(label, colorForLabel(label));
}

void BaseAnnoLegacyCanvas::toSetHighlightedRoiKey(const QString &key)
{
    setSelectedAnnotationIndex(key.isEmpty() ? -1 : key.toInt() - 1);
}

void BaseAnnoLegacyCanvas::addROI(const int roiType, const QVector<double> &data, const QString &label)
{
    toAppItemLabel(label, roiType, data);
}

void BaseAnnoLegacyCanvas::toAppItemLabel(const QString &label,
                                          const int roiType,
                                          const QVector<double> &data,
                                          const QString &key)
{
    BaseAnnoAnnotation annotation;
    annotation.shapeIndex = key.isEmpty() ? annotations().size() : key.toInt() - 1;
    annotation.label = label;
    annotation.colorValue = colorForLabel(label);
    annotation.shapeType = toShapeType(roiType);
    annotation.pointsImage = toPoints(roiType, data);
    QString errorMessage;
    if (!addAnnotation(annotation, &errorMessage))
    {
        qCritical().noquote() << errorMessage;
    }
}

void BaseAnnoLegacyCanvas::toRemoveRoiByKey(const QString &key)
{
    QString errorMessage;
    if (!removeAnnotation(key.toInt() - 1, &errorMessage))
    {
        qCritical().noquote() << errorMessage;
    }
}

QVector<double> BaseAnnoLegacyCanvas::getROI(const QString &key) const
{
    const int index = key.isEmpty() ? 0 : key.toInt() - 1;
    if (index < 0 || index >= annotations().size())
    {
        return {};
    }
    return toData(annotations().at(index));
}

void BaseAnnoLegacyCanvas::toSetBGColor(const QColor &color)
{
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, color);
    setPalette(palette);
}

void BaseAnnoLegacyCanvas::ClearAllOverPlayPtr()
{
    m_overlayAnnotations.clear();
    m_overlayTexts.clear();
    m_overlaySources.clear();
    update();
}

void BaseAnnoLegacyCanvas::ClearAllStdOverPlayPtr()
{
    m_overlayAnnotations.clear();
    m_overlayTexts.clear();
    m_overlaySources.clear();
    update();
}

void BaseAnnoLegacyCanvas::addOverPlayPtr(YtSetShowtObj *overlay, const QString &name)
{
    if (overlay == nullptr)
    {
        qCritical() << "添加显示结果失败: 结果图层为空";
        return;
    }

    for (QPair<YtSetShowtObj *, QString> &source : m_overlaySources)
    {
        if (source.first == overlay)
        {
            source.second = name;
            rebuildOverlayAnnotations();
            return;
        }
    }

    m_overlaySources.append(qMakePair(overlay, name));
    rebuildOverlayAnnotations();
}

void BaseAnnoLegacyCanvas::appendOverlay(YtSetShowtObj *overlay, const QString &name)
{
    const QString geometryCaption = overlay->m_DispTxt.isEmpty() ? name : QString();
    m_overlayTexts.append(overlay->m_DispTxt);

    for (const DispRects &group : overlay->m_DispRects)
    {
        for (const CMvRect &rect : group.ShowDispRects)
        {
            QPolygonF points;
            points << QPointF(rect.LeftTop.x, rect.LeftTop.y) << QPointF(rect.LeftTop.x + rect.cx, rect.LeftTop.y)
                   << QPointF(rect.LeftTop.x + rect.cx, rect.LeftTop.y + rect.cy)
                   << QPointF(rect.LeftTop.x, rect.LeftTop.y + rect.cy);
            appendOverlayAnnotation(BaseAnnoShapeType::Rectangle, points, group.clrLine, geometryCaption);
        }
    }

    for (const DispCircles &group : overlay->m_DispCircles)
    {
        for (const CMvCircle &circle : group.ShowCircles)
        {
            QPolygonF points;
            points << QPointF(circle.center.x, circle.center.y)
                   << QPointF(circle.center.x + circle.radius, circle.center.y);
            appendOverlayAnnotation(BaseAnnoShapeType::Circle, points, group.clrLine, geometryCaption);
        }
    }

    for (const DispRotatedRects &group : overlay->m_DispRotatedRects)
    {
        for (const CMvRotatedRect &rect : group.ShowRotatedRects)
        {
            CMvRotatedRect editableRect = rect;
            CMvPoint corners[4];
            editableRect.toGetPoint(corners);
            QPolygonF points;
            for (const CMvPoint &corner : corners)
            {
                points << QPointF(corner.x, corner.y);
            }
            appendOverlayAnnotation(BaseAnnoShapeType::RotatedRectangle, points, group.clrLine, geometryCaption);
        }
    }

    for (const DispPolygons &group : overlay->m_DispPolygons)
    {
        for (const CMvPolygon &polygon : group.ShowPolygons)
        {
            QPolygonF points;
            for (const CMvPoint &point : polygon.points)
            {
                points << QPointF(point.x, point.y);
            }
            appendOverlayAnnotation(BaseAnnoShapeType::Polygon, points, group.clrLine, geometryCaption);
        }
    }

    for (const DispLineSegs &group : overlay->m_DispLineSegs)
    {
        for (const CMvLineSeg &line : group.ShowLineSeg)
        {
            QPolygonF points;
            points << QPointF(line.st.x, line.st.y) << QPointF(line.ed.x, line.ed.y);
            appendOverlayAnnotation(BaseAnnoShapeType::Line, points, group.clrLine, geometryCaption);
        }
    }

    update();
}

void BaseAnnoLegacyCanvas::toUpdateShow()
{
    rebuildOverlayAnnotations();
    update();
}

void BaseAnnoLegacyCanvas::rebuildOverlayAnnotations()
{
    m_overlayAnnotations.clear();
    m_overlayTexts.clear();
    for (const QPair<YtSetShowtObj *, QString> &source : m_overlaySources)
    {
        if (source.first == nullptr)
        {
            qCritical() << "刷新显示结果失败: 结果图层为空";
            continue;
        }
        appendOverlay(source.first, source.second);
    }
}

void BaseAnnoLegacyCanvas::paintEvent(QPaintEvent *event)
{
    BaseAnnoDisplayWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const BaseAnnoAnnotation &annotation : m_overlayAnnotations)
    {
        const QColor color = QColor::fromRgb(annotation.colorValue & 0x00FFFFFF);
        QPolygonF points;
        for (const QPointF &point : annotation.pointsImage)
        {
            points << mapImagePointToWidget(point);
        }

        painter.setPen(QPen(color, 2.0));
        painter.setBrush(Qt::NoBrush);
        if (annotation.shapeType == BaseAnnoShapeType::Circle && points.size() == 2)
        {
            const double radius = QLineF(points.at(0), points.at(1)).length();
            painter.drawEllipse(points.first(), radius, radius);
        }
        else if (annotation.shapeType == BaseAnnoShapeType::Line)
        {
            painter.drawPolyline(points);
        }
        else
        {
            painter.drawPolygon(points);
        }

        if (!annotation.caption.isEmpty() && !points.isEmpty())
        {
            painter.setPen(color);
            painter.drawText(points.boundingRect().topLeft() + QPointF(3.0, -3.0), annotation.caption);
        }
    }

    for (const DispTxt &text : m_overlayTexts)
    {
        painter.setFont(text.FtTxt);
        painter.setPen(text.clrTxt);
        const QPointF textPosition = mapImagePointToWidget(QPointF(text.Position.x, text.Position.y));
        painter.drawText(textPosition + QPointF(0.0, painter.fontMetrics().ascent()), text.showText);
    }
}

BaseAnnoShapeType BaseAnnoLegacyCanvas::toShapeType(const int roiType) const
{
    switch (roiType)
    {
    case LabelSet::LrotaterectangleROI:
        return BaseAnnoShapeType::RotatedRectangle;
    case LabelSet::LcircleROI:
        return BaseAnnoShapeType::Circle;
    case LabelSet::LpolygonROI:
        return BaseAnnoShapeType::Polygon;
    case LabelSet::LpointROI:
        return BaseAnnoShapeType::Point;
    case LabelSet::LlineSegROI:
        return BaseAnnoShapeType::Line;
    default:
        return BaseAnnoShapeType::Rectangle;
    }
}

int BaseAnnoLegacyCanvas::toLegacyType(const BaseAnnoShapeType shapeType) const
{
    switch (shapeType)
    {
    case BaseAnnoShapeType::RotatedRectangle:
        return LabelSet::LrotaterectangleROI;
    case BaseAnnoShapeType::Circle:
        return LabelSet::LcircleROI;
    case BaseAnnoShapeType::Polygon:
        return LabelSet::LpolygonROI;
    case BaseAnnoShapeType::Point:
        return LabelSet::LpointROI;
    case BaseAnnoShapeType::Line:
        return LabelSet::LlineSegROI;
    case BaseAnnoShapeType::Rectangle:
        return LabelSet::LrectangleROI;
    }
    return LabelSet::LrectangleROI;
}

QPolygonF BaseAnnoLegacyCanvas::toPoints(const int roiType, const QVector<double> &data) const
{
    LabelSet label;
    label.toInitProData(QStringLiteral("legacy"), roiType, data);
    QPolygonF points;
    for (const QPointF &point : label.toGetPoints())
    {
        points.append(point);
    }
    return points;
}

QVector<double> BaseAnnoLegacyCanvas::toData(const BaseAnnoAnnotation &annotation) const
{
    LabelSet label;
    if (!label.toInitData(annotation.label,
                          annotation.shapeType == BaseAnnoShapeType::RotatedRectangle ? QStringLiteral("rotation")
                          : annotation.shapeType == BaseAnnoShapeType::Circle         ? QStringLiteral("circle")
                          : annotation.shapeType == BaseAnnoShapeType::Polygon        ? QStringLiteral("polygon")
                          : annotation.shapeType == BaseAnnoShapeType::Point          ? QStringLiteral("point")
                          : annotation.shapeType == BaseAnnoShapeType::Line           ? QStringLiteral("line")
                                                                                      : QStringLiteral("rectangle"),
                          annotation.pointsImage))
    {
        return {};
    }
    return label.toGetRoiData();
}

int BaseAnnoLegacyCanvas::colorForLabel(const QString &label) const
{
    const int index = m_labels.indexOf(label);
    if (index >= 0 && index < m_colors.size())
    {
        return static_cast<int>(m_colors.at(index).rgb());
    }
    return 0x00C8FF;
}

void BaseAnnoLegacyCanvas::appendOverlayAnnotation(const BaseAnnoShapeType shapeType,
                                                   const QPolygonF &points,
                                                   const QColor &color,
                                                   const QString &name)
{
    if (points.isEmpty())
    {
        qCritical() << "添加显示结果失败: 图层几何数据为空";
        return;
    }

    BaseAnnoAnnotation annotation;
    annotation.shapeIndex = m_overlayAnnotations.size();
    annotation.shapeType = shapeType;
    annotation.pointsImage = points;
    annotation.colorValue = static_cast<int>(color.rgb());
    annotation.caption = name;
    m_overlayAnnotations.append(annotation);
}
