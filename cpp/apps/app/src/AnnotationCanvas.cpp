#include "visionaiflow/app/AnnotationCanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace visionaiflow::app
{
AnnotationCanvas::AnnotationCanvas(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
}

foundation::Result<void> AnnotationCanvas::SetImage(const QImage &image)
{
    if (image.isNull()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation canvas image must not be null"));
    m_image = image;
    m_transform = {};
    m_polygonPoints.clear();
    update();
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationCanvas::SetDocumentLocation(const QString &projectRoot, const QString &imageId)
{
    if (projectRoot.isEmpty() || QUuid(imageId).isNull()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation document location is invalid"));
    m_projectRoot = projectRoot;
    m_imageId = imageId;
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationCanvas::LoadAnnotations()
{
    if (m_projectRoot.isEmpty() || m_imageId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Annotation document location has not been configured"));
    const auto loaded = m_store.Load(m_projectRoot, m_imageId);
    if (!loaded.IsSuccess()) return foundation::Result<void>::Failure(loaded.Failure());
    const auto reset = m_document.Reset(loaded.Value());
    if (!reset.IsSuccess()) return reset;
    emit DocumentChanged();
    update();
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationCanvas::SaveAnnotations()
{
    if (m_projectRoot.isEmpty() || m_imageId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Annotation document location has not been configured"));
    const auto saved = m_store.Save(m_projectRoot, m_imageId, m_document.Annotations());
    if (saved.IsSuccess())
    {
        m_document.MarkSaved();
        emit DocumentChanged();
    }
    return saved;
}

foundation::Result<void> AnnotationCanvas::SetActiveLabel(const QString &labelId)
{
    if (labelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Active annotation label must not be empty"));
    m_activeLabelId = labelId;
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationCanvas::SetTool(const Tool tool)
{
    m_tool = tool;
    m_dragging = false;
    if (tool != Tool::Polygon) m_polygonPoints.clear();
    update();
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationCanvas::Undo()
{
    const auto result = m_document.Undo();
    if (result.IsSuccess()) { emit DocumentChanged(); update(); }
    return result;
}

foundation::Result<void> AnnotationCanvas::Redo()
{
    const auto result = m_document.Redo();
    if (result.IsSuccess()) { emit DocumentChanged(); update(); }
    return result;
}

const annotation::AnnotationDocument &AnnotationCanvas::Document() const noexcept { return m_document; }

foundation::Result<annotation::Point> AnnotationCanvas::ToImagePoint(const QPointF &viewportPoint) const
{
    if (m_image.isNull()) return foundation::Result<annotation::Point>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "An image must be loaded before annotation"));
    const auto mapped = annotation::ViewportToImage({viewportPoint.x(), viewportPoint.y()}, m_transform);
    if (!mapped.IsSuccess()) return mapped;
    return annotation::ClampPointToImage(mapped.Value(), {m_image.width(), m_image.height()});
}

void AnnotationCanvas::ReportFailure(const foundation::Error &error)
{
    emit OperationFailed(QString::fromStdString(error.message));
}

foundation::Result<void> AnnotationCanvas::CommitDragAnnotation(const annotation::Point &endPoint)
{
    if (m_activeLabelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Select an annotation label before drawing"));
    annotation::Annotation value;
    value.annotationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.labelId = m_activeLabelId;
    if (m_tool == Tool::BoundingBox)
    {
        value.kind = annotation::AnnotationKind::BoundingBox;
        value.boundingBox = {std::min(m_dragStart.x, endPoint.x), std::min(m_dragStart.y, endPoint.y), std::abs(m_dragStart.x - endPoint.x), std::abs(m_dragStart.y - endPoint.y)};
    }
    else if (m_tool == Tool::Line)
    {
        value.kind = annotation::AnnotationKind::Line;
        value.labelId.clear();
        value.line = {m_dragStart, endPoint};
    }
    else return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Current tool does not create drag annotations"));
    return m_document.Add(value);
}

foundation::Result<void> AnnotationCanvas::CompletePolygon()
{
    if (m_activeLabelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Select an annotation label before drawing"));
    annotation::Annotation value;
    value.annotationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.kind = annotation::AnnotationKind::Polygon;
    value.labelId = m_activeLabelId;
    value.polygon = m_polygonPoints;
    const auto result = m_document.Add(value);
    if (result.IsSuccess()) m_polygonPoints.clear();
    return result;
}

void AnnotationCanvas::mousePressEvent(QMouseEvent *event)
{
    const auto point = ToImagePoint(event->position());
    if (!point.IsSuccess()) { ReportFailure(point.Failure()); return; }
    if (event->button() == Qt::MiddleButton || (m_tool == Tool::Select && event->button() == Qt::LeftButton)) { m_dragging = true; m_dragStart = {event->position().x(), event->position().y()}; return; }
    if (event->button() != Qt::LeftButton) return;
    if (m_tool == Tool::Polygon) { m_polygonPoints.push_back(point.Value()); update(); return; }
    if (m_tool == Tool::BoundingBox || m_tool == Tool::Line) { m_dragging = true; m_dragStart = point.Value(); m_dragCurrent = point.Value(); update(); }
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    if (event->buttons().testFlag(Qt::MiddleButton) || m_tool == Tool::Select)
    {
        m_transform.pan.x += event->position().x() - m_dragStart.x;
        m_transform.pan.y += event->position().y() - m_dragStart.y;
        m_dragStart = {event->position().x(), event->position().y()};
    }
    else
    {
        const auto point = ToImagePoint(event->position());
        if (!point.IsSuccess()) { ReportFailure(point.Failure()); return; }
        m_dragCurrent = point.Value();
    }
    update();
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    const bool panning = event->button() == Qt::MiddleButton || m_tool == Tool::Select;
    m_dragging = false;
    if (panning) { update(); return; }
    const auto point = ToImagePoint(event->position());
    if (!point.IsSuccess()) { ReportFailure(point.Failure()); return; }
    const auto committed = CommitDragAnnotation(point.Value());
    if (!committed.IsSuccess()) ReportFailure(committed.Failure());
    else { emit DocumentChanged(); update(); }
}

void AnnotationCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_tool != Tool::Polygon || event->button() != Qt::LeftButton) return;
    const auto point = ToImagePoint(event->position());
    if (!point.IsSuccess()) { ReportFailure(point.Failure()); return; }
    if (!m_polygonPoints.empty() && m_polygonPoints.back().x == point.Value().x && m_polygonPoints.back().y == point.Value().y) m_polygonPoints.pop_back();
    const auto completed = CompletePolygon();
    if (!completed.IsSuccess()) ReportFailure(completed.Failure());
    else { emit DocumentChanged(); update(); }
}

void AnnotationCanvas::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull()) return;
    const double multiplier = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_transform.zoom = std::clamp(m_transform.zoom * multiplier, 0.05, 64.0);
    update();
}

void AnnotationCanvas::RenderAnnotation(QPainter &painter, const annotation::Annotation &annotation) const
{
    painter.setPen(QPen(Qt::green, 1.5 / m_transform.zoom));
    if (annotation.kind == annotation::AnnotationKind::BoundingBox) painter.drawRect(QRectF(annotation.boundingBox.x, annotation.boundingBox.y, annotation.boundingBox.width, annotation.boundingBox.height));
    else if (annotation.kind == annotation::AnnotationKind::Line) painter.drawLine(QPointF(annotation.line.first.x, annotation.line.first.y), QPointF(annotation.line.second.x, annotation.line.second.y));
    else if (annotation.kind == annotation::AnnotationKind::Polygon || annotation.kind == annotation::AnnotationKind::OcrQuadrilateral)
    {
        QPolygonF polygon;
        for (const annotation::Point &point : annotation.polygon) polygon.append(QPointF(point.x, point.y));
        painter.drawPolygon(polygon);
    }
}

void AnnotationCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    if (m_image.isNull()) return;
    painter.save();
    painter.translate(m_transform.pan.x, m_transform.pan.y);
    painter.scale(m_transform.zoom, m_transform.zoom);
    painter.drawImage(QPointF(0.0, 0.0), m_image);
    for (const annotation::Annotation &annotation : m_document.Annotations()) RenderAnnotation(painter, annotation);
    if (m_dragging && (m_tool == Tool::BoundingBox || m_tool == Tool::Line))
    {
        annotation::Annotation preview;
        preview.kind = m_tool == Tool::BoundingBox ? annotation::AnnotationKind::BoundingBox : annotation::AnnotationKind::Line;
        preview.boundingBox = {std::min(m_dragStart.x, m_dragCurrent.x), std::min(m_dragStart.y, m_dragCurrent.y), std::abs(m_dragStart.x - m_dragCurrent.x), std::abs(m_dragStart.y - m_dragCurrent.y)};
        preview.line = {m_dragStart, m_dragCurrent};
        painter.setPen(QPen(Qt::yellow, 1.5 / m_transform.zoom, Qt::DashLine));
        if (preview.kind == annotation::AnnotationKind::BoundingBox) painter.drawRect(QRectF(preview.boundingBox.x, preview.boundingBox.y, preview.boundingBox.width, preview.boundingBox.height));
        else painter.drawLine(QPointF(preview.line.first.x, preview.line.first.y), QPointF(preview.line.second.x, preview.line.second.y));
    }
    if (!m_polygonPoints.empty())
    {
        painter.setPen(QPen(Qt::yellow, 1.5 / m_transform.zoom, Qt::DashLine));
        QPolygonF polygon;
        for (const annotation::Point &point : m_polygonPoints) polygon.append(QPointF(point.x, point.y));
        painter.drawPolyline(polygon);
    }
    painter.restore();
}
}
