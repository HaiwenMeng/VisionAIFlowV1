#ifndef BASEANNODISPLAYTYPES_H
#define BASEANNODISPLAYTYPES_H

#include "BaseAnnoDispExport.h"

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>

enum class BaseAnnoShapeType
{
    Rectangle,
    RotatedRectangle,
    Circle,
    Polygon,
    Point,
    Line
};

struct BASE_ANNODISP_EXPORT BaseAnnoTempPreview
{
    bool valid = false;

    QPointF clickPointImage;
    QRectF promptRectImage;
    bool hasClick = false;
    bool hasRect = false;

    QPolygonF contourImage;
    QPolygonF minAreaRectImage;
};

struct BASE_ANNODISP_EXPORT BaseAnnoAnnotation
{
    int shapeIndex = -1;
    QString label;
    int colorValue = 0x00C8FF;
    BaseAnnoShapeType shapeType = BaseAnnoShapeType::Rectangle;
    QPolygonF pointsImage;
    QString caption;
};

using BaseAnnoAnnotationList = QList<BaseAnnoAnnotation>;

#endif // BASEANNODISPLAYTYPES_H
