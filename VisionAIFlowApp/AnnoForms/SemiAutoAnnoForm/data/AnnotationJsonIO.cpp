#include "data/AnnotationJsonIO.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace {
QPolygonF toAxisAlignedRectPolygon(const QPolygonF& poly) {
    if (poly.isEmpty()) {
        return {};
    }

    qreal minX = poly.first().x();
    qreal minY = poly.first().y();
    qreal maxX = minX;
    qreal maxY = minY;
    for (const QPointF& p : poly) {
        minX = qMin(minX, p.x());
        minY = qMin(minY, p.y());
        maxX = qMax(maxX, p.x());
        maxY = qMax(maxY, p.y());
    }

    QPolygonF rect;
    rect << QPointF(minX, minY) << QPointF(maxX, minY) << QPointF(maxX, maxY) << QPointF(minX, maxY);
    return rect;
}

QPolygonF rawPolygonFromShapePoints(const QJsonArray& pointsArray) {
    QPolygonF poly;
    for (const QJsonValue& pointValue : pointsArray) {
        if (!pointValue.isArray()) {
            continue;
        }
        const QJsonArray p = pointValue.toArray();
        if (p.size() < 2) {
            continue;
        }
        poly << QPointF(p.at(0).toDouble(), p.at(1).toDouble());
    }
    return poly;
}

QPolygonF rectPolygonFromShapePoints(const QJsonArray& pointsArray) {
    return toAxisAlignedRectPolygon(rawPolygonFromShapePoints(pointsArray));
}

QJsonArray pointsArrayFromPolygon(const QPolygonF& polygon) {
    QJsonArray points;
    for (const QPointF& p : polygon) {
        QJsonArray point;
        point.append(p.x());
        point.append(p.y());
        points.append(point);
    }
    return points;
}

bool isValidAnnotationGeometry(const AnnotationObject& annotation) {
    const QRectF bounds = annotation.rectPolygonImage.boundingRect().normalized();
    if (!bounds.isValid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return false;
    }
    if (annotation.shapeType == AnnotationShapeType::Polygon) {
        return annotation.polygonImage.size() >= 3 && annotation.rectPolygonImage.size() == 4;
    }
    return annotation.rectPolygonImage.size() == 4;
}

QJsonObject createDefaultRoot(const QString& imagePath) {
    QFileInfo info(imagePath);
    QImageReader reader(imagePath);
    const QSize size = reader.size();

    QJsonObject root;
    root.insert(QStringLiteral("imageData"), QJsonValue::Null);
    root.insert(QStringLiteral("imageHeight"), size.height() > 0 ? size.height() : 0);
    root.insert(QStringLiteral("imageWidth"), size.width() > 0 ? size.width() : 0);
    root.insert(QStringLiteral("imagePath"), info.fileName());
    root.insert(QStringLiteral("shapes"), QJsonArray());
    return root;
}

void refreshImageFields(QJsonObject* root, const QString& imagePath) {
    if (root == nullptr) {
        return;
    }
    QFileInfo info(imagePath);
    QImageReader reader(imagePath);
    const QSize size = reader.size();
    root->insert(QStringLiteral("imageData"), QJsonValue::Null);
    root->insert(QStringLiteral("imageHeight"), size.height() > 0 ? size.height() : 0);
    root->insert(QStringLiteral("imageWidth"), size.width() > 0 ? size.width() : 0);
    root->insert(QStringLiteral("imagePath"), info.fileName());
}

QJsonObject shapeObjectFromAnnotation(const AnnotationObject& annotation) {
    QJsonObject obj;
    obj.insert(QStringLiteral("label"), annotation.label);
    if (annotation.shapeType == AnnotationShapeType::Polygon) {
        obj.insert(QStringLiteral("shape_type"), QStringLiteral("polygon"));
        obj.insert(QStringLiteral("points"), pointsArrayFromPolygon(annotation.polygonImage));
    } else {
        obj.insert(QStringLiteral("shape_type"), QStringLiteral("rectangle"));
        obj.insert(QStringLiteral("points"), pointsArrayFromPolygon(toAxisAlignedRectPolygon(annotation.rectPolygonImage)));
    }
    return obj;
}

