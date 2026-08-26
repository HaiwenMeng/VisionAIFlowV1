#ifndef SAM2LABELCANVASCOMPAT_H
#define SAM2LABELCANVASCOMPAT_H

#include <QColor>
#include <QImage>
#include <QMap>
#include <QMouseEvent>
#include <QPointF>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <QWheelEvent>

#include <memory>

#include "sambaselib.h"
#include "ytyolodefine.h"

class Sam2LabelCanvasCompat : public QWidget
{
    Q_OBJECT
public:
    explicit Sam2LabelCanvasCompat(QWidget *parent = nullptr);
    bool toInitSam2Model(const QString &modelDir);
    bool isSam2Ready() const;
    QString toGetSam2ModelDir() const;
    bool toInitSamModel(int samTypeIndex, const QString &modelDir);
    bool toSwitchSamType(int samTypeIndex);
    bool isSamReady() const;
    int samTypeIndex() const;
    QString toGetSamModelDir() const;
    void toSetManualBoxMode(bool enabled);

    template <typename T>
    void addOverPlayPtr(T *overPlay, const QString &name)
    {
        Q_UNUSED(overPlay);
        Q_UNUSED(name);
    }

    void toSetImage(const QImage &image);
    void toRemoveAllRoi();
    void toSetLabelNames(const QStringList &labelNames, const QVector<QColor> &labelColors);
    void toSetRoiDefaltType(int roiType);
    void toSetRoiDefaltName(const QString &labelName);
    QVector<double> getROI(const QString &key) const;
    void toAppItemLabel(const QString &labelName, int roiType, const QVector<double> &roiData);
    void toUpdateShow();
    void toRemoveRoiByKey(const QString &key);
    void toSetHighlightedRoiKey(const QString &key);

signals:
    void SigGetDataUpdate(QString SetKey, QString LableName, int type);
    void ROIChange(QVector<double> tdata, QString &key, int type);
    void SamRuntimeError(QString errorMessage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct RoiEntry
    {
        QString labelName;
        int roiType = 0;
        QVector<double> roiData;
        QVector<QPointF> maskContour;
    };

    QColor toFindLabelColor(const QString &labelName) const;
    QRectF toImageRectInWidget() const;
    QPointF toMapWidgetToImage(const QPoint &pt) const;
    void toRunPointPrompt(const QPoint &widgetPoint);
    void toRunRectPrompt(const QPoint &beginPt, const QPoint &endPt);
    void toCommitManualRect(const QRectF &rect);
    void toDrawRoiByImageData(QPainter &painter, const QVector<double> &roiData, const QColor &color,
                              const QRectF &imageRect, qreal scale, bool dashed, int penWidth = 2) const;
    void toDrawPolylineByImageData(QPainter &painter, const QVector<QPointF> &points, const QColor &color,
                                   const QRectF &imageRect, qreal scale, bool closed, int penWidth) const;
    void toDrawImageInfoBar(QPainter &painter) const;
    void toUpdateMouseImageInfo(const QPoint &widgetPoint);
    QString toImageInfoText() const;
    void toCommitInferObjects(const SamInferResult &result);
    void toClearCandidateRoi();
    void toReindexRoiStore();
    QString toResolveCommitLabel() const;
    void toResetViewTransform();
    qreal toComputeFitScale() const;
    void toClampViewCenter();

private:
    QImage m_currentImage;
    QStringList m_labelNames;
    QVector<QColor> m_labelColors;
    int m_defaultRoiType = 0;
    QString m_defaultLabelName;
    QMap<int, RoiEntry> m_roiStore;
    int m_nextRoiKey = 1;
    int m_highlightedRoiKey = -1;

    std::shared_ptr<SamBase> m_inferenceService;
    int m_samTypeIndex = 0;
    QVector<double> m_candidateRoi;
    QVector<double> m_candidateMinRectRoi;
    QVector<QPointF> m_candidateMaskContour;
    bool m_draggingRectPrompt = false;
    bool m_draggingView = false;
    QPoint m_dragStartPoint;
    QPoint m_dragCurrentPoint;
    QPoint m_viewDragStartPoint;
    QPointF m_viewDragStartCenter;
    bool m_manualBoxMode = false;
    bool m_hasMouseImagePos = false;
    QPointF m_lastMouseImagePos;

    qreal m_zoomFactor = 1.0;
    QPointF m_viewCenterImage;
};

typedef Sam2LabelCanvasCompat YtLabelRoiShow;

#ifndef LrectangleROI
static const int LrectangleROI = LabelSet::LrectangleROI;
static const int LrotaterectangleROI = LabelSet::LrotaterectangleROI;
static const int LcircleROI = LabelSet::LcircleROI;
static const int LpolygonROI = LabelSet::LpolygonROI;
static const int LpointROI = LabelSet::LpointROI;
static const int LlineSegROI = LabelSet::LlineSegROI;
#endif

#endif // SAM2LABELCANVASCOMPAT_H
