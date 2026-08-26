#ifndef SAM2TYPES_H
#define SAM2TYPES_H

#include <QPointF>
#include <QString>
#include <QVector>

struct Sam2InferResult
{
    bool success = false;
    QString errorMessage;

    // Axis-aligned rectangle used by current label persistence pipeline.
    QVector<double> roiData;

    // Polygon contour of best mask in image coordinates.
    QVector<QPointF> maskContour;

    // Min-area enclosing rectangle (4 points, x1,y1,...,x4,y4) in image coordinates.
    QVector<double> minRectRoiData;
};

#endif // SAM2TYPES_H