bool loadRootObject(const QString& jsonPath, const QString& imagePath, QJsonObject* root, QString* errorMessage) {
    if (root == nullptr) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null root");
        }
        return false;
    }

    QFile file(jsonPath);
    if (!file.exists()) {
        *root = createDefaultRoot(imagePath);
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open json: %1").arg(jsonPath);
        }
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        *root = createDefaultRoot(imagePath);
        return true;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid json format: %1").arg(jsonPath);
        }
        return false;
    }

    *root = doc.object();
    if (!root->contains(QStringLiteral("shapes")) || !(*root)[QStringLiteral("shapes")].isArray()) {
        root->insert(QStringLiteral("shapes"), QJsonArray());
    }
    if (!root->contains(QStringLiteral("imageData"))) {
        root->insert(QStringLiteral("imageData"), QJsonValue::Null);
    }

    return true;
}

bool saveRootObject(const QString& jsonPath, const QJsonObject& root, QString* errorMessage) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write json: %1").arg(jsonPath);
        }
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
}

QString AnnotationJsonIO::jsonPathFromImagePath(const QString& imagePath) {
    QFileInfo info(imagePath);
    return info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".json");
}

QString AnnotationJsonIO::annotationFilePath(const QString& imagePath) {
    return jsonPathFromImagePath(imagePath);
}

bool AnnotationJsonIO::hasValidAnnotations(const QString& imagePath, bool* hasAnnotations, QString* errorMessage) {
    if (hasAnnotations == nullptr) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null hasAnnotations");
        }
        return false;
    }

    *hasAnnotations = false;
    const QString jsonPath = jsonPathFromImagePath(imagePath);
    if (!QFileInfo::exists(jsonPath)) {
        return true;
    }

    QList<AnnotationObject> annotations;
    if (!loadAnnotations(imagePath, &annotations, errorMessage)) {
        return false;
    }

    *hasAnnotations = !annotations.isEmpty();
    return true;
}

bool AnnotationJsonIO::removeAnnotationFile(const QString& imagePath, QString* errorMessage) {
    const QString jsonPath = jsonPathFromImagePath(imagePath);
    if (!QFileInfo::exists(jsonPath)) {
        return true;
    }

    if (!QFile::remove(jsonPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to delete json: %1").arg(jsonPath);
        }
        return false;
    }

    return true;
}

bool AnnotationJsonIO::annotationFileContainsLabel(const QString& imagePath, const QString& labelName, bool* containsLabel,
                                                   QString* errorMessage) {
    if (containsLabel == nullptr) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null containsLabel");
        }
        return false;
    }

    *containsLabel = false;
    if (labelName.isEmpty()) {
        return true;
    }

    const QString jsonPath = jsonPathFromImagePath(imagePath);
    if (!QFileInfo::exists(jsonPath)) {
        return true;
    }

    QList<AnnotationObject> annotations;
    if (!loadAnnotations(imagePath, &annotations, errorMessage)) {
        return false;
    }

    for (const AnnotationObject& annotation : annotations) {
        if (annotation.label == labelName) {
            *containsLabel = true;
            return true;
        }
    }

    return true;
}

bool AnnotationJsonIO::loadAnnotations(const QString& imagePath, QList<AnnotationObject>* annotations, QString* errorMessage) {
    if (annotations == nullptr) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Internal error: null annotations");
        }
        return false;
    }

    annotations->clear();
    const QString jsonPath = jsonPathFromImagePath(imagePath);

    QJsonObject root;
    if (!loadRootObject(jsonPath, imagePath, &root, errorMessage)) {
        return false;
    }

    const QJsonArray shapes = root.value(QStringLiteral("shapes")).toArray();
    for (int i = 0; i < shapes.size(); ++i) {
        if (!shapes.at(i).isObject()) {
            continue;
        }
        const QJsonObject obj = shapes.at(i).toObject();
        const QString shapeType = obj.value(QStringLiteral("shape_type")).toString();
        if (!shapeType.isEmpty() && shapeType != QStringLiteral("rectangle") && shapeType != QStringLiteral("polygon")) {
            qWarning().noquote() << "[AnnotationJsonIO] skip unsupported shape_type:" << shapeType;
            continue;
        }

        const QJsonArray pointsArray = obj.value(QStringLiteral("points")).toArray();
        const QPolygonF rawPoly = rawPolygonFromShapePoints(pointsArray);
        const QPolygonF rectPoly = toAxisAlignedRectPolygon(rawPoly);
        if (rectPoly.size() != 4) {
            continue;
        }
        if (shapeType == QStringLiteral("polygon") && rawPoly.size() < 3) {
            continue;
        }

        AnnotationObject ann;
        ann.shapeIndex = i;
        ann.label = obj.value(QStringLiteral("label")).toString();
        ann.shapeType = shapeType == QStringLiteral("polygon")
            ? AnnotationShapeType::Polygon
            : AnnotationShapeType::Rectangle;
        ann.rectPolygonImage = rectPoly;
        ann.polygonImage = ann.shapeType == AnnotationShapeType::Polygon ? rawPoly : rectPoly;
        annotations->push_back(ann);
    }

    return true;
}

