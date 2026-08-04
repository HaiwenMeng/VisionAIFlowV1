#include "visionaiflow/annotation/AnnotationStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <limits>

namespace visionaiflow::annotation
{
namespace
{
foundation::Result<void> ValidateExactKeys(const QJsonObject &object, const QStringList &requiredKeys, const QString &context)
{
    for (const QString &key : requiredKeys)
    {
        if (!object.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("%1 is missing required field: %2").arg(context, key).toStdString()));
    }
    for (const QString &key : object.keys())
    {
        if (!requiredKeys.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("%1 contains unsupported field: %2").arg(context, key).toStdString()));
    }
    return foundation::Result<void>::Success();
}

QString KindToString(const AnnotationKind kind)
{
    switch (kind)
    {
    case AnnotationKind::BoundingBox: return QStringLiteral("bounding_box");
    case AnnotationKind::Polygon: return QStringLiteral("polygon");
    case AnnotationKind::Line: return QStringLiteral("line");
    case AnnotationKind::Classification: return QStringLiteral("classification");
    case AnnotationKind::InstanceMask: return QStringLiteral("instance_mask");
    case AnnotationKind::SemanticMask: return QStringLiteral("semantic_mask");
    case AnnotationKind::AnomalyMask: return QStringLiteral("anomaly_mask");
    case AnnotationKind::OcrQuadrilateral: return QStringLiteral("ocr_quadrilateral");
    }
    return {};
}

foundation::Result<AnnotationKind> KindFromString(const QString &value)
{
    if (value == QStringLiteral("bounding_box")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::BoundingBox);
    if (value == QStringLiteral("polygon")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::Polygon);
    if (value == QStringLiteral("line")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::Line);
    if (value == QStringLiteral("classification")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::Classification);
    if (value == QStringLiteral("instance_mask")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::InstanceMask);
    if (value == QStringLiteral("semantic_mask")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::SemanticMask);
    if (value == QStringLiteral("anomaly_mask")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::AnomalyMask);
    if (value == QStringLiteral("ocr_quadrilateral")) return foundation::Result<AnnotationKind>::Success(AnnotationKind::OcrQuadrilateral);
    return foundation::Result<AnnotationKind>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation kind is invalid"));
}

QJsonObject PointToJson(const Point &point) { return {{QStringLiteral("x"), point.x}, {QStringLiteral("y"), point.y}}; }

foundation::Result<Point> PointFromJson(const QJsonValue &value)
{
    if (!value.isObject()) return foundation::Result<Point>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation point is not an object"));
    const QJsonObject object = value.toObject();
    const auto keys = ValidateExactKeys(object, {QStringLiteral("x"), QStringLiteral("y")}, QStringLiteral("Annotation point"));
    if (!keys.IsSuccess()) return foundation::Result<Point>::Failure(keys.Failure());
    if (!object.value(QStringLiteral("x")).isDouble() || !object.value(QStringLiteral("y")).isDouble()) return foundation::Result<Point>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation point coordinates must be numbers"));
    Point point{object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()};
    const auto validation = ValidatePoint(point);
    if (!validation.IsSuccess()) return foundation::Result<Point>::Failure(validation.Failure());
    return foundation::Result<Point>::Success(point);
}

QJsonObject MaskToJson(const RasterMask &mask)
{
    QJsonArray runs;
    for (const MaskRun &run : mask.runs) runs.append(QJsonObject{{QStringLiteral("value"), static_cast<qint64>(run.value)}, {QStringLiteral("length"), static_cast<qint64>(run.length)}});
    return {{QStringLiteral("width"), mask.width}, {QStringLiteral("height"), mask.height}, {QStringLiteral("runs"), runs}};
}

