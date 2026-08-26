#ifndef AUTOLABELPROJECT_APP_APPTYPES_H
#define AUTOLABELPROJECT_APP_APPTYPES_H

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringList>

enum class AnnotationShapeType {
    Rectangle,
    Polygon
};

struct TempInferenceResult {
    bool valid = false;

    QPointF clickPointImage;
    QRectF promptRectImage;
    bool hasClick = false;
    bool hasRect = false;

    QPolygonF contourImage;
    QPolygonF minAreaRectImage;
};

struct AnnotationObject {
    int shapeIndex = -1;
    QString label;
    int colorValue = 0x00C8FF;
    AnnotationShapeType shapeType = AnnotationShapeType::Rectangle;
    QPolygonF rectPolygonImage;
    QPolygonF polygonImage;
};

struct LabelConfig {
    QString infoSet;
    QStringList nameList;
    QList<int> colorDefine;
};

#endif // AUTOLABELPROJECT_APP_APPTYPES_H
