#ifndef BASEANNODISPLAYWIDGET_H
#define BASEANNODISPLAYWIDGET_H

#include "BaseAnnoDispExport.h"
#include "BaseAnnoDisplayTypes.h"

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPainter;
class QWheelEvent;

class BASE_ANNODISP_EXPORT BaseAnnoDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BaseAnnoDisplayWidget(QWidget *parent = nullptr);

    bool loadImage(const QString &imagePath, QString *errorMessage = nullptr);
    bool setImage(const QImage &image, QString *errorMessage = nullptr);
    void clearImage();
    void clearTempPreview();
    void setTempPreview(const BaseAnnoTempPreview &preview);
    void setAnnotations(const BaseAnnoAnnotationList &annotations);
    const BaseAnnoAnnotationList &annotations() const noexcept;
    void clearAnnotations();
    bool addAnnotation(const BaseAnnoAnnotation &annotation, QString *errorMessage = nullptr);
    bool updateAnnotation(int index, const BaseAnnoAnnotation &annotation, QString *errorMessage = nullptr);
    bool removeAnnotation(int index, QString *errorMessage = nullptr);
    void setPendingPromptRects(const QVector<QRectF> &rects);
    void clearPendingPromptRects();
    void setSelectedAnnotationIndex(int index);
    void setShowAnnotationLabels(bool show);
    void setPolygonDrawingEnabled(bool enabled);
    void setDrawingShape(BaseAnnoShapeType shapeType);
    void setAnnotationDrawingEnabled(bool enabled);
    void setDefaultAnnotation(const QString &label, int colorValue);
    void clearPolygonDraft();
    QPointF mapImagePointToWidget(const QPointF &imagePoint) const;
    QString viewportStatusText() const;

signals:
    void pointPromptRequested(const QPointF &imagePoint);
    void rectPromptRequested(const QRectF &imageRect);
    void polygonPromptRequested(const QPolygonF &imagePolygon);
    void polygonDraftRejected(const QString &message);
    void annotationCreated(const BaseAnnoAnnotation &annotation);
    void annotationChanged(int annotationIndex, const BaseAnnoAnnotation &annotation);
    void annotationSelectionChanged(int annotationIndex);
    void viewportStatusChanged(const QString &statusText);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRect imageDisplayRect() const;
    void emitViewportStatus();
    void drawEmptyState(QPainter *painter);
    void drawCanvasBackground(QPainter *painter);
    void drawHud(QPainter *painter, const QRect &display);
    bool widgetToImage(const QPoint &widgetPoint, QPointF *imagePoint) const;
    QPointF imageToWidget(const QPointF &imagePoint) const;
    QPolygonF imageToWidgetPolygon(const QPolygonF &imagePolygon) const;
    int findAnnotationAtImagePoint(const QPointF &imagePoint) const;
    QRectF normalizedImageRect(const QPointF &firstPoint, const QPointF &secondPoint) const;
    QPolygonF polygonForAnnotation(const BaseAnnoAnnotation &annotation) const;
    bool validateAnnotation(const BaseAnnoAnnotation &annotation, QString *errorMessage) const;
    void createAnnotationFromPoints(const QPolygonF &points);
    void finishPolygonDraft();

    QImage m_image;
    BaseAnnoAnnotationList m_annotations;
    QVector<QRectF> m_pendingPromptRects;
    int m_selectedAnnotationIndex = -1;
    bool m_showAnnotationLabels = true;
    bool m_polygonDrawingEnabled = false;
    BaseAnnoShapeType m_drawingShape = BaseAnnoShapeType::Rectangle;
    bool m_annotationDrawingEnabled = false;
    QString m_defaultLabel;
    int m_defaultColorValue = 0x00C8FF;
    QPolygonF m_polygonDraftImage;
    BaseAnnoTempPreview m_tempPreview;

    bool m_mousePressed = false;
    bool m_draggingRect = false;
    QPoint m_pressWidgetPos;
    QPointF m_pressImagePos;
    QPoint m_currentWidgetPos;

    double m_zoomFactor = 1.0;
    QPointF m_viewOffsetWidget = QPointF(0.0, 0.0);
};

#endif // BASEANNODISPLAYWIDGET_H