foundation::Result<RasterMask> MaskFromJson(const QJsonValue &value)
{
    if (!value.isObject()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask is not an object"));
    const QJsonObject object = value.toObject();
    const auto keys = ValidateExactKeys(object, {QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("runs")}, QStringLiteral("Annotation mask"));
    if (!keys.IsSuccess()) return foundation::Result<RasterMask>::Failure(keys.Failure());
    if (!object.value(QStringLiteral("width")).isDouble() || !object.value(QStringLiteral("height")).isDouble()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask dimensions must be numeric"));
    if (!object.value(QStringLiteral("runs")).isArray()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask runs are not an array"));
    RasterMask mask{object.value(QStringLiteral("width")).toInt(), object.value(QStringLiteral("height")).toInt(), {}};
    for (const QJsonValue &runValueJson : object.value(QStringLiteral("runs")).toArray())
    {
        if (!runValueJson.isObject()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask run is not an object"));
        const QJsonObject run = runValueJson.toObject();
        const auto runKeys = ValidateExactKeys(run, {QStringLiteral("value"), QStringLiteral("length")}, QStringLiteral("Annotation mask run"));
        if (!runKeys.IsSuccess()) return foundation::Result<RasterMask>::Failure(runKeys.Failure());
        if (!run.value(QStringLiteral("value")).isDouble() || !run.value(QStringLiteral("length")).isDouble()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask run values must be numeric"));
        const qint64 runValue = run.value(QStringLiteral("value")).toInteger(-1);
        const qint64 runLength = run.value(QStringLiteral("length")).toInteger(-1);
        if (runValue < 0 || runValue > std::numeric_limits<quint32>::max() || runLength <= 0 || runLength > std::numeric_limits<quint32>::max()) return foundation::Result<RasterMask>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation mask run values are invalid"));
        mask.runs.push_back({static_cast<quint32>(runValue), static_cast<quint32>(runLength)});
    }
    return foundation::Result<RasterMask>::Success(std::move(mask));
}

QJsonObject ToJson(const Annotation &annotation)
{
    QJsonArray polygon;
    for (const Point &point : annotation.polygon) polygon.append(PointToJson(point));
    return {{QStringLiteral("annotationId"), annotation.annotationId}, {QStringLiteral("kind"), KindToString(annotation.kind)}, {QStringLiteral("labelId"), annotation.labelId}, {QStringLiteral("boundingBox"), QJsonObject{{QStringLiteral("x"), annotation.boundingBox.x}, {QStringLiteral("y"), annotation.boundingBox.y}, {QStringLiteral("width"), annotation.boundingBox.width}, {QStringLiteral("height"), annotation.boundingBox.height}}}, {QStringLiteral("polygon"), polygon}, {QStringLiteral("line"), QJsonObject{{QStringLiteral("first"), PointToJson(annotation.line.first)}, {QStringLiteral("second"), PointToJson(annotation.line.second)}}}, {QStringLiteral("classification"), annotation.classification}, {QStringLiteral("mask"), MaskToJson(annotation.mask)}, {QStringLiteral("transcription"), annotation.transcription}, {QStringLiteral("dictionaryId"), annotation.dictionaryId}};
}

foundation::Result<Annotation> FromJson(const QJsonObject &object)
{
    const auto keys = ValidateExactKeys(object, {QStringLiteral("annotationId"), QStringLiteral("kind"), QStringLiteral("labelId"), QStringLiteral("boundingBox"), QStringLiteral("polygon"), QStringLiteral("line"), QStringLiteral("classification"), QStringLiteral("mask"), QStringLiteral("transcription"), QStringLiteral("dictionaryId")}, QStringLiteral("Annotation entry"));
    if (!keys.IsSuccess()) return foundation::Result<Annotation>::Failure(keys.Failure());
    for (const QString &stringField : {QStringLiteral("annotationId"), QStringLiteral("kind"), QStringLiteral("labelId"), QStringLiteral("classification"), QStringLiteral("transcription"), QStringLiteral("dictionaryId")})
    {
        if (!object.value(stringField).isString()) return foundation::Result<Annotation>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Annotation string field has invalid type: ").append(stringField).toStdString()));
    }
    if (!object.value(QStringLiteral("boundingBox")).isObject() || !object.value(QStringLiteral("line")).isObject() || !object.value(QStringLiteral("polygon")).isArray()) return foundation::Result<Annotation>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation geometry fields have invalid types"));
    const auto kind = KindFromString(object.value(QStringLiteral("kind")).toString());
    if (!kind.IsSuccess()) return foundation::Result<Annotation>::Failure(kind.Failure());
    const QJsonObject rect = object.value(QStringLiteral("boundingBox")).toObject();
    const auto rectKeys = ValidateExactKeys(rect, {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")}, QStringLiteral("Annotation boundingBox"));
    if (!rectKeys.IsSuccess()) return foundation::Result<Annotation>::Failure(rectKeys.Failure());
    for (const QString &rectField : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")})
    {
        if (!rect.value(rectField).isDouble()) return foundation::Result<Annotation>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Annotation boundingBox numeric field has invalid type: ").append(rectField).toStdString()));
    }
    const QJsonObject line = object.value(QStringLiteral("line")).toObject();
    const auto lineKeys = ValidateExactKeys(line, {QStringLiteral("first"), QStringLiteral("second")}, QStringLiteral("Annotation line"));
    if (!lineKeys.IsSuccess()) return foundation::Result<Annotation>::Failure(lineKeys.Failure());
    const auto mask = MaskFromJson(object.value(QStringLiteral("mask")));
    if (!mask.IsSuccess()) return foundation::Result<Annotation>::Failure(mask.Failure());
    std::vector<Point> polygon;
    for (const QJsonValue &value : object.value(QStringLiteral("polygon")).toArray())
    {
        const auto point = PointFromJson(value);
        if (!point.IsSuccess()) return foundation::Result<Annotation>::Failure(point.Failure());
        polygon.push_back(point.Value());
    }
    const auto first = PointFromJson(line.value(QStringLiteral("first")));
    if (!first.IsSuccess()) return foundation::Result<Annotation>::Failure(first.Failure());
    const auto second = PointFromJson(line.value(QStringLiteral("second")));
    if (!second.IsSuccess()) return foundation::Result<Annotation>::Failure(second.Failure());
    Annotation annotation{object.value(QStringLiteral("annotationId")).toString(), kind.Value(), object.value(QStringLiteral("labelId")).toString(), {rect.value(QStringLiteral("x")).toDouble(), rect.value(QStringLiteral("y")).toDouble(), rect.value(QStringLiteral("width")).toDouble(), rect.value(QStringLiteral("height")).toDouble()}, std::move(polygon), {first.Value(), second.Value()}, object.value(QStringLiteral("classification")).toString(), mask.Value(), object.value(QStringLiteral("transcription")).toString(), object.value(QStringLiteral("dictionaryId")).toString()};
    const auto validation = ValidateAnnotation(annotation);
    if (!validation.IsSuccess()) return foundation::Result<Annotation>::Failure(validation.Failure());
    return foundation::Result<Annotation>::Success(std::move(annotation));
}
}

foundation::Result<void> ValidateAnnotation(const Annotation &annotation)
{
    if (QUuid(annotation.annotationId).isNull()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation id must be a UUID"));
    if (annotation.kind != AnnotationKind::Line && annotation.kind != AnnotationKind::Classification && annotation.kind != AnnotationKind::AnomalyMask && annotation.labelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation label id must not be empty"));
    if (annotation.kind == AnnotationKind::BoundingBox) return ValidateRect(annotation.boundingBox);
    if (annotation.kind == AnnotationKind::Polygon) return ValidatePolygon(annotation.polygon);
    if (annotation.kind == AnnotationKind::Line)
    {
        const auto line = CanonicalizeLine(annotation.line);
        return line.IsSuccess() ? foundation::Result<void>::Success() : foundation::Result<void>::Failure(line.Failure());
    }
    if (annotation.kind == AnnotationKind::InstanceMask) return ValidateRasterMask(annotation.mask, true);
    if (annotation.kind == AnnotationKind::SemanticMask) return ValidateRasterMask(annotation.mask, false);
    if (annotation.kind == AnnotationKind::AnomalyMask) return ValidateRasterMask(annotation.mask, true);
    if (annotation.kind == AnnotationKind::OcrQuadrilateral)
    {
        if (annotation.transcription.isEmpty() || annotation.dictionaryId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "OCR annotation requires transcription and dictionary metadata"));
        if (annotation.polygon.size() != 4U) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "OCR annotation requires exactly four quadrilateral points"));
        return ValidatePolygon(annotation.polygon);
    }
    if (annotation.classification.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Classification annotation must not be empty"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateRasterMask(const RasterMask &mask, const bool binaryOnly)
{
    if (mask.width <= 0 || mask.height <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask dimensions must be positive"));
    if (mask.runs.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask must contain at least one run"));
    const quint64 expectedPixels = static_cast<quint64>(mask.width) * static_cast<quint64>(mask.height);
    quint64 pixelCount = 0;
    bool containsForeground = false;
    for (const MaskRun &run : mask.runs)
    {
        if (run.length == 0U || (binaryOnly && run.value > 1U)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask run is invalid"));
        pixelCount += run.length;
        if (pixelCount > expectedPixels) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask runs exceed declared dimensions"));
        containsForeground = containsForeground || run.value != 0U;
    }
    if (pixelCount != expectedPixels) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask runs do not cover declared dimensions"));
    if (!containsForeground) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Mask must contain foreground pixels"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationStore::Save(const QString &projectRoot, const QString &imageId, const std::vector<Annotation> &annotations) const
{
    if (QUuid(imageId).isNull()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Image id must be a UUID"));
    QJsonArray values;
    QSet<QString> annotationIds;
    for (const Annotation &annotation : annotations)
    {
        const auto validation = ValidateAnnotation(annotation);
        if (!validation.IsSuccess()) return validation;
        if (annotationIds.contains(annotation.annotationId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Annotation document contains duplicate annotation ids"));
        annotationIds.insert(annotation.annotationId);
        values.append(ToJson(annotation));
    }
    const QString directory = QDir(projectRoot).filePath(QStringLiteral("data/annotations"));
    if (!QDir(directory).exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project annotation directory does not exist"));
    QSaveFile file(QDir(directory).filePath(imageId + QStringLiteral(".json")));
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write annotations: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("imageId"), imageId}, {QStringLiteral("annotations"), values}}).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit annotations: ").append(file.errorString()).toStdString()));
    return foundation::Result<void>::Success();
}

foundation::Result<std::vector<Annotation>> AnnotationStore::Load(const QString &projectRoot, const QString &imageId) const
{
    if (QUuid(imageId).isNull()) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Image id must be a UUID"));
    QFile file(QDir(projectRoot).filePath(QStringLiteral("data/annotations/") + imageId + QStringLiteral(".json")));
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read annotations: ").append(file.errorString()).toStdString()));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation document is invalid"));
    const QJsonObject root = document.object();
    const auto rootKeys = ValidateExactKeys(root, {QStringLiteral("schemaVersion"), QStringLiteral("imageId"), QStringLiteral("annotations")}, QStringLiteral("Annotation document"));
    if (!rootKeys.IsSuccess()) return foundation::Result<std::vector<Annotation>>::Failure(rootKeys.Failure());
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1 || root.value(QStringLiteral("imageId")).toString() != imageId || !root.value(QStringLiteral("annotations")).isArray()) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation document schema is invalid"));
    std::vector<Annotation> annotations;
    QSet<QString> annotationIds;
    for (const QJsonValue &value : root.value(QStringLiteral("annotations")).toArray())
    {
        if (!value.isObject()) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation entry is not an object"));
        const auto annotation = FromJson(value.toObject());
        if (!annotation.IsSuccess()) return foundation::Result<std::vector<Annotation>>::Failure(annotation.Failure());
        if (annotationIds.contains(annotation.Value().annotationId)) return foundation::Result<std::vector<Annotation>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation document contains duplicate annotation ids"));
        annotationIds.insert(annotation.Value().annotationId);
        annotations.push_back(annotation.Value());
    }
    return foundation::Result<std::vector<Annotation>>::Success(std::move(annotations));
}
}
