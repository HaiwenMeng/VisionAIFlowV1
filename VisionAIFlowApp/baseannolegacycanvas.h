#pragma once

#include "BaseAnnoDisplayWidget.h"
#include "ytyolodefine.h"

class QPaintEvent;

class BaseAnnoLegacyCanvas final : public BaseAnnoDisplayWidget
{
    Q_OBJECT

public:
    explicit BaseAnnoLegacyCanvas(QWidget *parent = nullptr);

    void toSetImage(const QImage &image);
    void toRemoveAllRoi();
    void toSetLabelNames(const QStringList &labels, const QVector<QColor> &colors);
    void toSetRoiDefaltType(int roiType);
    void toSetRoiDefaltName(const QString &label);
    void toSetHighlightedRoiKey(const QString &key);
    void addROI(int roiType, const QVector<double> &data, const QString &label);
    void toAppItemLabel(const QString &label,
                        int roiType,
                        const QVector<double> &data,
                        const QString &key = QString());
    void toRemoveRoiByKey(const QString &key);
    QVector<double> getROI(const QString &key) const;
    void toSetBGColor(const QColor &color);
    void ClearAllOverPlayPtr();
    void ClearAllStdOverPlayPtr();
    void addOverPlayPtr(YtSetShowtObj *overlay, const QString &name);
    void toUpdateShow();

signals:
    void SigGetDataUpdate(QString setKey, QString labelName, int type);
    void ROIChange(QVector<double> data, QString &key, int type);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    BaseAnnoShapeType toShapeType(int roiType) const;
    int toLegacyType(BaseAnnoShapeType shapeType) const;
    QPolygonF toPoints(int roiType, const QVector<double> &data) const;
    QVector<double> toData(const BaseAnnoAnnotation &annotation) const;
    int colorForLabel(const QString &label) const;
    void appendOverlayAnnotation(BaseAnnoShapeType shapeType,
                                 const QPolygonF &points,
                                 const QColor &color,
                                 const QString &name);
    void appendOverlay(YtSetShowtObj *overlay, const QString &name);
    void rebuildOverlayAnnotations();

    QStringList m_labels;
    QVector<QColor> m_colors;
    BaseAnnoAnnotationList m_overlayAnnotations;
    QVector<QPair<YtSetShowtObj *, QString>> m_overlaySources;
};
