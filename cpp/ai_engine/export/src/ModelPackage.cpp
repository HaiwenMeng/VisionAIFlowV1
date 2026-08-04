#include "visionaiflow/export/ModelPackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <QDateTime>
#include <vector>

namespace visionaiflow::exporter
{
namespace
{
QJsonArray ShapeArray(const std::vector<int64_t> &dimensions)
{
    QJsonArray values;
    for (const int64_t dimension : dimensions) values.append(static_cast<qint64>(dimension));
    return values;
}

QJsonObject StaticShapeProfile(const std::vector<int64_t> &dimensions)
{
    const QJsonArray shape = ShapeArray(dimensions);
    return QJsonObject{{QStringLiteral("dynamicDimensions"), false}, {QStringLiteral("min"), shape}, {QStringLiteral("opt"), shape}, {QStringLiteral("max"), shape}};
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

foundation::Result<void> ValidatePackageMetadataCommon(const QString &packageRoot, const QString &packageId, const QString &packageVersion, const QString &adapterId, const QString &adapterVersion, const QString &trainingRunId, const QString &datasetId, const QString &trainingConfigSha256, const QString &sourceCheckpointSha256, const QString &exporterProductVersion, const QString &minSupportedProductVersion, const QString &maxSupportedProductVersion, const QString &licenseId, const QString &licenseName)
{
    if (packageRoot.isEmpty() || packageId.isEmpty() || packageVersion.isEmpty() || adapterId.isEmpty() || adapterVersion.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package identity metadata is incomplete"));
    if (trainingRunId.isEmpty() || datasetId.isEmpty() || !IsSha256Hex(trainingConfigSha256)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package training provenance is incomplete"));
    if (!sourceCheckpointSha256.isEmpty() && !IsSha256Hex(sourceCheckpointSha256)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package source checkpoint hash is invalid"));
    if (exporterProductVersion.isEmpty() || minSupportedProductVersion.isEmpty() || maxSupportedProductVersion.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package product version range is incomplete"));
    if (licenseId.isEmpty() || licenseName.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package license metadata is incomplete"));
    return foundation::Result<void>::Success();
}

QJsonObject TrainingProvenance(const QString &trainingRunId, const QString &datasetId, const QString &trainingConfigSha256, const QString &sourceCheckpointSha256, const QString &exporterProductVersion)
{
    QJsonObject provenance{{QStringLiteral("trainingRunId"), trainingRunId}, {QStringLiteral("datasetId"), datasetId}, {QStringLiteral("trainingConfigSha256"), trainingConfigSha256}, {QStringLiteral("exporterProductVersion"), exporterProductVersion}, {QStringLiteral("exportedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!sourceCheckpointSha256.isEmpty()) provenance.insert(QStringLiteral("sourceCheckpointSha256"), sourceCheckpointSha256);
    return provenance;
}

QJsonObject SupportedProductRange(const QString &minimumVersion, const QString &maximumVersion)
{
    return QJsonObject{{QStringLiteral("productId"), QStringLiteral("VisionAIFlowV1")}, {QStringLiteral("minVersion"), minimumVersion}, {QStringLiteral("maxVersion"), maximumVersion}};
}

QJsonObject LicenseMetadata(const QString &licenseId, const QString &licenseName)
{
    return QJsonObject{{QStringLiteral("licenseId"), licenseId}, {QStringLiteral("name"), licenseName}, {QStringLiteral("file"), QStringLiteral("licenses/license.json")}};
}

QJsonObject LicenseSidecar(const QString &licenseId, const QString &licenseName)
{
    return QJsonObject{{QStringLiteral("licenseId"), licenseId}, {QStringLiteral("name"), licenseName}, {QStringLiteral("scope"), QStringLiteral("model_package")}};
}

QJsonObject EmptyPluginManifest()
{
    return QJsonObject{{QStringLiteral("platform"), QStringLiteral("win-x64")}, {QStringLiteral("plugins"), QJsonArray{}}};
}

QJsonObject UnsignedSignatureMetadata()
{
    return QJsonObject{{QStringLiteral("signatureSchemaVersion"), 1}, {QStringLiteral("signatureState"), QStringLiteral("unsigned")}, {QStringLiteral("signedPayload"), QStringLiteral("checksums.json")}};
}

foundation::Result<void> WriteJson(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open JSON output: ").append(file.errorString()).toStdString()));
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically write JSON output: ").append(file.errorString()).toStdString()));
    return foundation::Result<void>::Success();
}

foundation::Result<QByteArray> Sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read package artifact: ").append(file.errorString()).toStdString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && file.error() != QFile::NoError) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to hash package artifact: ").append(file.errorString()).toStdString()));
        hash.addData(block);
    }
    return foundation::Result<QByteArray>::Success(hash.result().toHex());
}

bool IsSafeRelativePath(const QString &path)
{
    const QString normalized = QDir::cleanPath(path);
    return !path.isEmpty() && QDir::isRelativePath(path) && normalized != QStringLiteral("..") && !normalized.startsWith(QStringLiteral("../"));
}

foundation::Result<QJsonObject> ReadObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read package JSON: ").append(file.errorString()).toStdString()));
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<QJsonObject>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Package JSON is invalid or not an object"));
    return foundation::Result<QJsonObject>::Success(document.object());
}

foundation::Result<void> WriteChecksums(const QString &packageRoot, const QStringList &relativePaths)
{
    QJsonObject checksums;
    for (const QString &relativePath : relativePaths)
    {
        const auto hash = Sha256File(QDir(packageRoot).filePath(relativePath));
        if (!hash.IsSuccess()) return foundation::Result<void>::Failure(hash.Failure());
        checksums.insert(relativePath, QString::fromLatin1(hash.Value()));
    }
    return WriteJson(QDir(packageRoot).filePath(QStringLiteral("checksums.json")), checksums);
}

foundation::Result<void> VerifyChecksummedFile(const QString &packageRoot, const QString &relativePath, const QJsonObject &checksums)
{
    if (!IsSafeRelativePath(relativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package path is unsafe"));
    const QString expectedHash = checksums.value(relativePath).toString();
    if (expectedHash.size() != 64) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package checksum is missing or invalid"));
    const auto actualHash = Sha256File(QDir(packageRoot).filePath(relativePath));
    if (!actualHash.IsSuccess()) return foundation::Result<void>::Failure(actualHash.Failure());
    if (QString::fromLatin1(actualHash.Value()).compare(expectedHash, Qt::CaseInsensitive) != 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package checksum mismatch"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> VerifyPackageManifestCommon(const QJsonObject &packageObject)
{
    const QJsonObject provenance = packageObject.value(QStringLiteral("trainingProvenance")).toObject();
    if (provenance.value(QStringLiteral("trainingRunId")).toString().isEmpty() || provenance.value(QStringLiteral("datasetId")).toString().isEmpty() || !IsSha256Hex(provenance.value(QStringLiteral("trainingConfigSha256")).toString()) || provenance.value(QStringLiteral("exporterProductVersion")).toString().isEmpty() || provenance.value(QStringLiteral("exportedUtc")).toString().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package training provenance is missing or invalid"));
    const QString checkpointHash = provenance.value(QStringLiteral("sourceCheckpointSha256")).toString();
    if (!checkpointHash.isEmpty() && !IsSha256Hex(checkpointHash)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package source checkpoint hash is invalid"));
    const QJsonObject productRange = packageObject.value(QStringLiteral("supportedProductRange")).toObject();
    if (productRange.value(QStringLiteral("productId")).toString() != QStringLiteral("VisionAIFlowV1") || productRange.value(QStringLiteral("minVersion")).toString().isEmpty() || productRange.value(QStringLiteral("maxVersion")).toString().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package supported product range is missing or invalid"));
    if (!packageObject.value(QStringLiteral("pluginRequirements")).isArray()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package plugin requirements must be an array"));
    const QJsonObject license = packageObject.value(QStringLiteral("licenseMetadata")).toObject();
    if (license.value(QStringLiteral("licenseId")).toString().isEmpty() || license.value(QStringLiteral("name")).toString().isEmpty() || license.value(QStringLiteral("file")).toString() != QStringLiteral("licenses/license.json")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package license metadata is missing or invalid"));
    const QJsonObject runtime = packageObject.value(QStringLiteral("runtimeRequirements")).toObject();
    if (runtime.value(QStringLiteral("tensorRt")).toString() != QStringLiteral("10.0.1.6") || runtime.value(QStringLiteral("cuda")).toString() != QStringLiteral("11.8") || runtime.value(QStringLiteral("openVino")).toString() != QStringLiteral("2025.3.0")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package runtime requirements are unsupported"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> VerifyPackageSidecarsCommon(const QJsonObject &packageObject, const QJsonObject &signatureObject, const QJsonObject &pluginObject, const QJsonObject &licenseObject, const bool requireSignature)
{
    if (signatureObject.value(QStringLiteral("signatureSchemaVersion")).toInt() != 1 || signatureObject.value(QStringLiteral("signedPayload")).toString() != QStringLiteral("checksums.json")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package signature sidecar is invalid"));
    const QString signatureState = signatureObject.value(QStringLiteral("signatureState")).toString();
    if (signatureState != QStringLiteral("unsigned")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Signed model package verification is not available without the product public certificate"));
    if (requireSignature) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package is unsigned"));
    if (pluginObject.value(QStringLiteral("platform")).toString() != QStringLiteral("win-x64") || !pluginObject.value(QStringLiteral("plugins")).isArray() || pluginObject.value(QStringLiteral("plugins")).toArray().size() != packageObject.value(QStringLiteral("pluginRequirements")).toArray().size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package plugin sidecar does not match package requirements"));
    const QJsonObject licenseMetadata = packageObject.value(QStringLiteral("licenseMetadata")).toObject();
    if (licenseObject.value(QStringLiteral("licenseId")).toString() != licenseMetadata.value(QStringLiteral("licenseId")).toString() || licenseObject.value(QStringLiteral("name")).toString() != licenseMetadata.value(QStringLiteral("name")).toString() || licenseObject.value(QStringLiteral("scope")).toString() != QStringLiteral("model_package")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package license sidecar does not match package metadata"));
    return foundation::Result<void>::Success();
}

void RollbackPackageInstall(const QStringList &copiedFiles, const QStringList &createdDirectories)
{
    for (auto file = copiedFiles.crbegin(); file != copiedFiles.crend(); ++file) QFile::remove(*file);
    for (auto directory = createdDirectories.crbegin(); directory != createdDirectories.crend(); ++directory) QDir().rmdir(*directory);
}

foundation::Result<void> EnsureInstallDirectory(const QString &destinationRoot, const QString &relativeDirectory, QStringList &createdDirectories)
{
    if (relativeDirectory.isEmpty() || relativeDirectory == QStringLiteral(".")) return foundation::Result<void>::Success();
    const QString normalized = QDir::cleanPath(relativeDirectory);
    if (!QDir::isRelativePath(normalized) || normalized == QStringLiteral("..") || normalized.startsWith(QStringLiteral("../"))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package install path is unsafe"));
    QString accumulated;
    const QStringList segments = normalized.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    for (const QString &segment : segments)
    {
        if (segment == QStringLiteral("..")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package install path is unsafe"));
        accumulated = accumulated.isEmpty() ? segment : accumulated + QStringLiteral("/") + segment;
        const QString directoryPath = QDir(destinationRoot).filePath(accumulated);
        if (!QFileInfo::exists(directoryPath))
        {
            if (!QDir().mkdir(directoryPath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create model package install directory"));
            if (!createdDirectories.contains(directoryPath)) createdDirectories.append(directoryPath);
        }
        else if (!QFileInfo(directoryPath).isDir())
        {
            return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Model package install path collides with a file"));
        }
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> CopyPackageFileForInstall(const QString &sourceRoot, const QString &destinationRoot, const QString &relativePath, QStringList &copiedFiles, QStringList &createdDirectories)
{
    if (!IsSafeRelativePath(relativePath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package install path is unsafe"));
    const QString normalized = QDir::cleanPath(relativePath);
    const QString sourcePath = QDir(sourceRoot).filePath(normalized);
    const QString destinationPath = QDir(destinationRoot).filePath(normalized);
    if (!QFileInfo(sourcePath).isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Model package install source file is missing"));
    if (QFileInfo::exists(destinationPath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package install destination file already exists"));
    const auto directoryCreated = EnsureInstallDirectory(destinationRoot, QFileInfo(normalized).path(), createdDirectories);
    if (!directoryCreated.IsSuccess()) return directoryCreated;
    if (!QFile::copy(sourcePath, destinationPath)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to copy model package file during install"));
    copiedFiles.append(destinationPath);
    return foundation::Result<void>::Success();
}

bool ShapeArrayMatches(const QJsonArray &actual, const std::vector<int64_t> &expected)
{
    if (actual.size() != static_cast<qsizetype>(expected.size())) return false;
    for (qsizetype index = 0; index < actual.size(); ++index)
    {
        if (static_cast<int64_t>(actual.at(index).toDouble(-1.0)) != expected.at(static_cast<size_t>(index))) return false;
    }
    return true;
}

foundation::Result<void> VerifyStaticShapeProfile(const QJsonObject &profile, const std::vector<int64_t> &expected)
{
    if (profile.value(QStringLiteral("dynamicDimensions")).toBool(true)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package dynamic shape declaration is unsupported for this profile"));
    if (!ShapeArrayMatches(profile.value(QStringLiteral("min")).toArray(), expected) || !ShapeArrayMatches(profile.value(QStringLiteral("opt")).toArray(), expected) || !ShapeArrayMatches(profile.value(QStringLiteral("max")).toArray(), expected)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package shape profile does not match the declared tensor contract"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> VerifyClasses(const QJsonObject &packageObject, const QJsonObject &labelsObject, const int64_t classCount)
{
    const QJsonArray manifestClasses = packageObject.value(QStringLiteral("classes")).toArray();
    const QJsonArray labels = labelsObject.value(QStringLiteral("classes")).toArray();
    if (classCount <= 0 || manifestClasses.size() != classCount || labels.size() != classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package class labels do not match the output contract"));
    for (qsizetype index = 0; index < labels.size(); ++index)
    {
        if (labels.at(index).toString().isEmpty() || labels.at(index).toString() != manifestClasses.at(index).toString()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package labels.json does not match package.json classes"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> VerifyClassificationContract(const QJsonObject &packageObject, const QJsonObject &labelsObject, const QJsonObject &preprocessingObject, const QJsonObject &postprocessingObject)
{
    const QJsonObject input = packageObject.value(QStringLiteral("inputContract")).toObject();
    const QJsonObject output = packageObject.value(QStringLiteral("outputContract")).toObject();
    const int64_t featureCount = static_cast<int64_t>(input.value(QStringLiteral("featureCount")).toDouble(-1.0));
    const int64_t classCount = static_cast<int64_t>(output.value(QStringLiteral("classCount")).toDouble(-1.0));
    const auto classesVerified = VerifyClasses(packageObject, labelsObject, classCount);
    if (!classesVerified.IsSuccess()) return classesVerified;
    if (packageObject.value(QStringLiteral("decoderId")).toString() != QStringLiteral("classification.argmax") || input.value(QStringLiteral("name")).toString() != QStringLiteral("input") || input.value(QStringLiteral("layout")).toString() != QStringLiteral("NC") || input.value(QStringLiteral("dtype")).toString() != QStringLiteral("float32") || featureCount <= 0 || output.value(QStringLiteral("name")).toString() != QStringLiteral("logits") || output.value(QStringLiteral("dtype")).toString() != QStringLiteral("float32") || classCount < 2) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Classification model package tensor contract is invalid"));
    if (preprocessingObject.value(QStringLiteral("inputName")).toString() != input.value(QStringLiteral("name")).toString() || preprocessingObject.value(QStringLiteral("layout")).toString() != input.value(QStringLiteral("layout")).toString() || preprocessingObject.value(QStringLiteral("dtype")).toString() != input.value(QStringLiteral("dtype")).toString() || static_cast<int64_t>(preprocessingObject.value(QStringLiteral("featureCount")).toDouble(-1.0)) != featureCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Classification preprocessing sidecar does not match the input contract"));
    if (postprocessingObject.value(QStringLiteral("decoderId")).toString() != packageObject.value(QStringLiteral("decoderId")).toString() || postprocessingObject.value(QStringLiteral("outputName")).toString() != output.value(QStringLiteral("name")).toString() || static_cast<int64_t>(postprocessingObject.value(QStringLiteral("classCount")).toDouble(-1.0)) != classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Classification postprocessing sidecar does not match the output contract"));
    const auto inputProfile = VerifyStaticShapeProfile(input.value(QStringLiteral("shapeProfile")).toObject(), {1, featureCount});
    if (!inputProfile.IsSuccess()) return inputProfile;
    const auto preprocessingProfile = VerifyStaticShapeProfile(preprocessingObject.value(QStringLiteral("shapeProfile")).toObject(), {1, featureCount});
    if (!preprocessingProfile.IsSuccess()) return preprocessingProfile;
    return VerifyStaticShapeProfile(output.value(QStringLiteral("shapeProfile")).toObject(), {1, classCount});
}

foundation::Result<void> VerifyDetectionContract(const QJsonObject &packageObject, const QJsonObject &labelsObject, const QJsonObject &preprocessingObject, const QJsonObject &postprocessingObject)
{
    const QJsonObject input = packageObject.value(QStringLiteral("inputContract")).toObject();
    const QJsonObject output = packageObject.value(QStringLiteral("outputContract")).toObject();
    const int64_t channels = static_cast<int64_t>(input.value(QStringLiteral("channels")).toDouble(-1.0));
    const int64_t height = static_cast<int64_t>(input.value(QStringLiteral("height")).toDouble(-1.0));
    const int64_t width = static_cast<int64_t>(input.value(QStringLiteral("width")).toDouble(-1.0));
    const int64_t rowCount = static_cast<int64_t>(output.value(QStringLiteral("rowCount")).toDouble(-1.0));
    const int64_t classCount = static_cast<int64_t>(output.value(QStringLiteral("classCount")).toDouble(-1.0));
    const int64_t rowWidth = static_cast<int64_t>(output.value(QStringLiteral("rowWidth")).toDouble(-1.0));
    const auto classesVerified = VerifyClasses(packageObject, labelsObject, classCount);
    if (!classesVerified.IsSuccess()) return classesVerified;
    if (packageObject.value(QStringLiteral("decoderId")).toString() != QStringLiteral("yolo11.center_nms") || input.value(QStringLiteral("name")).toString() != QStringLiteral("image") || input.value(QStringLiteral("layout")).toString() != QStringLiteral("NCHW") || input.value(QStringLiteral("dtype")).toString() != QStringLiteral("float32") || channels <= 0 || height <= 0 || width <= 0 || output.value(QStringLiteral("name")).toString() != QStringLiteral("rawHead") || output.value(QStringLiteral("layout")).toString() != QStringLiteral("NRC") || output.value(QStringLiteral("dtype")).toString() != QStringLiteral("float32") || rowCount <= 0 || classCount <= 0 || rowWidth != 4 + classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Detection model package tensor contract is invalid"));
    if (preprocessingObject.value(QStringLiteral("inputName")).toString() != input.value(QStringLiteral("name")).toString() || preprocessingObject.value(QStringLiteral("layout")).toString() != input.value(QStringLiteral("layout")).toString() || preprocessingObject.value(QStringLiteral("dtype")).toString() != input.value(QStringLiteral("dtype")).toString() || preprocessingObject.value(QStringLiteral("resizePolicy")).toString() != QStringLiteral("letterbox_supported") || static_cast<int64_t>(preprocessingObject.value(QStringLiteral("channels")).toDouble(-1.0)) != channels || static_cast<int64_t>(preprocessingObject.value(QStringLiteral("height")).toDouble(-1.0)) != height || static_cast<int64_t>(preprocessingObject.value(QStringLiteral("width")).toDouble(-1.0)) != width) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Detection preprocessing sidecar does not match the input contract"));
    if (postprocessingObject.value(QStringLiteral("decoderId")).toString() != packageObject.value(QStringLiteral("decoderId")).toString() || postprocessingObject.value(QStringLiteral("outputName")).toString() != output.value(QStringLiteral("name")).toString() || postprocessingObject.value(QStringLiteral("boxFormat")).toString() != QStringLiteral("center_xywh") || static_cast<int64_t>(postprocessingObject.value(QStringLiteral("rowCount")).toDouble(-1.0)) != rowCount || static_cast<int64_t>(postprocessingObject.value(QStringLiteral("classCount")).toDouble(-1.0)) != classCount || postprocessingObject.value(QStringLiteral("coordinateRestore")).toString() != QStringLiteral("letterbox_inverse_if_present")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Detection postprocessing sidecar does not match the output contract"));
    const auto inputProfile = VerifyStaticShapeProfile(input.value(QStringLiteral("shapeProfile")).toObject(), {1, channels, height, width});
    if (!inputProfile.IsSuccess()) return inputProfile;
    const auto preprocessingProfile = VerifyStaticShapeProfile(preprocessingObject.value(QStringLiteral("shapeProfile")).toObject(), {1, channels, height, width});
    if (!preprocessingProfile.IsSuccess()) return preprocessingProfile;
    return VerifyStaticShapeProfile(output.value(QStringLiteral("shapeProfile")).toObject(), {1, rowCount, rowWidth});
}
}

foundation::Result<void> CreateUnsignedClassificationModelPackage(const QString &packageRoot, const QString &onnxPath, const ClassificationPackageMetadata &metadata)
{
    const auto commonValidation = ValidatePackageMetadataCommon(packageRoot, metadata.packageId, metadata.packageVersion, metadata.adapterId, metadata.adapterVersion, metadata.trainingRunId, metadata.datasetId, metadata.trainingConfigSha256, metadata.sourceCheckpointSha256, metadata.exporterProductVersion, metadata.minSupportedProductVersion, metadata.maxSupportedProductVersion, metadata.licenseId, metadata.licenseName);
    if (!commonValidation.IsSuccess()) return commonValidation;
    if (metadata.inputFeatures <= 0 || metadata.classCount < 2 || metadata.labels.size() != metadata.classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Classification model package metadata is incomplete"));
    if (!QFileInfo(onnxPath).isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Classification ONNX artifact does not exist"));
    if (QFileInfo::exists(packageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package destination already exists and will not be overwritten"));
    QDir directory;
    if (!directory.mkpath(packageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create model package directory"));
    const QString trtRelative = QStringLiteral("artifacts/trt1001_opset12/model.onnx");
    const QString openVinoRelative = QStringLiteral("artifacts/openvino_cpu/model.onnx");
    const QString signatureRelative = QStringLiteral("signature.json");
    const QString pluginRelative = QStringLiteral("plugins/win-x64/plugins.json");
    const QString licenseRelative = QStringLiteral("licenses/license.json");
    if (!directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("artifacts/trt1001_opset12"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("artifacts/openvino_cpu"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("plugins/win-x64"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("licenses")))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create model package directories"));
    if (!QFile::copy(onnxPath, QDir(packageRoot).filePath(trtRelative)) || !QFile::copy(onnxPath, QDir(packageRoot).filePath(openVinoRelative))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to copy ONNX artifacts into the model package"));
    QJsonArray labels;
    for (const QString &label : metadata.labels) labels.append(label);
    QJsonObject package;
    package.insert(QStringLiteral("packageSchemaVersion"), 1);
    package.insert(QStringLiteral("packageId"), metadata.packageId);
    package.insert(QStringLiteral("packageVersion"), metadata.packageVersion);
    package.insert(QStringLiteral("modelFamily"), QStringLiteral("linear_classifier"));
    package.insert(QStringLiteral("adapterId"), metadata.adapterId);
    package.insert(QStringLiteral("adapterVersion"), metadata.adapterVersion);
    package.insert(QStringLiteral("projectType"), QStringLiteral("classification"));
    package.insert(QStringLiteral("decoderId"), QStringLiteral("classification.argmax"));
    package.insert(QStringLiteral("classes"), labels);
    package.insert(QStringLiteral("inputContract"), QJsonObject{{QStringLiteral("name"), QStringLiteral("input")}, {QStringLiteral("layout"), QStringLiteral("NC")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("featureCount"), metadata.inputFeatures}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.inputFeatures})}});
    package.insert(QStringLiteral("outputContract"), QJsonObject{{QStringLiteral("name"), QStringLiteral("logits")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("classCount"), metadata.classCount}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.classCount})}});
    package.insert(QStringLiteral("artifacts"), QJsonArray{trtRelative, openVinoRelative});
    package.insert(QStringLiteral("runtimeRequirements"), QJsonObject{{QStringLiteral("tensorRt"), QStringLiteral("10.0.1.6")}, {QStringLiteral("cuda"), QStringLiteral("11.8")}, {QStringLiteral("openVino"), QStringLiteral("2025.3.0")}});
    package.insert(QStringLiteral("trainingProvenance"), TrainingProvenance(metadata.trainingRunId, metadata.datasetId, metadata.trainingConfigSha256, metadata.sourceCheckpointSha256, metadata.exporterProductVersion));
    package.insert(QStringLiteral("supportedProductRange"), SupportedProductRange(metadata.minSupportedProductVersion, metadata.maxSupportedProductVersion));
    package.insert(QStringLiteral("pluginRequirements"), QJsonArray{});
    package.insert(QStringLiteral("licenseMetadata"), LicenseMetadata(metadata.licenseId, metadata.licenseName));
    const auto packageWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("package.json")), package);
    if (!packageWritten.IsSuccess()) return packageWritten;
    const auto labelsWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("labels.json")), QJsonObject{{QStringLiteral("classes"), labels}});
    if (!labelsWritten.IsSuccess()) return labelsWritten;
    const auto preprocessingWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("preprocessing.json")), QJsonObject{{QStringLiteral("inputName"), QStringLiteral("input")}, {QStringLiteral("layout"), QStringLiteral("NC")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("featureCount"), metadata.inputFeatures}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.inputFeatures})}});
    if (!preprocessingWritten.IsSuccess()) return preprocessingWritten;
    const auto postprocessingWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("postprocessing.json")), QJsonObject{{QStringLiteral("decoderId"), QStringLiteral("classification.argmax")}, {QStringLiteral("outputName"), QStringLiteral("logits")}, {QStringLiteral("classCount"), metadata.classCount}});
    if (!postprocessingWritten.IsSuccess()) return postprocessingWritten;
    const auto signatureWritten = WriteJson(QDir(packageRoot).filePath(signatureRelative), UnsignedSignatureMetadata());
    if (!signatureWritten.IsSuccess()) return signatureWritten;
    const auto pluginsWritten = WriteJson(QDir(packageRoot).filePath(pluginRelative), EmptyPluginManifest());
    if (!pluginsWritten.IsSuccess()) return pluginsWritten;
    const auto licenseWritten = WriteJson(QDir(packageRoot).filePath(licenseRelative), LicenseSidecar(metadata.licenseId, metadata.licenseName));
    if (!licenseWritten.IsSuccess()) return licenseWritten;
    return WriteChecksums(packageRoot, QStringList{trtRelative, openVinoRelative, QStringLiteral("package.json"), signatureRelative, QStringLiteral("labels.json"), QStringLiteral("preprocessing.json"), QStringLiteral("postprocessing.json"), pluginRelative, licenseRelative});
}

foundation::Result<void> CreateUnsignedYolo11DetectionModelPackage(const QString &packageRoot, const QString &onnxPath, const Yolo11DetectionPackageMetadata &metadata)
{
    const auto commonValidation = ValidatePackageMetadataCommon(packageRoot, metadata.packageId, metadata.packageVersion, metadata.adapterId, metadata.adapterVersion, metadata.trainingRunId, metadata.datasetId, metadata.trainingConfigSha256, metadata.sourceCheckpointSha256, metadata.exporterProductVersion, metadata.minSupportedProductVersion, metadata.maxSupportedProductVersion, metadata.licenseId, metadata.licenseName);
    if (!commonValidation.IsSuccess()) return commonValidation;
    if (metadata.inputChannels <= 0 || metadata.imageHeight <= 0 || metadata.imageWidth <= 0 || metadata.rowCount <= 0 || metadata.classCount <= 0 || metadata.labels.size() != metadata.classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 detection model package metadata is incomplete"));
    if (!QFileInfo(onnxPath).isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "YOLO11 detection ONNX artifact does not exist"));
    if (QFileInfo::exists(packageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package destination already exists and will not be overwritten"));
    QDir directory;
    if (!directory.mkpath(packageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create YOLO11 detection model package directory"));
    const QString trtRelative = QStringLiteral("artifacts/trt1001_opset12/model.onnx");
    const QString openVinoRelative = QStringLiteral("artifacts/openvino_cpu/model.onnx");
    const QString signatureRelative = QStringLiteral("signature.json");
    const QString pluginRelative = QStringLiteral("plugins/win-x64/plugins.json");
    const QString licenseRelative = QStringLiteral("licenses/license.json");
    if (!directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("artifacts/trt1001_opset12"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("artifacts/openvino_cpu"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("plugins/win-x64"))) || !directory.mkpath(QDir(packageRoot).filePath(QStringLiteral("licenses")))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create YOLO11 detection package directories"));
    if (!QFile::copy(onnxPath, QDir(packageRoot).filePath(trtRelative)) || !QFile::copy(onnxPath, QDir(packageRoot).filePath(openVinoRelative))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to copy YOLO11 detection ONNX artifacts into the model package"));
    QJsonArray labels;
    for (const QString &label : metadata.labels) labels.append(label);
    QJsonObject package;
    package.insert(QStringLiteral("packageSchemaVersion"), 1);
    package.insert(QStringLiteral("packageId"), metadata.packageId);
    package.insert(QStringLiteral("packageVersion"), metadata.packageVersion);
    package.insert(QStringLiteral("modelFamily"), QStringLiteral("yolo11_detection"));
    package.insert(QStringLiteral("adapterId"), metadata.adapterId);
    package.insert(QStringLiteral("adapterVersion"), metadata.adapterVersion);
    package.insert(QStringLiteral("projectType"), QStringLiteral("detection"));
    package.insert(QStringLiteral("decoderId"), QStringLiteral("yolo11.center_nms"));
    package.insert(QStringLiteral("classes"), labels);
    package.insert(QStringLiteral("inputContract"), QJsonObject{{QStringLiteral("name"), QStringLiteral("image")}, {QStringLiteral("layout"), QStringLiteral("NCHW")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("channels"), metadata.inputChannels}, {QStringLiteral("height"), metadata.imageHeight}, {QStringLiteral("width"), metadata.imageWidth}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.inputChannels, metadata.imageHeight, metadata.imageWidth})}});
    package.insert(QStringLiteral("outputContract"), QJsonObject{{QStringLiteral("name"), QStringLiteral("rawHead")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("layout"), QStringLiteral("NRC")}, {QStringLiteral("rowCount"), metadata.rowCount}, {QStringLiteral("classCount"), metadata.classCount}, {QStringLiteral("rowWidth"), 4 + metadata.classCount}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.rowCount, 4 + metadata.classCount})}});
    package.insert(QStringLiteral("postprocessing"), QJsonObject{{QStringLiteral("boxFormat"), QStringLiteral("center_xywh")}, {QStringLiteral("decoder"), QStringLiteral("cpp_yolo11_center_nms")}, {QStringLiteral("nms"), QStringLiteral("per_class_or_class_agnostic_configurable")}});
    package.insert(QStringLiteral("artifacts"), QJsonArray{trtRelative, openVinoRelative});
    package.insert(QStringLiteral("runtimeRequirements"), QJsonObject{{QStringLiteral("tensorRt"), QStringLiteral("10.0.1.6")}, {QStringLiteral("cuda"), QStringLiteral("11.8")}, {QStringLiteral("openVino"), QStringLiteral("2025.3.0")}});
    package.insert(QStringLiteral("trainingProvenance"), TrainingProvenance(metadata.trainingRunId, metadata.datasetId, metadata.trainingConfigSha256, metadata.sourceCheckpointSha256, metadata.exporterProductVersion));
    package.insert(QStringLiteral("supportedProductRange"), SupportedProductRange(metadata.minSupportedProductVersion, metadata.maxSupportedProductVersion));
    package.insert(QStringLiteral("pluginRequirements"), QJsonArray{});
    package.insert(QStringLiteral("licenseMetadata"), LicenseMetadata(metadata.licenseId, metadata.licenseName));
    const auto packageWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("package.json")), package);
    if (!packageWritten.IsSuccess()) return packageWritten;
    const auto labelsWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("labels.json")), QJsonObject{{QStringLiteral("classes"), labels}});
    if (!labelsWritten.IsSuccess()) return labelsWritten;
    const auto preprocessingWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("preprocessing.json")), QJsonObject{{QStringLiteral("inputName"), QStringLiteral("image")}, {QStringLiteral("layout"), QStringLiteral("NCHW")}, {QStringLiteral("dtype"), QStringLiteral("float32")}, {QStringLiteral("channels"), metadata.inputChannels}, {QStringLiteral("height"), metadata.imageHeight}, {QStringLiteral("width"), metadata.imageWidth}, {QStringLiteral("resizePolicy"), QStringLiteral("letterbox_supported")}, {QStringLiteral("shapeProfile"), StaticShapeProfile({1, metadata.inputChannels, metadata.imageHeight, metadata.imageWidth})}});
    if (!preprocessingWritten.IsSuccess()) return preprocessingWritten;
    const auto postprocessingWritten = WriteJson(QDir(packageRoot).filePath(QStringLiteral("postprocessing.json")), QJsonObject{{QStringLiteral("decoderId"), QStringLiteral("yolo11.center_nms")}, {QStringLiteral("outputName"), QStringLiteral("rawHead")}, {QStringLiteral("boxFormat"), QStringLiteral("center_xywh")}, {QStringLiteral("rowCount"), metadata.rowCount}, {QStringLiteral("classCount"), metadata.classCount}, {QStringLiteral("coordinateRestore"), QStringLiteral("letterbox_inverse_if_present")}});
    if (!postprocessingWritten.IsSuccess()) return postprocessingWritten;
    const auto signatureWritten = WriteJson(QDir(packageRoot).filePath(signatureRelative), UnsignedSignatureMetadata());
    if (!signatureWritten.IsSuccess()) return signatureWritten;
    const auto pluginsWritten = WriteJson(QDir(packageRoot).filePath(pluginRelative), EmptyPluginManifest());
    if (!pluginsWritten.IsSuccess()) return pluginsWritten;
    const auto licenseWritten = WriteJson(QDir(packageRoot).filePath(licenseRelative), LicenseSidecar(metadata.licenseId, metadata.licenseName));
    if (!licenseWritten.IsSuccess()) return licenseWritten;
    return WriteChecksums(packageRoot, QStringList{trtRelative, openVinoRelative, QStringLiteral("package.json"), signatureRelative, QStringLiteral("labels.json"), QStringLiteral("preprocessing.json"), QStringLiteral("postprocessing.json"), pluginRelative, licenseRelative});
}

foundation::Result<void> VerifyModelPackage(const QString &packageRoot, const bool requireSignature)
{
    const QFileInfo rootInfo(packageRoot);
    if (!rootInfo.isDir()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Model package directory does not exist"));
    const auto package = ReadObject(QDir(packageRoot).filePath(QStringLiteral("package.json")));
    if (!package.IsSuccess()) return foundation::Result<void>::Failure(package.Failure());
    const QJsonObject packageObject = package.Value();
    const QString projectType = packageObject.value(QStringLiteral("projectType")).toString();
    if (packageObject.value(QStringLiteral("packageSchemaVersion")).toInt() != 1 || packageObject.value(QStringLiteral("packageId")).toString().isEmpty() || packageObject.value(QStringLiteral("packageVersion")).toString().isEmpty() || packageObject.value(QStringLiteral("modelFamily")).toString().isEmpty() || packageObject.value(QStringLiteral("adapterId")).toString().isEmpty() || packageObject.value(QStringLiteral("adapterVersion")).toString().isEmpty() || (projectType != QStringLiteral("classification") && projectType != QStringLiteral("detection")) || !packageObject.value(QStringLiteral("artifacts")).isArray() || !packageObject.value(QStringLiteral("inputContract")).isObject() || !packageObject.value(QStringLiteral("outputContract")).isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Model package manifest does not satisfy a supported package schema"));
    const auto manifestCommon = VerifyPackageManifestCommon(packageObject);
    if (!manifestCommon.IsSuccess()) return manifestCommon;
    const auto checksums = ReadObject(QDir(packageRoot).filePath(QStringLiteral("checksums.json")));
    if (!checksums.IsSuccess()) return foundation::Result<void>::Failure(checksums.Failure());
    for (const QString &relativePath : {QStringLiteral("package.json"), QStringLiteral("signature.json"), QStringLiteral("labels.json"), QStringLiteral("preprocessing.json"), QStringLiteral("postprocessing.json"), QStringLiteral("plugins/win-x64/plugins.json"), QStringLiteral("licenses/license.json")})
    {
        const auto verified = VerifyChecksummedFile(packageRoot, relativePath, checksums.Value());
        if (!verified.IsSuccess()) return verified;
    }
    for (const QJsonValue &artifact : packageObject.value(QStringLiteral("artifacts")).toArray())
    {
        const QString relativePath = artifact.toString();
        const auto verified = VerifyChecksummedFile(packageRoot, relativePath, checksums.Value());
        if (!verified.IsSuccess()) return verified;
    }
    const auto labels = ReadObject(QDir(packageRoot).filePath(QStringLiteral("labels.json")));
    if (!labels.IsSuccess()) return foundation::Result<void>::Failure(labels.Failure());
    const auto preprocessing = ReadObject(QDir(packageRoot).filePath(QStringLiteral("preprocessing.json")));
    if (!preprocessing.IsSuccess()) return foundation::Result<void>::Failure(preprocessing.Failure());
    const auto postprocessing = ReadObject(QDir(packageRoot).filePath(QStringLiteral("postprocessing.json")));
    if (!postprocessing.IsSuccess()) return foundation::Result<void>::Failure(postprocessing.Failure());
    const auto signature = ReadObject(QDir(packageRoot).filePath(QStringLiteral("signature.json")));
    if (!signature.IsSuccess()) return foundation::Result<void>::Failure(signature.Failure());
    const auto plugins = ReadObject(QDir(packageRoot).filePath(QStringLiteral("plugins/win-x64/plugins.json")));
    if (!plugins.IsSuccess()) return foundation::Result<void>::Failure(plugins.Failure());
    const auto license = ReadObject(QDir(packageRoot).filePath(QStringLiteral("licenses/license.json")));
    if (!license.IsSuccess()) return foundation::Result<void>::Failure(license.Failure());
    const auto sidecarsVerified = VerifyPackageSidecarsCommon(packageObject, signature.Value(), plugins.Value(), license.Value(), requireSignature);
    if (!sidecarsVerified.IsSuccess()) return sidecarsVerified;
    const auto contractVerified = projectType == QStringLiteral("classification") ? VerifyClassificationContract(packageObject, labels.Value(), preprocessing.Value(), postprocessing.Value()) : VerifyDetectionContract(packageObject, labels.Value(), preprocessing.Value(), postprocessing.Value());
    if (!contractVerified.IsSuccess()) return contractVerified;
    return foundation::Result<void>::Success();
}

foundation::Result<void> InstallModelPackage(const QString &sourcePackageRoot, const QString &destinationPackageRoot, const bool requireSignature)
{
    if (sourcePackageRoot.isEmpty() || destinationPackageRoot.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package install paths are incomplete"));
    if (!QFileInfo(sourcePackageRoot).isDir()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Model package install source directory does not exist"));
    if (QFileInfo::exists(destinationPackageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model package install destination already exists and will not be overwritten"));
    const auto sourceVerified = VerifyModelPackage(sourcePackageRoot, requireSignature);
    if (!sourceVerified.IsSuccess()) return sourceVerified;
    const auto checksums = ReadObject(QDir(sourcePackageRoot).filePath(QStringLiteral("checksums.json")));
    if (!checksums.IsSuccess()) return foundation::Result<void>::Failure(checksums.Failure());
    QDir directory;
    if (!directory.mkdir(destinationPackageRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create model package install destination"));
    QStringList createdDirectories{destinationPackageRoot};
    QStringList copiedFiles;
    QStringList relativeFiles{QStringLiteral("checksums.json")};
    const QJsonObject checksumsObject = checksums.Value();
    for (auto iterator = checksumsObject.begin(); iterator != checksumsObject.end(); ++iterator)
    {
        if (!relativeFiles.contains(iterator.key())) relativeFiles.append(iterator.key());
    }
    for (const QString &relativePath : relativeFiles)
    {
        const auto copied = CopyPackageFileForInstall(sourcePackageRoot, destinationPackageRoot, relativePath, copiedFiles, createdDirectories);
        if (!copied.IsSuccess())
        {
            RollbackPackageInstall(copiedFiles, createdDirectories);
            return copied;
        }
    }
    const auto destinationVerified = VerifyModelPackage(destinationPackageRoot, requireSignature);
    if (!destinationVerified.IsSuccess())
    {
        RollbackPackageInstall(copiedFiles, createdDirectories);
        return destinationVerified;
    }
    return foundation::Result<void>::Success();
}
}
