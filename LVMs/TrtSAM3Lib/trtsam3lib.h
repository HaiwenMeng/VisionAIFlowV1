#ifndef TRTSAM3LIB_H
#define TRTSAM3LIB_H

#include "TrtSam3Lib_global.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <memory>

#include <QStringList>
#include <QVector>

struct TRTSAM3LIB_EXPORT TrtSam3ObjectResult
{
    bool success = false;
    QString errorMessage;
    QString labelName;
    float score = 0.0f;
    QVector<double> roiData;
    QVector<double> minRectRoiData;
    QVector<QPointF> maskContour;
};

struct TRTSAM3LIB_EXPORT TrtSam3InferResult
{
    bool success = false;
    QString errorMessage;
    QVector<TrtSam3ObjectResult> objects;
};

namespace cv
{
class Mat;
}

class InferBase;

class TRTSAM3LIB_EXPORT TrtSam3
{
public:
    TrtSam3();
    ~TrtSam3();

    bool initialize(const QString &modelDir, int deviceId = 0);
    void setCurrentImage(const QImage &image);
    bool isInitialized() const;
    QString modelDir() const;
    QString displayName() const;
    TrtSam3InferResult inferByPoint(const QPointF &point, const QString &labelName) const;
    TrtSam3InferResult inferByRect(const QRectF &rect, const QString &labelName) const;
    TrtSam3InferResult inferByRects(const QVector<QRectF> &rects, const QString &labelName) const;

private:
    bool isReady() const;
    QRectF toClampRect(const QRectF &rect) const;
    QString toResolveEnginePath(const QString &modelDir, const QStringList &candidates) const;
    QImage toImageForInference(const QImage &image) const;
    cv::Mat toQImageAsBgrMat(const QImage &image) const;
    QRectF toPointPromptRect(const QPointF &point) const;
    TrtSam3InferResult toInferByGeometryRect(const QRectF &rect, const QString &labelName) const;
    TrtSam3InferResult toInferByGeometryRects(const QVector<QRectF> &rects, const QString &labelName) const;

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
