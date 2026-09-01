#ifndef AUTOLABELPROJECT_INFERENCE_SAMINFERENCEBRIDGE_H
#define AUTOLABELPROJECT_INFERENCE_SAMINFERENCEBRIDGE_H

#include <memory>

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include "inference/SamTypes.h"

class TrtSam3;

class SamInferenceBridge
{
public:
    SamInferenceBridge();
    ~SamInferenceBridge();

    bool initialize(QString *errorMessage = nullptr);
    void release();
    bool isInitialized() const;

    bool setCurrentImage(const QString &imagePath, QString *errorMessage = nullptr);

    TrtSam3InferResult inferByPoint(const QPointF &imagePoint, const QString &labelName);
    TrtSam3InferResult inferByRect(const QRectF &imageRect, const QString &labelName);
    TrtSam3InferResult inferByRects(const QVector<QRectF> &imageRects, const QString &labelName);
    TrtSam3InferResult inferSmallTargetByRect(const QRectF &imageRect, const QString &labelName);

private:
    bool validateReady(TrtSam3InferResult *result) const;
    bool restoreCurrentImage(QString *errorMessage = nullptr);

    bool m_initialized = false;
    QString m_currentImagePath;
    QImage m_currentImage;
    std::unique_ptr<TrtSam3> m_sam3;
};

#endif // AUTOLABELPROJECT_INFERENCE_SAMINFERENCEBRIDGE_H
