#include "visionaiflow/project_store/LabelStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUuid>

namespace visionaiflow::project_store
{
namespace
{
foundation::Result<void> ValidateExactKeys(const QJsonObject &object, const QStringList &requiredKeys, const QString &context)
{
    for (const QString &key : requiredKeys)
    {
        if (!object.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("%1 is missing required field: %2").arg(context, key).toStdString()));
    }
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator)
    {
        if (!requiredKeys.contains(iterator.key())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("%1 contains unsupported field: %2").arg(context, iterator.key()).toStdString()));
    }
    return foundation::Result<void>::Success();
}

bool IsColorHex(const QString &value)
{
    if (value.size() != 7 || value.at(0) != QLatin1Char('#')) return false;
    for (int index = 1; index < value.size(); ++index)
    {
        const QChar character = value.at(index);
        if (!character.isDigit() && (character < QLatin1Char('a') || character > QLatin1Char('f')) && (character < QLatin1Char('A') || character > QLatin1Char('F'))) return false;
    }
    return true;
}

QJsonObject ToJson(const LabelDefinition &label)
{
    return {{QStringLiteral("labelId"), label.labelId}, {QStringLiteral("name"), label.name}, {QStringLiteral("colorHex"), label.colorHex.toUpper()}};
}

foundation::Result<LabelDefinition> ParseLabel(const QJsonObject &object)
{
    const auto keys = ValidateExactKeys(object, {QStringLiteral("labelId"), QStringLiteral("name"), QStringLiteral("colorHex")}, QStringLiteral("Label entry"));
    if (!keys.IsSuccess()) return foundation::Result<LabelDefinition>::Failure(keys.Failure());
    for (const QString &key : {QStringLiteral("labelId"), QStringLiteral("name"), QStringLiteral("colorHex")})
    {
        if (!object.value(key).isString()) return foundation::Result<LabelDefinition>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Label field must be a string: ").append(key).toStdString()));
    }
    LabelDefinition label{object.value(QStringLiteral("labelId")).toString(), object.value(QStringLiteral("name")).toString(), object.value(QStringLiteral("colorHex")).toString().toUpper()};
    const auto validation = ValidateLabelDefinition(label);
    if (!validation.IsSuccess()) return foundation::Result<LabelDefinition>::Failure(validation.Failure());
    return foundation::Result<LabelDefinition>::Success(std::move(label));
}

foundation::Result<void> ValidateLabelCollection(const std::vector<LabelDefinition> &labels)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const LabelDefinition &label : labels)
    {
        const auto validation = ValidateLabelDefinition(label);
        if (!validation.IsSuccess()) return validation;
        if (ids.contains(label.labelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label ids must be unique"));
        const QString normalizedName = label.name.toCaseFolded();
        if (names.contains(normalizedName)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label names must be unique"));
        ids.insert(label.labelId);
        names.insert(normalizedName);
    }
    return foundation::Result<void>::Success();
}
}

foundation::Result<void> ValidateLabelDefinition(const LabelDefinition &label)
{
    if (QUuid(label.labelId).isNull()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label id must be a valid UUID"));
    if (label.name.isEmpty() || label.name.trimmed() != label.name) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label name must be non-empty and must not contain leading or trailing whitespace"));
    if (!IsColorHex(label.colorHex)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label color must use #RRGGBB format"));
    return foundation::Result<void>::Success();
}

foundation::Result<std::vector<LabelDefinition>> LabelStore::Load(const QString &projectRoot) const
{
    QFile file(QDir(projectRoot).filePath(QStringLiteral("labels.json")));
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<std::vector<LabelDefinition>>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read labels.json: ").append(file.errorString()).toStdString()));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<std::vector<LabelDefinition>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "labels.json is not a valid JSON object"));
    const QJsonObject root = document.object();
    const auto keys = ValidateExactKeys(root, {QStringLiteral("schemaVersion"), QStringLiteral("labels")}, QStringLiteral("labels.json"));
    if (!keys.IsSuccess()) return foundation::Result<std::vector<LabelDefinition>>::Failure(keys.Failure());
    if (!root.value(QStringLiteral("schemaVersion")).isDouble() || root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1 || !root.value(QStringLiteral("labels")).isArray()) return foundation::Result<std::vector<LabelDefinition>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "labels.json schema is invalid"));
    std::vector<LabelDefinition> labels;
    const QJsonArray values = root.value(QStringLiteral("labels")).toArray();
    labels.reserve(static_cast<size_t>(values.size()));
    for (const QJsonValue &value : values)
    {
        if (!value.isObject()) return foundation::Result<std::vector<LabelDefinition>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "labels.json label entry is not an object"));
        const auto label = ParseLabel(value.toObject());
        if (!label.IsSuccess()) return foundation::Result<std::vector<LabelDefinition>>::Failure(label.Failure());
        labels.push_back(label.Value());
    }
    const auto collection = ValidateLabelCollection(labels);
    if (!collection.IsSuccess()) return foundation::Result<std::vector<LabelDefinition>>::Failure(collection.Failure());
    return foundation::Result<std::vector<LabelDefinition>>::Success(std::move(labels));
}

foundation::Result<void> LabelStore::Save(const QString &projectRoot, const std::vector<LabelDefinition> &labels) const
{
    const auto collection = ValidateLabelCollection(labels);
    if (!collection.IsSuccess()) return collection;
    QJsonArray values;
    for (const LabelDefinition &label : labels) values.append(ToJson(label));
    QSaveFile file(QDir(projectRoot).filePath(QStringLiteral("labels.json")));
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write labels.json: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("labels"), values}}).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write labels.json: ").append(file.errorString()).toStdString()));
    if (!file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit labels.json: ").append(file.errorString()).toStdString()));
    const auto reloaded = Load(projectRoot);
    if (!reloaded.IsSuccess()) return foundation::Result<void>::Failure(reloaded.Failure());
    if (reloaded.Value().size() != labels.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "labels.json verification after save returned a different label count"));
    return foundation::Result<void>::Success();
}

foundation::Result<LabelDefinition> LabelStore::AddLabel(const QString &projectRoot, const QString &name, const QString &colorHex) const
{
    auto labels = Load(projectRoot);
    if (!labels.IsSuccess()) return foundation::Result<LabelDefinition>::Failure(labels.Failure());
    LabelDefinition label{QUuid::createUuid().toString(QUuid::WithoutBraces), name, colorHex.toUpper()};
    const auto validation = ValidateLabelDefinition(label);
    if (!validation.IsSuccess()) return foundation::Result<LabelDefinition>::Failure(validation.Failure());
    labels.Value().push_back(label);
    const auto saved = Save(projectRoot, labels.Value());
    if (!saved.IsSuccess()) return foundation::Result<LabelDefinition>::Failure(saved.Failure());
    return foundation::Result<LabelDefinition>::Success(std::move(label));
}
}
