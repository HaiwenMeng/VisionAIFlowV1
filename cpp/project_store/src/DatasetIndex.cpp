#include "visionaiflow/project_store/DatasetIndex.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
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
foundation::Result<void> WriteIndex(const QString &path, const QJsonArray &images)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write dataset index: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("images"), images}}).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to write dataset index: ").append(file.errorString()).toStdString()));
    if (!file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to commit dataset index: ").append(file.errorString()).toStdString()));
    return foundation::Result<void>::Success();
}

foundation::Result<QString> CalculateSha256(const QString &sourcePath)
{
    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QString>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read source image: ").append(file.errorString()).toStdString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && file.error() != QFile::NoError) return foundation::Result<QString>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to hash source image: ").append(file.errorString()).toStdString()));
        hash.addData(block);
    }
    return foundation::Result<QString>::Success(QString::fromLatin1(hash.result().toHex()));
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

bool IsSafeImageRelativePath(const QString &relativePath)
{
    const QString normalized = QDir::cleanPath(relativePath);
    return !normalized.isEmpty() && QDir::isRelativePath(normalized) && normalized.startsWith(QStringLiteral("data/images/")) && normalized != QStringLiteral("data/images/..") && !normalized.contains(QStringLiteral("/../"));
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

foundation::Result<DatasetImage> ParseImage(const QJsonObject &object)
{
    const auto keys = ValidateExactKeys(object, {QStringLiteral("imageId"), QStringLiteral("fileName"), QStringLiteral("relativePath"), QStringLiteral("sha256"), QStringLiteral("bytes"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("importedUtc")}, QStringLiteral("Dataset index image entry"));
    if (!keys.IsSuccess()) return foundation::Result<DatasetImage>::Failure(keys.Failure());
    for (const QString &stringField : {QStringLiteral("imageId"), QStringLiteral("fileName"), QStringLiteral("relativePath"), QStringLiteral("sha256"), QStringLiteral("importedUtc")})
    {
        if (!object.value(stringField).isString()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Dataset index image field has invalid type: ").append(stringField).toStdString()));
    }
    for (const QString &numberField : {QStringLiteral("bytes"), QStringLiteral("width"), QStringLiteral("height")})
    {
        if (!object.value(numberField).isDouble()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Dataset index image numeric field has invalid type: ").append(numberField).toStdString()));
    }
    DatasetImage image{object.value(QStringLiteral("imageId")).toString(), object.value(QStringLiteral("fileName")).toString(), object.value(QStringLiteral("relativePath")).toString(), object.value(QStringLiteral("sha256")).toString(), object.value(QStringLiteral("bytes")).toVariant().toLongLong(), QSize(object.value(QStringLiteral("width")).toInt(), object.value(QStringLiteral("height")).toInt()), object.value(QStringLiteral("importedUtc")).toString()};
    const QDateTime importedUtc = QDateTime::fromString(image.importedUtc, Qt::ISODateWithMs);
    if (QUuid(image.imageId).isNull() || image.fileName.isEmpty() || !IsSafeImageRelativePath(image.relativePath) || !IsSha256Hex(image.sha256) || image.bytes <= 0 || !image.size.isValid() || !importedUtc.isValid() || importedUtc.timeSpec() != Qt::UTC) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index contains an invalid image record"));
    return foundation::Result<DatasetImage>::Success(std::move(image));
}

foundation::Result<void> VerifyIndexedImageFile(const QString &projectRoot, const DatasetImage &image)
{
    const QString path = QDir(projectRoot).filePath(QDir::cleanPath(image.relativePath));
    const QFileInfo fileInfo(path);
    if (!fileInfo.isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index references a missing image file"));
    if (fileInfo.size() != image.bytes) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index byte count does not match the image file"));
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (reader.size() != image.size) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index dimensions do not match the image file"));
    const auto actualHash = CalculateSha256(path);
    if (!actualHash.IsSuccess()) return foundation::Result<void>::Failure(actualHash.Failure());
    if (actualHash.Value().compare(image.sha256, Qt::CaseInsensitive) != 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index SHA-256 does not match the image file"));
    return foundation::Result<void>::Success();
}

QJsonObject ToJson(const DatasetImage &image)
{
    return {{QStringLiteral("imageId"), image.imageId}, {QStringLiteral("fileName"), image.fileName}, {QStringLiteral("relativePath"), image.relativePath}, {QStringLiteral("sha256"), image.sha256}, {QStringLiteral("bytes"), image.bytes}, {QStringLiteral("width"), image.size.width()}, {QStringLiteral("height"), image.size.height()}, {QStringLiteral("importedUtc"), image.importedUtc}};
}
}

foundation::Result<std::vector<DatasetImage>> DatasetIndex::Load(const QString &projectRoot) const
{
    const QString indexPath = QDir(projectRoot).filePath(QStringLiteral("data/index.json"));
    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<std::vector<DatasetImage>>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read dataset index: ").append(file.errorString()).toStdString()));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<std::vector<DatasetImage>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index is not a valid JSON object"));
    const QJsonObject root = document.object();
    const auto keys = ValidateExactKeys(root, {QStringLiteral("schemaVersion"), QStringLiteral("images")}, QStringLiteral("Dataset index"));
    if (!keys.IsSuccess()) return foundation::Result<std::vector<DatasetImage>>::Failure(keys.Failure());
    if (!root.value(QStringLiteral("schemaVersion")).isDouble() || root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1 || !root.value(QStringLiteral("images")).isArray()) return foundation::Result<std::vector<DatasetImage>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index schema is invalid"));
    std::vector<DatasetImage> images;
    const QJsonArray values = root.value(QStringLiteral("images")).toArray();
    images.reserve(static_cast<size_t>(values.size()));
    QSet<QString> imageIds;
    QSet<QString> hashes;
    QSet<QString> paths;
    for (const QJsonValue &value : values)
    {
        if (!value.isObject()) return foundation::Result<std::vector<DatasetImage>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index image entry is not an object"));
        const auto image = ParseImage(value.toObject());
        if (!image.IsSuccess()) return foundation::Result<std::vector<DatasetImage>>::Failure(image.Failure());
        if (imageIds.contains(image.Value().imageId) || hashes.contains(image.Value().sha256) || paths.contains(image.Value().relativePath)) return foundation::Result<std::vector<DatasetImage>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Dataset index contains duplicate image identity, hash, or path"));
        const auto fileVerified = VerifyIndexedImageFile(projectRoot, image.Value());
        if (!fileVerified.IsSuccess()) return foundation::Result<std::vector<DatasetImage>>::Failure(fileVerified.Failure());
        imageIds.insert(image.Value().imageId);
        hashes.insert(image.Value().sha256);
        paths.insert(image.Value().relativePath);
        images.push_back(image.Value());
    }
    return foundation::Result<std::vector<DatasetImage>>::Success(std::move(images));
}

foundation::Result<DatasetImage> DatasetIndex::ImportImage(const QString &projectRoot, const QString &sourcePath) const
{
    const QFileInfo source(sourcePath);
    if (projectRoot.isEmpty() || sourcePath.isEmpty()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root and source image path must not be empty"));
    if (!source.exists() || !source.isFile()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Source image does not exist or is not a regular file"));
    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QSize imageSize = reader.size();
    if (!imageSize.isValid()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Source image cannot be decoded: ").append(reader.errorString()).toStdString()));
    const auto hash = CalculateSha256(sourcePath);
    if (!hash.IsSuccess()) return foundation::Result<DatasetImage>::Failure(hash.Failure());
    const auto existing = Load(projectRoot);
    if (!existing.IsSuccess()) return foundation::Result<DatasetImage>::Failure(existing.Failure());
    for (const DatasetImage &image : existing.Value())
    {
        if (image.sha256 == hash.Value()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Source image is already imported into this project"));
    }
    const QString suffix = source.suffix().toLower();
    if (suffix.isEmpty()) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Source image must have a file extension"));
    const QString relativePath = QStringLiteral("data/images/") + hash.Value() + QLatin1Char('.') + suffix;
    const QString destinationPath = QDir(projectRoot).filePath(relativePath);
    if (QFileInfo::exists(destinationPath)) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Destination image path already exists"));
    if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create project image directory"));
    if (!QFile::copy(sourcePath, destinationPath)) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to copy source image into project"));
    const DatasetImage image{QUuid::createUuid().toString(QUuid::WithoutBraces), source.fileName(), relativePath, hash.Value(), source.size(), imageSize, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)};
    QJsonArray values;
    for (const DatasetImage &entry : existing.Value()) values.append(ToJson(entry));
    values.append(ToJson(image));
    const auto written = WriteIndex(QDir(projectRoot).filePath(QStringLiteral("data/index.json")), values);
    if (!written.IsSuccess())
    {
        if (!QFile::remove(destinationPath)) return foundation::Result<DatasetImage>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Dataset index update failed and copied image rollback failed: ").append(destinationPath).toStdString()));
        return foundation::Result<DatasetImage>::Failure(written.Failure());
    }
    return foundation::Result<DatasetImage>::Success(image);
}
}
