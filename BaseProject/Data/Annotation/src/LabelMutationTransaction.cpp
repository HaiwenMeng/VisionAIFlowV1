#include "visionaiflow/annotation/LabelMutationTransaction.h"

#include "visionaiflow/annotation/AnnotationStore.h"
#include "visionaiflow/annotation/LabelImpactAnalyzer.h"
#include "visionaiflow/project_store/LabelStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <algorithm>

namespace visionaiflow::annotation
{
namespace
{
QString ActionToString(const LabelMutationAction action)
{
    switch (action)
    {
    case LabelMutationAction::Delete: return QStringLiteral("delete");
    case LabelMutationAction::Rename: return QStringLiteral("rename");
    case LabelMutationAction::Merge: return QStringLiteral("merge");
    }
    return QStringLiteral("unknown");
}

foundation::Result<void> ValidateRequest(const LabelMutationRequest &request)
{
    if (request.sourceLabelIds.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label mutation transaction requires at least one source label id"));
    QSet<QString> ids;
    for (const QString &labelId : request.sourceLabelIds)
    {
        if (labelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label mutation transaction source label id must not be empty"));
        if (ids.contains(labelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label mutation transaction source label ids must be unique"));
        ids.insert(labelId);
    }
    if (request.action == LabelMutationAction::Delete)
    {
        if (!request.targetLabelId.isEmpty() || !request.newName.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Delete label transaction must not define a target label or new name"));
    }
    else if (request.action == LabelMutationAction::Rename)
    {
        if (request.sourceLabelIds.size() != 1 || request.newName.isEmpty() || !request.targetLabelId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rename label transaction requires one source label id and one new name"));
    }
    else if (request.action == LabelMutationAction::Merge)
    {
        if (request.targetLabelId.isEmpty() || request.newName.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Merge label transaction requires a target label id and target label name"));
        if (ids.contains(request.targetLabelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Merge label transaction target must not also be a source label"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidatePlanAgainstLabels(const QString &projectRoot, const LabelMutationRequest &request)
{
    project_store::LabelStore labelStore;
    const auto labels = labelStore.Load(projectRoot);
    if (!labels.IsSuccess()) return foundation::Result<void>::Failure(labels.Failure());

    QHash<QString, project_store::LabelDefinition> labelsById;
    QHash<QString, QString> labelIdsByName;
    for (const project_store::LabelDefinition &label : labels.Value())
    {
        labelsById.insert(label.labelId, label);
        labelIdsByName.insert(label.name.toCaseFolded(), label.labelId);
    }

    for (const QString &sourceLabelId : request.sourceLabelIds)
    {
        if (!labelsById.contains(sourceLabelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label mutation source label does not exist in labels.json"));
    }

    if (request.action == LabelMutationAction::Rename)
    {
        const project_store::LabelDefinition source = labelsById.value(request.sourceLabelIds.front());
        project_store::LabelDefinition renamed{source.labelId, request.newName, source.colorHex};
        const auto labelValidation = project_store::ValidateLabelDefinition(renamed);
        if (!labelValidation.IsSuccess()) return labelValidation;
        const QString normalizedName = request.newName.toCaseFolded();
        if (source.name.toCaseFolded() == normalizedName) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rename label transaction new name must be different from the current name"));
        if (labelIdsByName.contains(normalizedName)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Rename label transaction new name already exists"));
    }
    else if (request.action == LabelMutationAction::Merge)
    {
        if (!labelsById.contains(request.targetLabelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Merge label transaction target label does not exist in labels.json"));
        const project_store::LabelDefinition target = labelsById.value(request.targetLabelId);
        project_store::LabelDefinition merged{target.labelId, request.newName, target.colorHex};
        const auto labelValidation = project_store::ValidateLabelDefinition(merged);
        if (!labelValidation.IsSuccess()) return labelValidation;
        const QString normalizedName = request.newName.toCaseFolded();
        const QString existingNameOwner = labelIdsByName.value(normalizedName);
        if (!existingNameOwner.isEmpty() && existingNameOwner != request.targetLabelId) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Merge label transaction target name already belongs to another label"));
    }

    return foundation::Result<void>::Success();
}

bool IsSafeTransactionRelativePath(const QString &relativePath)
{
    const QString normalized = QDir::cleanPath(relativePath);
    return normalized == relativePath && QDir::isRelativePath(normalized) && normalized.startsWith(QStringLiteral("backups/label-transactions/")) && normalized.endsWith(QStringLiteral("/transaction.json")) && !normalized.contains(QStringLiteral("/../"));
}

bool IsSafeProjectFileRelativePath(const QString &relativePath)
{
    const QString normalized = QDir::cleanPath(relativePath);
    if (normalized != relativePath || !QDir::isRelativePath(normalized) || normalized.contains(QStringLiteral("/../"))) return false;
    if (normalized == QStringLiteral("labels.json")) return true;
    if (normalized.startsWith(QStringLiteral("data/annotations/")) && normalized.endsWith(QStringLiteral(".json")))
    {
        const QString imageId = normalized.mid(QStringLiteral("data/annotations/").size(), normalized.size() - QStringLiteral("data/annotations/").size() - QStringLiteral(".json").size());
        return !QUuid(imageId).isNull();
    }
    return false;
}

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

bool IsSha256Hex(const QString &value)
{
    if (value.size() != 64) return false;
    for (const QChar character : value)
    {
        if (!character.isDigit() && (character < QLatin1Char('a') || character > QLatin1Char('f')) && (character < QLatin1Char('A') || character > QLatin1Char('F'))) return false;
    }
    return true;
}

foundation::Result<QString> Sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QString>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read annotation file for label transaction: ").append(file.errorString()).toStdString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && file.error() != QFile::NoError) return foundation::Result<QString>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to hash annotation file for label transaction: ").append(file.errorString()).toStdString()));
        hash.addData(block);
    }
    return foundation::Result<QString>::Success(QString::fromLatin1(hash.result().toHex()));
}

foundation::Result<QJsonObject> ReadTransactionObject(const QString &projectRoot, const QString &transactionRelativePath)
{
    if (!IsSafeTransactionRelativePath(transactionRelativePath)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label transaction path is unsafe or not a transaction.json path"));
    QFile file(QDir(projectRoot).filePath(transactionRelativePath));
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read label transaction file: ").append(file.errorString()).toStdString()));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction file is not a valid JSON object"));
    return foundation::Result<QJsonObject>::Success(document.object());
}

foundation::Result<void> RestoreFileFromBackup(const QString &projectRoot, const QJsonObject &entry)
{
    const QString relativePath = entry.value(QStringLiteral("relativePath")).toString();
    const QString backupRelativePath = entry.value(QStringLiteral("backupRelativePath")).toString();
    const QString expectedHash = entry.value(QStringLiteral("sha256")).toString();
    const qint64 expectedBytes = entry.value(QStringLiteral("bytes")).toVariant().toLongLong();
    if (!IsSafeProjectFileRelativePath(relativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback target path is unsafe"));
    QFile backup(QDir(projectRoot).filePath(backupRelativePath));
    if (!backup.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read label rollback backup file: ").append(backup.errorString()).toStdString()));
    const QString targetAbsolutePath = QDir(projectRoot).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(targetAbsolutePath).absolutePath())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create rollback target directory"));
    QSaveFile target(targetAbsolutePath);
    if (!target.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open rollback target file: ").append(target.errorString()).toStdString()));
    while (!backup.atEnd())
    {
        const QByteArray block = backup.read(1024 * 1024);
        if (block.isEmpty() && backup.error() != QFile::NoError) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read label rollback backup file: ").append(backup.errorString()).toStdString()));
        if (!block.isEmpty() && target.write(block) != block.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write rollback target file: ").append(target.errorString()).toStdString()));
    }
    if (!target.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit rollback target file: ").append(target.errorString()).toStdString()));
    const QFileInfo restored(targetAbsolutePath);
    if (!restored.isFile() || restored.size() != expectedBytes) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Restored rollback target file size mismatch"));
    const auto restoredHash = Sha256File(targetAbsolutePath);
    if (!restoredHash.IsSuccess()) return foundation::Result<void>::Failure(restoredHash.Failure());
    if (restoredHash.Value().compare(expectedHash, Qt::CaseInsensitive) != 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Restored rollback target file hash mismatch"));
    return foundation::Result<void>::Success();
}

foundation::Result<QJsonObject> BackupFile(const QString &projectRoot, const QString &sourceRelativePath, const QString &transactionId)
{
    const QString sourceAbsolutePath = QDir(projectRoot).filePath(sourceRelativePath);
    const QFileInfo sourceInfo(sourceAbsolutePath);
    if (!sourceInfo.isFile()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Label transaction rollback source is missing: ").append(sourceRelativePath).toStdString()));
    const auto sourceHash = Sha256File(sourceAbsolutePath);
    if (!sourceHash.IsSuccess()) return foundation::Result<QJsonObject>::Failure(sourceHash.Failure());
    QFile source(sourceAbsolutePath);
    if (!source.open(QIODevice::ReadOnly)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read rollback source file: ").append(source.errorString()).toStdString()));
    const QString backupRelativePath = QStringLiteral("backups/label-transactions/") + transactionId + QStringLiteral("/files/") + sourceRelativePath;
    const QString backupAbsolutePath = QDir(projectRoot).filePath(backupRelativePath);
    if (!QDir().mkpath(QFileInfo(backupAbsolutePath).absolutePath())) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to create rollback backup directory for: ").append(sourceRelativePath).toStdString()));
    QSaveFile backup(backupAbsolutePath);
    if (!backup.open(QIODevice::WriteOnly)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write rollback backup file: ").append(backup.errorString()).toStdString()));
    while (!source.atEnd())
    {
        const QByteArray block = source.read(1024 * 1024);
        if (block.isEmpty() && source.error() != QFile::NoError) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read rollback source file: ").append(source.errorString()).toStdString()));
        if (!block.isEmpty() && backup.write(block) != block.size()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write rollback backup file: ").append(backup.errorString()).toStdString()));
    }
    if (!backup.commit()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit rollback backup file: ").append(backup.errorString()).toStdString()));
    const auto backupHash = Sha256File(backupAbsolutePath);
    if (!backupHash.IsSuccess()) return foundation::Result<QJsonObject>::Failure(backupHash.Failure());
    if (backupHash.Value().compare(sourceHash.Value(), Qt::CaseInsensitive) != 0) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Rollback backup file hash does not match its source"));
    return foundation::Result<QJsonObject>::Success(QJsonObject{{QStringLiteral("relativePath"), sourceRelativePath}, {QStringLiteral("backupRelativePath"), backupRelativePath}, {QStringLiteral("bytes"), sourceInfo.size()}, {QStringLiteral("sha256"), sourceHash.Value()}});
}

QJsonArray StringArray(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) array.append(value);
    return array;
}

QJsonObject ImpactToJson(const LabelImpact &impact)
{
    QJsonArray images;
    for (const LabelUsageInImage &image : impact.images)
    {
        images.append(QJsonObject{{QStringLiteral("imageId"), image.imageId}, {QStringLiteral("annotationCount"), image.annotationCount}});
    }
    return {{QStringLiteral("labelId"), impact.labelId}, {QStringLiteral("totalAnnotationCount"), impact.totalAnnotationCount}, {QStringLiteral("images"), images}};
}
}

foundation::Result<LabelMutationTransactionFile> LabelMutationTransactionWriter::Create(const QString &projectRoot, const LabelMutationRequest &request) const
{
    if (projectRoot.isEmpty()) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root must not be empty when creating a label mutation transaction"));
    const auto requestValidation = ValidateRequest(request);
    if (!requestValidation.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(requestValidation.Failure());
    const auto planValidation = ValidatePlanAgainstLabels(projectRoot, request);
    if (!planValidation.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(planValidation.Failure());
    LabelImpactAnalyzer analyzer;
    const auto impacts = analyzer.Analyze(projectRoot, request.sourceLabelIds);
    if (!impacts.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(impacts.Failure());
    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSet<QString> affectedImageIds;
    QJsonArray impactJson;
    for (const LabelImpact &impact : impacts.Value())
    {
        impactJson.append(ImpactToJson(impact));
        for (const LabelUsageInImage &image : impact.images) affectedImageIds.insert(image.imageId);
    }

    QJsonArray files;
    QJsonArray rollbackFiles;
    const auto labelsBackup = BackupFile(projectRoot, QStringLiteral("labels.json"), transactionId);
    if (!labelsBackup.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(labelsBackup.Failure());
    rollbackFiles.append(labelsBackup.Value());
    QStringList orderedImageIds;
    for (const QString &imageId : affectedImageIds) orderedImageIds.append(imageId);
    orderedImageIds.sort();
    for (const QString &imageId : orderedImageIds)
    {
        const QString relativePath = QStringLiteral("data/annotations/") + imageId + QStringLiteral(".json");
        const QString absolutePath = QDir(projectRoot).filePath(relativePath);
        const QFileInfo info(absolutePath);
        if (!info.isFile()) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction references a missing annotation file"));
        const auto sha256 = Sha256File(absolutePath);
        if (!sha256.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(sha256.Failure());
        const auto annotationBackup = BackupFile(projectRoot, relativePath, transactionId);
        if (!annotationBackup.IsSuccess()) return foundation::Result<LabelMutationTransactionFile>::Failure(annotationBackup.Failure());
        QJsonObject fileEntry{{QStringLiteral("imageId"), imageId}, {QStringLiteral("relativePath"), relativePath}, {QStringLiteral("bytes"), info.size()}, {QStringLiteral("sha256"), sha256.Value()}};
        fileEntry.insert(QStringLiteral("backupRelativePath"), annotationBackup.Value().value(QStringLiteral("backupRelativePath")).toString());
        files.append(fileEntry);
        rollbackFiles.append(annotationBackup.Value());
    }

    const QString relativePath = QStringLiteral("backups/label-transactions/") + transactionId + QStringLiteral("/transaction.json");
    const QString absolutePath = QDir(projectRoot).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath())) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create label transaction backup directory"));
    const QJsonObject document{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("transactionId"), transactionId},
        {QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("action"), ActionToString(request.action)},
        {QStringLiteral("sourceLabelIds"), StringArray(request.sourceLabelIds)},
        {QStringLiteral("targetLabelId"), request.targetLabelId},
        {QStringLiteral("newName"), request.newName},
        {QStringLiteral("impacts"), impactJson},
        {QStringLiteral("affectedAnnotationFiles"), files},
        {QStringLiteral("rollbackFiles"), rollbackFiles}};
    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write label transaction file: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write label transaction file: ").append(file.errorString()).toStdString()));
    if (!file.commit()) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit label transaction file: ").append(file.errorString()).toStdString()));
    QFile verify(absolutePath);
    if (!verify.open(QIODevice::ReadOnly)) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to reopen label transaction file after commit"));
    QJsonParseError error{};
    const QJsonDocument parsed = QJsonDocument::fromJson(verify.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !parsed.isObject() || parsed.object().value(QStringLiteral("transactionId")).toString() != transactionId) return foundation::Result<LabelMutationTransactionFile>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction file verification failed after commit"));
    return foundation::Result<LabelMutationTransactionFile>::Success({transactionId, relativePath});
}

foundation::Result<void> LabelMutationTransactionVerifier::VerifyRollbackFiles(const QString &projectRoot, const QString &transactionRelativePath) const
{
    if (projectRoot.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root must not be empty when verifying a label transaction"));
    if (!IsSafeTransactionRelativePath(transactionRelativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label transaction path is unsafe or not a transaction.json path"));
    QFile file(QDir(projectRoot).filePath(transactionRelativePath));
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read label transaction file: ").append(file.errorString()).toStdString()));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction file is not a valid JSON object"));
    const QJsonObject root = document.object();
    const auto rootKeys = ValidateExactKeys(root, {QStringLiteral("schemaVersion"), QStringLiteral("transactionId"), QStringLiteral("createdUtc"), QStringLiteral("action"), QStringLiteral("sourceLabelIds"), QStringLiteral("targetLabelId"), QStringLiteral("newName"), QStringLiteral("impacts"), QStringLiteral("affectedAnnotationFiles"), QStringLiteral("rollbackFiles")}, QStringLiteral("Label transaction"));
    if (!rootKeys.IsSuccess()) return rootKeys;
    if (!root.value(QStringLiteral("schemaVersion")).isDouble() || root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction schemaVersion is unsupported"));
    const QString transactionId = root.value(QStringLiteral("transactionId")).toString();
    if (QUuid(transactionId).isNull() || !transactionRelativePath.startsWith(QStringLiteral("backups/label-transactions/") + transactionId + QStringLiteral("/"))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction id does not match its path"));
    const QString action = root.value(QStringLiteral("action")).toString();
    if (action != QStringLiteral("delete") && action != QStringLiteral("rename") && action != QStringLiteral("merge")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction action is unsupported"));
    for (const QString &stringField : {QStringLiteral("createdUtc"), QStringLiteral("targetLabelId"), QStringLiteral("newName")})
    {
        if (!root.value(stringField).isString()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Label transaction field must be a string: ").append(stringField).toStdString()));
    }
    const QDateTime createdUtc = QDateTime::fromString(root.value(QStringLiteral("createdUtc")).toString(), Qt::ISODateWithMs);
    if (!createdUtc.isValid() || createdUtc.timeSpec() != Qt::UTC) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction createdUtc must be a valid UTC ISO timestamp"));
    if (!root.value(QStringLiteral("sourceLabelIds")).isArray() || !root.value(QStringLiteral("impacts")).isArray() || !root.value(QStringLiteral("affectedAnnotationFiles")).isArray() || !root.value(QStringLiteral("rollbackFiles")).isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction array fields are invalid"));
    const QJsonArray rollbackFiles = root.value(QStringLiteral("rollbackFiles")).toArray();
    if (rollbackFiles.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction has no rollback files"));
    QSet<QString> rollbackBackupPaths;
    for (const QJsonValue &value : rollbackFiles)
    {
        if (!value.isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback entry is not an object"));
        const QJsonObject entry = value.toObject();
        const auto keys = ValidateExactKeys(entry, {QStringLiteral("relativePath"), QStringLiteral("backupRelativePath"), QStringLiteral("bytes"), QStringLiteral("sha256")}, QStringLiteral("Label transaction rollback entry"));
        if (!keys.IsSuccess()) return keys;
        const QString backupRelativePath = entry.value(QStringLiteral("backupRelativePath")).toString();
        const QString sha256 = entry.value(QStringLiteral("sha256")).toString();
        const qint64 bytes = entry.value(QStringLiteral("bytes")).toVariant().toLongLong();
        if (entry.value(QStringLiteral("relativePath")).toString().isEmpty() || !backupRelativePath.startsWith(QStringLiteral("backups/label-transactions/") + transactionId + QStringLiteral("/files/")) || bytes <= 0 || !IsSha256Hex(sha256)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback entry is invalid"));
        const QFileInfo backupInfo(QDir(projectRoot).filePath(backupRelativePath));
        if (!backupInfo.isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback backup file is missing"));
        if (backupInfo.size() != bytes) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback backup file size mismatch"));
        const auto actualHash = Sha256File(backupInfo.absoluteFilePath());
        if (!actualHash.IsSuccess()) return foundation::Result<void>::Failure(actualHash.Failure());
        if (actualHash.Value().compare(sha256, Qt::CaseInsensitive) != 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback backup file hash mismatch"));
        if (rollbackBackupPaths.contains(backupRelativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction contains duplicate rollback backup paths"));
        rollbackBackupPaths.insert(backupRelativePath);
    }
    const QJsonArray affectedFiles = root.value(QStringLiteral("affectedAnnotationFiles")).toArray();
    for (const QJsonValue &value : affectedFiles)
    {
        if (!value.isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction affected annotation entry is not an object"));
        const QJsonObject entry = value.toObject();
        const auto keys = ValidateExactKeys(entry, {QStringLiteral("imageId"), QStringLiteral("relativePath"), QStringLiteral("bytes"), QStringLiteral("sha256"), QStringLiteral("backupRelativePath")}, QStringLiteral("Label transaction affected annotation entry"));
        if (!keys.IsSuccess()) return keys;
        const QString backupRelativePath = entry.value(QStringLiteral("backupRelativePath")).toString();
        if (!rollbackBackupPaths.contains(backupRelativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction affected annotation file does not have a matching rollback backup"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<LabelMutationApplyResult> LabelMutationTransactionApplier::Apply(const QString &projectRoot, const LabelMutationRequest &request) const
{
    LabelMutationTransactionWriter writer;
    const auto transaction = writer.Create(projectRoot, request);
    if (!transaction.IsSuccess()) return foundation::Result<LabelMutationApplyResult>::Failure(transaction.Failure());

    auto failWithRollback = [this, &projectRoot, &transaction](const foundation::Error &error) -> foundation::Result<LabelMutationApplyResult> {
        const auto rollback = Rollback(projectRoot, transaction.Value().relativePath);
        if (!rollback.IsSuccess())
        {
            const QString message = QString::fromStdString(error.message) + QStringLiteral("; rollback failed: ") + QString::fromStdString(rollback.Failure().message);
            return foundation::Result<LabelMutationApplyResult>::Failure(foundation::Error::Create(error.code, message.toStdString()));
        }
        return foundation::Result<LabelMutationApplyResult>::Failure(error);
    };

    LabelMutationTransactionVerifier verifier;
    const auto verified = verifier.VerifyRollbackFiles(projectRoot, transaction.Value().relativePath);
    if (!verified.IsSuccess()) return foundation::Result<LabelMutationApplyResult>::Failure(verified.Failure());

    const auto transactionObject = ReadTransactionObject(projectRoot, transaction.Value().relativePath);
    if (!transactionObject.IsSuccess()) return foundation::Result<LabelMutationApplyResult>::Failure(transactionObject.Failure());

    project_store::LabelStore labelStore;
    auto labels = labelStore.Load(projectRoot);
    if (!labels.IsSuccess()) return failWithRollback(labels.Failure());

    qsizetype changedLabelCount = 0;
    if (request.action == LabelMutationAction::Delete)
    {
        const auto oldSize = labels.Value().size();
        labels.Value().erase(std::remove_if(labels.Value().begin(), labels.Value().end(), [&request](const project_store::LabelDefinition &label) { return request.sourceLabelIds.contains(label.labelId); }), labels.Value().end());
        changedLabelCount = static_cast<qsizetype>(oldSize - labels.Value().size());
    }
    else if (request.action == LabelMutationAction::Rename)
    {
        for (project_store::LabelDefinition &label : labels.Value())
        {
            if (label.labelId == request.sourceLabelIds.front())
            {
                label.name = request.newName;
                changedLabelCount = 1;
                break;
            }
        }
    }
    else if (request.action == LabelMutationAction::Merge)
    {
        for (project_store::LabelDefinition &label : labels.Value())
        {
            if (label.labelId == request.targetLabelId && label.name != request.newName)
            {
                label.name = request.newName;
                ++changedLabelCount;
                break;
            }
        }
        const auto oldSize = labels.Value().size();
        labels.Value().erase(std::remove_if(labels.Value().begin(), labels.Value().end(), [&request](const project_store::LabelDefinition &label) { return request.sourceLabelIds.contains(label.labelId); }), labels.Value().end());
        changedLabelCount += static_cast<qsizetype>(oldSize - labels.Value().size());
    }

    AnnotationStore annotationStore;
    qsizetype changedAnnotationCount = 0;
    const QJsonArray affectedFiles = transactionObject.Value().value(QStringLiteral("affectedAnnotationFiles")).toArray();
    for (const QJsonValue &value : affectedFiles)
    {
        if (!value.isObject()) return failWithRollback(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label mutation transaction affected file entry is invalid"));
        const QJsonObject fileEntry = value.toObject();
        const QString imageId = fileEntry.value(QStringLiteral("imageId")).toString();
        if (QUuid(imageId).isNull()) return failWithRollback(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label mutation transaction affected image id is invalid"));
        auto annotations = annotationStore.Load(projectRoot, imageId);
        if (!annotations.IsSuccess()) return failWithRollback(annotations.Failure());
        qsizetype imageChanges = 0;
        if (request.action == LabelMutationAction::Delete)
        {
            const auto oldSize = annotations.Value().size();
            annotations.Value().erase(std::remove_if(annotations.Value().begin(), annotations.Value().end(), [&request](const Annotation &annotation) { return request.sourceLabelIds.contains(annotation.labelId); }), annotations.Value().end());
            imageChanges = static_cast<qsizetype>(oldSize - annotations.Value().size());
        }
        else if (request.action == LabelMutationAction::Merge)
        {
            for (Annotation &annotation : annotations.Value())
            {
                if (request.sourceLabelIds.contains(annotation.labelId))
                {
                    annotation.labelId = request.targetLabelId;
                    ++imageChanges;
                }
            }
        }
        if (imageChanges > 0)
        {
            const auto saved = annotationStore.Save(projectRoot, imageId, annotations.Value());
            if (!saved.IsSuccess()) return failWithRollback(saved.Failure());
            changedAnnotationCount += imageChanges;
        }
    }

    const auto savedLabels = labelStore.Save(projectRoot, labels.Value());
    if (!savedLabels.IsSuccess()) return failWithRollback(savedLabels.Failure());

    return foundation::Result<LabelMutationApplyResult>::Success({transaction.Value(), changedLabelCount, changedAnnotationCount});
}

foundation::Result<void> LabelMutationTransactionApplier::Rollback(const QString &projectRoot, const QString &transactionRelativePath) const
{
    LabelMutationTransactionVerifier verifier;
    const auto verified = verifier.VerifyRollbackFiles(projectRoot, transactionRelativePath);
    if (!verified.IsSuccess()) return verified;
    const auto transactionObject = ReadTransactionObject(projectRoot, transactionRelativePath);
    if (!transactionObject.IsSuccess()) return foundation::Result<void>::Failure(transactionObject.Failure());
    const QJsonArray rollbackFiles = transactionObject.Value().value(QStringLiteral("rollbackFiles")).toArray();
    for (const QJsonValue &value : rollbackFiles)
    {
        if (!value.isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Label transaction rollback entry is not an object"));
        const auto restored = RestoreFileFromBackup(projectRoot, value.toObject());
        if (!restored.IsSuccess()) return restored;
    }
    return foundation::Result<void>::Success();
}
}
