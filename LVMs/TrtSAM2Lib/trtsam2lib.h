#ifndef TRTSAM2LIB_H
#define TRTSAM2LIB_H

#include "TrtSam2Lib_global.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <memory>

struct TRTSAM2LIB_EXPORT TrtSam2ObjectResult
{
    bool success = false;
    QString errorMessage;
    QString labelName;
    float score = 0.0f;
    QVector<double> roiData;
    QVector<double> minRectRoiData;
    QVector<QPointF> maskContour;
};

struct TRTSAM2LIB_EXPORT TrtSam2InferResult
{
    bool success = false;
    QString errorMessage;
    QVector<TrtSam2ObjectResult> objects;
};

namespace cv
{
class Mat;
}

namespace Sam2
{
class SAM;
}

class TRTSAM2LIB_EXPORT TrtSam2
{
public:
    TrtSam2();
    ~TrtSam2();

    bool initialize(const QString &modelDir, int deviceId = 0);
    void setCurrentImage(const QImage &image);
    bool isInitialized() const;
    QString modelDir() const;
    QString displayName() const;
    TrtSam2InferResult inferByPoint(const QPointF &point, const QString &labelName) const;
    TrtSam2InferResult inferByRect(const QRectF &rect, const QString &labelName) const;
    TrtSam2InferResult inferByRects(const QVector<QRectF> &rects, const QString &labelName) const;

private:
    bool isReady() const;
    QRectF toClampRect(const QRectF &rect) const;
    QString toResolveEncoderEnginePath(const QString &modelDir) const;
    QString toResolveDecoderEnginePath(const QString &modelDir) const;
    QImage toImageForInference(const QImage &image) const;
    cv::Mat toQImageAsBgrMat(const QImage &image) const;
    TrtSam2InferResult
    toInferByPrompt(const QPointF *pointPrompt, const QRectF *rectPrompt, const QString &labelName) const;

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
    int m_deviceId = 0;
};

#endif // TRTSAM2LIB_H
