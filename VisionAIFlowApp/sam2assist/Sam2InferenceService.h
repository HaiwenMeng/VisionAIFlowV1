#ifndef SAM2INFERENCESERVICE_H
#define SAM2INFERENCESERVICE_H

#include <memory>

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "Sam2Types.h"

namespace cv
{
class Mat;
}

namespace Sam2
{
class SAM;
}

class Sam2InferenceService
{
public:
    bool initialize(const QString &modelDir);
    void setCurrentImage(const QImage &image);
    bool isInitialized() const;
    QString modelDir() const;

    Sam2InferResult inferByPoint(const QPointF &point) const;
    Sam2InferResult inferByRect(const QRectF &rect) const;

private:
    bool isReady() const;
    QRectF toClampRect(const QRectF &rect) const;
    QString toResolveEncoderEnginePath(const QString &modelDir) const;
    QString toResolveDecoderEnginePath(const QString &modelDir) const;
    QImage toImageForInference(const QImage &image) const;
    cv::Mat toQImageAsBgrMat(const QImage &image) const;
    Sam2InferResult toInferByPrompt(const QPointF *pointPrompt, const QRectF *rectPrompt) const;

    static QVector<double> toRectToRoiData(const QRectF &rect);
    static cv::Mat toMaskBinaryU8(const cv::Mat &mask);
    static QRectF toBoundingRectFromMask(const cv::Mat &mask);
    static QVector<QPointF> toLargestContour(const cv::Mat &mask);
    static QVector<double> toAxisAlignedRectRoiData(const QVector<QPointF> &contour);

private:
    std::shared_ptr<Sam2::SAM> m_sam;
    QString m_modelDir;
    QImage m_currentImage;
    std::shared_ptr<cv::Mat> m_currentImageBgr;
    bool m_initialized = false;
};

#endif // SAM2INFERENCESERVICE_H
