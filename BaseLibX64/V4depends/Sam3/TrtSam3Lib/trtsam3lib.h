#ifndef TRTSAM3LIB_H
#define TRTSAM3LIB_H

#include "TrtSam3Lib_global.h"

#include <memory>

#include "../SamBaseLib/sambaselib.h"

#include <QStringList>

namespace cv
{
class Mat;
}

class InferBase;

class TRTSAM3LIB_EXPORT TrtSam3 : public SamBase
{
public:
    TrtSam3();
    ~TrtSam3() override;

    bool initialize(const QString &modelDir, int deviceId = 0) override;
    void setCurrentImage(const QImage &image) override;
    bool isInitialized() const override;
    QString modelDir() const override;
    QString displayName() const override;
    SamInferResult inferByPoint(const QPointF &point, const QString &labelName) const override;
    SamInferResult inferByRect(const QRectF &rect, const QString &labelName) const override;
    SamInferResult inferByRects(const QVector<QRectF> &rects, const QString &labelName) const;

private:
    bool isReady() const;
    QRectF toClampRect(const QRectF &rect) const;
    QString toResolveEnginePath(const QString &modelDir, const QStringList &candidates) const;
    QImage toImageForInference(const QImage &image) const;
    cv::Mat toQImageAsBgrMat(const QImage &image) const;
    QRectF toPointPromptRect(const QPointF &point) const;
    SamInferResult toInferByGeometryRect(const QRectF &rect, const QString &labelName) const;
    SamInferResult toInferByGeometryRects(const QVector<QRectF> &rects, const QString &labelName) const;

    static QVector<double> toRectToRoiData(const QRectF &rect);
    static cv::Mat toMaskBinaryU8(const cv::Mat &mask);
    static QVector<QPointF> toLargestContour(const cv::Mat &mask);
    static QVector<double> toAxisAlignedRectRoiData(const QVector<QPointF> &contour);

private:
    std::shared_ptr<InferBase> m_engine;
    QString m_modelDir;
    QImage m_currentImage;
    std::shared_ptr<cv::Mat> m_currentImageBgr;
    bool m_initialized = false;
    int m_deviceId = 0;
    float m_confidenceThreshold = 0.5f;
};

#endif // TRTSAM3LIB_H