bool AnnotationJsonIO::appendAnnotation(const QString& imagePath, const AnnotationObject& annotation, QString* errorMessage) {
    return appendAnnotations(imagePath, QList<AnnotationObject>{annotation}, errorMessage);
}

bool AnnotationJsonIO::appendAnnotations(const QString& imagePath, const QList<AnnotationObject>& annotations, QString* errorMessage) {
    if (annotations.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No annotations to append");
        }
        return false;
    }

    const QString jsonPath = jsonPathFromImagePath(imagePath);

    QJsonObject root;
    if (!loadRootObject(jsonPath, imagePath, &root, errorMessage)) {
        return false;
    }

    refreshImageFields(&root, imagePath);

    QJsonArray shapes = root.value(QStringLiteral("shapes")).toArray();
    for (const AnnotationObject& annotation : annotations) {
        if (annotation.label.isEmpty() || !isValidAnnotationGeometry(annotation)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Invalid annotation in batch append");
            }
            return false;
        }
        shapes.append(shapeObjectFromAnnotation(annotation));
    }

    root.insert(QStringLiteral("shapes"), shapes);
    return saveRootObject(jsonPath, root, errorMessage);
}

bool AnnotationJsonIO::replaceAnnotations(const QString& imagePath, const QList<AnnotationObject>& annotations,
                                          QString* errorMessage) {
    if (annotations.isEmpty()) {
        return removeAnnotationFile(imagePath, errorMessage);
    }

    const QString jsonPath = jsonPathFromImagePath(imagePath);

    QJsonObject root;
    if (!loadRootObject(jsonPath, imagePath, &root, errorMessage)) {
        return false;
    }

    refreshImageFields(&root, imagePath);

    QJsonArray shapes;
    for (const AnnotationObject& annotation : annotations) {
        if (annotation.label.isEmpty() || !isValidAnnotationGeometry(annotation)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Invalid annotation in replace");
            }
            return false;
        }
        shapes.append(shapeObjectFromAnnotation(annotation));
    }

    root.insert(QStringLiteral("shapes"), shapes);
    return saveRootObject(jsonPath, root, errorMessage);
}

bool AnnotationJsonIO::clearAnnotations(const QString& imagePath, QString* errorMessage) {
    return removeAnnotationFile(imagePath, errorMessage);
}

bool AnnotationJsonIO::removeAnnotationByIndex(const QString& imagePath, int shapeIndex, QString* errorMessage) {
    if (shapeIndex < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid shape index");
        }
        return false;
    }

    const QString jsonPath = jsonPathFromImagePath(imagePath);

    QJsonObject root;
    if (!loadRootObject(jsonPath, imagePath, &root, errorMessage)) {
        return false;
    }

    QJsonArray shapes = root.value(QStringLiteral("shapes")).toArray();
    if (shapeIndex >= shapes.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Shape index out of range");
        }
        return false;
    }

    shapes.removeAt(shapeIndex);
    if (shapes.isEmpty()) {
        return removeAnnotationFile(imagePath, errorMessage);
    }

    root.insert(QStringLiteral("shapes"), shapes);

    return saveRootObject(jsonPath, root, errorMessage);
}

bool AnnotationJsonIO::updateAnnotationByIndex(const QString& imagePath, int shapeIndex, const AnnotationObject& annotation,
                                               QString* errorMessage) {
    if (shapeIndex < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid shape index");
        }
        return false;
    }

    const QString jsonPath = jsonPathFromImagePath(imagePath);

    QJsonObject root;
    if (!loadRootObject(jsonPath, imagePath, &root, errorMessage)) {
        return false;
    }

    refreshImageFields(&root, imagePath);

    QJsonArray shapes = root.value(QStringLiteral("shapes")).toArray();
    if (shapeIndex >= shapes.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Shape index out of range");
        }
        return false;
    }

    shapes.replace(shapeIndex, shapeObjectFromAnnotation(annotation));
    root.insert(QStringLiteral("shapes"), shapes);
    return saveRootObject(jsonPath, root, errorMessage);
}
