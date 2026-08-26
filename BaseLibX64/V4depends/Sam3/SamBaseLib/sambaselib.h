#ifndef SAMBASELIB_H
#define SAMBASELIB_H

#include "SamBaseLib_global.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

struct SAMBASELIB_EXPORT SamObjectResult
{
    bool success = false;
    QString errorMessage;
    QString labelName;
    float score = 0.0f;
    QVector<double> roiData;
    QVector<double> minRectRoiData;
    QVector<QPointF> maskContour;
};

struct SAMBASELIB_EXPORT SamInferResult
{
    bool success = false;
    QString errorMessage;
    QVector<SamObjectResult> objects;
};

class SAMBASELIB_EXPORT SamBase
{
public:
    virtual ~SamBase();

    virtual bool initialize(const QString &modelDir, int deviceId = 0) = 0;
    virtual void setCurrentImage(const QImage &image) = 0;
    virtual bool isInitialized() const = 0;
    virtual QString modelDir() const = 0;
    virtual QString displayName() const = 0;
    virtual SamInferResult inferByPoint(const QPointF &point, const QString &labelName) const = 0;
    virtual SamInferResult inferByRect(const QRectF &rect, const QString &labelName) const = 0;
    virtual SamInferResult inferByRects(const QVector<QRectF> &rects, const QString &labelName) const =0;
};

#endif // SAMBASELIB_H
