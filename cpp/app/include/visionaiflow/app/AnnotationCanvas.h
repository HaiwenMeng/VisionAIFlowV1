#pragma once

#include "visionaiflow/annotation/AnnotationDocument.h"
#include "visionaiflow/annotation/AnnotationStore.h"

#include <QImage>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QPainter;
class QWheelEvent;

namespace visionaiflow::app
{
class AnnotationCanvas final : public QWidget
{
    Q_OBJECT

public:
    enum class Tool
    {
        Select,
        BoundingBox,
        Polygon,
        Line
    };

    explicit AnnotationCanvas(QWidget *parent = nullptr);
    foundation::Result<void> SetImage(const QImage &image);
    foundation::Result<void> SetDocumentLocation(const QString &projectRoot, const QString &imageId);
    foundation::Result<void> LoadAnnotations();
    foundation::Result<void> SaveAnnotations();
    foundation::Result<void> SetActiveLabel(const QString &labelId);
    foundation::Result<void> SetTool(Tool tool);
    foundation::Result<void> Undo();
    foundation::Result<void> Redo();
    const annotation::AnnotationDocument &Document() const noexcept;

signals:
    void DocumentChanged();
    void OperationFailed(const QString &errorMessage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    foundation::Result<annotation::Point> ToImagePoint(const QPointF &viewportPoint) const;
    foundation::Result<void> CommitDragAnnotation(const annotation::Point &endPoint);
    foundation::Result<void> CompletePolygon();
    void ReportFailure(const foundation::Error &error);
    void RenderAnnotation(QPainter &painter, const annotation::Annotation &annotation) const;

    QImage m_image;
    QString m_projectRoot;
    QString m_imageId;
    QString m_activeLabelId;
    annotation::AnnotationStore m_store;
    annotation::AnnotationDocument m_document;
    annotation::ViewportTransform m_transform;
    Tool m_tool{Tool::Select};
    bool m_dragging{false};
    annotation::Point m_dragStart;
    annotation::Point m_dragCurrent;
    std::vector<annotation::Point> m_polygonPoints;
};
}
