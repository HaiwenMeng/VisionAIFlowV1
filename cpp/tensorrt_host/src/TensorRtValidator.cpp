#include "visionaiflow/tensorrt_host/TensorRtValidator.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <NvInferVersion.h>
#include <cuda_runtime_api.h>

#include <QByteArray>

#include <functional>
#include <memory>
#include <string>

namespace visionaiflow::tensorrt_host
{
namespace
{
class TensorRtLogger final : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char *message) noexcept override
    {
        if (severity <= Severity::kWARNING) m_lastError = message == nullptr ? "TensorRT emitted an empty diagnostic" : message;
    }

    [[nodiscard]] const std::string &LastError() const noexcept { return m_lastError; }

private:
    std::string m_lastError;
};

std::string ParserErrors(const nvonnxparser::IParser &parser)
{
    std::string errors;
    for (int index = 0; index < parser.getNbErrors(); ++index)
    {
        const nvonnxparser::IParserError *error = parser.getError(index);
        if (error == nullptr) continue;
        if (!errors.empty()) errors.append(" | ");
        errors.append(error->desc());
    }
    return errors.empty() ? "TensorRT ONNX parser did not report a diagnostic" : errors;
}

struct EngineCacheRequest final
{
    QString modelKind;
    QString onnxPath;
    QByteArray onnxSha256;
    QString tensorRtVersion;
    QString cudaRuntimeVersion;
    QString gpuUuid;
    int computeCapabilityMajor{0};
    int computeCapabilityMinor{0};
    QString precision;
    QVector<int64_t> minShape;
    QVector<int64_t> optShape;
    QVector<int64_t> maxShape;
    QString builderFlags;
};

struct EngineCacheEntry final
{
    QString directory;
    QString enginePath;
    QString manifestPath;
    QString cacheKey;
};

struct CachedSerializedEngine final
{
    QByteArray bytes;
    EngineCacheEntry entry;
    bool fromCache{false};
};

QJsonArray ShapeArray(const QVector<int64_t> &shape)
{
    QJsonArray values;
    for (const int64_t dimension : shape) values.append(static_cast<qint64>(dimension));
    return values;
}

QString ShapeKey(const QVector<int64_t> &shape)
{
    QStringList parts;
    for (const int64_t dimension : shape) parts.append(QString::number(dimension));
    return parts.join(QStringLiteral("x"));
}

foundation::Result<QByteArray> Sha256File(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read TensorRT cache input file: ").append(file.errorString()).toStdString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
    {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty() && file.error() != QFile::NoError) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to hash TensorRT cache input file: ").append(file.errorString()).toStdString()));
        hash.addData(block);
    }
    return foundation::Result<QByteArray>::Success(hash.result().toHex());
}

QByteArray Sha256Bytes(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

QString EngineCacheRoot()
{
    const QString overrideRoot = qEnvironmentVariable("VISIONAIFLOW_ENGINE_CACHE_ROOT");
    if (!overrideRoot.isEmpty()) return QDir::cleanPath(overrideRoot);
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) return QDir(localAppData).filePath(QStringLiteral("VisionAIFlowV1/engine-cache"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("engine-cache"));
}

QString TensorRtVersionString()
{
    return QStringLiteral("%1.%2.%3.%4").arg(NV_TENSORRT_MAJOR).arg(NV_TENSORRT_MINOR).arg(NV_TENSORRT_PATCH).arg(NV_TENSORRT_BUILD);
}

foundation::Result<QString> CudaRuntimeVersionString()
{
    int version = 0;
    const cudaError_t status = cudaRuntimeGetVersion(&version);
    if (status != cudaSuccess) return foundation::Result<QString>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("CUDA runtime version query failed: ") + cudaGetErrorString(status)));
    return foundation::Result<QString>::Success(QStringLiteral("%1.%2").arg(version / 1000).arg((version % 1000) / 10));
}

QString DeviceUuidString(const cudaUUID_t &uuid)
{
    QString value;
    for (int index = 0; index < 16; ++index)
    {
        if (index == 4 || index == 6 || index == 8 || index == 10) value.append(QChar('-'));
        value.append(QStringLiteral("%1").arg(static_cast<unsigned char>(uuid.bytes[index]), 2, 16, QChar('0')));
    }
    return value;
}

foundation::Result<void> FillCudaDeviceFields(EngineCacheRequest *request)
{
    int device = 0;
    cudaError_t status = cudaGetDevice(&device);
    if (status != cudaSuccess) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("CUDA current device query failed: ") + cudaGetErrorString(status)));
    cudaDeviceProp properties{};
    status = cudaGetDeviceProperties(&properties, device);
    if (status != cudaSuccess) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("CUDA device property query failed: ") + cudaGetErrorString(status)));
    request->gpuUuid = DeviceUuidString(properties.uuid);
    request->computeCapabilityMajor = properties.major;
    request->computeCapabilityMinor = properties.minor;
    return foundation::Result<void>::Success();
}

foundation::Result<EngineCacheRequest> CreateEngineCacheRequest(const QString &modelKind, const QString &onnxPath, const QVector<int64_t> &shape)
{
    const auto onnxHash = Sha256File(onnxPath);
    if (!onnxHash.IsSuccess()) return foundation::Result<EngineCacheRequest>::Failure(onnxHash.Failure());
    const auto cudaVersion = CudaRuntimeVersionString();
    if (!cudaVersion.IsSuccess()) return foundation::Result<EngineCacheRequest>::Failure(cudaVersion.Failure());
    EngineCacheRequest request;
    request.modelKind = modelKind;
    request.onnxPath = onnxPath;
    request.onnxSha256 = onnxHash.Value();
    request.tensorRtVersion = TensorRtVersionString();
    request.cudaRuntimeVersion = cudaVersion.Value();
    request.precision = QStringLiteral("fp32");
    request.minShape = shape;
    request.optShape = shape;
    request.maxShape = shape;
    request.builderFlags = QStringLiteral("workspace=1073741824;explicitBatch=true");
    const auto device = FillCudaDeviceFields(&request);
    if (!device.IsSuccess()) return foundation::Result<EngineCacheRequest>::Failure(device.Failure());
    return foundation::Result<EngineCacheRequest>::Success(request);
}

QString CacheKeyText(const EngineCacheRequest &request)
{
    return QStringList{
        request.modelKind,
        QString::fromLatin1(request.onnxSha256),
        request.tensorRtVersion,
        request.cudaRuntimeVersion,
        request.gpuUuid,
        QString::number(request.computeCapabilityMajor) + QStringLiteral(".") + QString::number(request.computeCapabilityMinor),
        request.precision,
        ShapeKey(request.minShape),
        ShapeKey(request.optShape),
        ShapeKey(request.maxShape),
        request.builderFlags}.join(QStringLiteral("|"));
}

EngineCacheEntry CacheEntryForRequest(const EngineCacheRequest &request)
{
    const QString key = QString::fromLatin1(QCryptographicHash::hash(CacheKeyText(request).toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString directory = QDir(EngineCacheRoot()).filePath(key);
    return {directory, QDir(directory).filePath(QStringLiteral("engine.plan")), QDir(directory).filePath(QStringLiteral("manifest.json")), key};
}

QJsonObject ManifestForRequest(const EngineCacheRequest &request, const EngineCacheEntry &entry, const QByteArray &engineBytes)
{
    return QJsonObject{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("cacheKey"), entry.cacheKey},
        {QStringLiteral("modelKind"), request.modelKind},
        {QStringLiteral("modelSha256"), QString::fromLatin1(request.onnxSha256)},
        {QStringLiteral("onnxSha256"), QString::fromLatin1(request.onnxSha256)},
        {QStringLiteral("engineFile"), QStringLiteral("engine.plan")},
        {QStringLiteral("engineSha256"), QString::fromLatin1(Sha256Bytes(engineBytes))},
        {QStringLiteral("tensorRtVersion"), request.tensorRtVersion},
        {QStringLiteral("cudaRuntimeVersion"), request.cudaRuntimeVersion},
        {QStringLiteral("gpuUuid"), request.gpuUuid},
        {QStringLiteral("computeCapability"), QStringLiteral("%1.%2").arg(request.computeCapabilityMajor).arg(request.computeCapabilityMinor)},
        {QStringLiteral("precision"), request.precision},
        {QStringLiteral("minShape"), ShapeArray(request.minShape)},
        {QStringLiteral("optShape"), ShapeArray(request.optShape)},
        {QStringLiteral("maxShape"), ShapeArray(request.maxShape)},
        {QStringLiteral("builderFlags"), request.builderFlags}};
}

bool ShapeMatches(const QJsonArray &actual, const QVector<int64_t> &expected)
{
    if (actual.size() != expected.size()) return false;
    for (qsizetype index = 0; index < actual.size(); ++index)
    {
        if (static_cast<int64_t>(actual.at(index).toDouble(-1.0)) != expected.at(index)) return false;
    }
    return true;
}

bool ManifestMatchesRequest(const QJsonObject &manifest, const EngineCacheRequest &request, const EngineCacheEntry &entry, const QByteArray &engineBytes)
{
    return manifest.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
        manifest.value(QStringLiteral("cacheKey")).toString() == entry.cacheKey &&
        manifest.value(QStringLiteral("modelKind")).toString() == request.modelKind &&
        manifest.value(QStringLiteral("onnxSha256")).toString() == QString::fromLatin1(request.onnxSha256) &&
        manifest.value(QStringLiteral("modelSha256")).toString() == QString::fromLatin1(request.onnxSha256) &&
        manifest.value(QStringLiteral("engineFile")).toString() == QStringLiteral("engine.plan") &&
        manifest.value(QStringLiteral("engineSha256")).toString().compare(QString::fromLatin1(Sha256Bytes(engineBytes)), Qt::CaseInsensitive) == 0 &&
        manifest.value(QStringLiteral("tensorRtVersion")).toString() == request.tensorRtVersion &&
        manifest.value(QStringLiteral("cudaRuntimeVersion")).toString() == request.cudaRuntimeVersion &&
        manifest.value(QStringLiteral("gpuUuid")).toString() == request.gpuUuid &&
        manifest.value(QStringLiteral("computeCapability")).toString() == QStringLiteral("%1.%2").arg(request.computeCapabilityMajor).arg(request.computeCapabilityMinor) &&
        manifest.value(QStringLiteral("precision")).toString() == request.precision &&
        ShapeMatches(manifest.value(QStringLiteral("minShape")).toArray(), request.minShape) &&
        ShapeMatches(manifest.value(QStringLiteral("optShape")).toArray(), request.optShape) &&
        ShapeMatches(manifest.value(QStringLiteral("maxShape")).toArray(), request.maxShape) &&
        manifest.value(QStringLiteral("builderFlags")).toString() == request.builderFlags;
}

bool ReadCachedSerializedEngine(const EngineCacheRequest &request, const EngineCacheEntry &entry, QByteArray *bytes, QString *reason)
{
    QFile engineFile(entry.enginePath);
    if (!engineFile.open(QIODevice::ReadOnly)) { if (reason != nullptr) *reason = engineFile.errorString(); return false; }
    const QByteArray engineBytes = engineFile.readAll();
    if (engineBytes.isEmpty()) { if (reason != nullptr) *reason = QStringLiteral("cached engine file is empty"); return false; }
    QFile manifestFile(entry.manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) { if (reason != nullptr) *reason = manifestFile.errorString(); return false; }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { if (reason != nullptr) *reason = QStringLiteral("cache manifest JSON is invalid"); return false; }
    if (!ManifestMatchesRequest(document.object(), request, entry, engineBytes)) { if (reason != nullptr) *reason = QStringLiteral("cache manifest does not match the current engine request"); return false; }
    *bytes = engineBytes;
    return true;
}

foundation::Result<void> WriteCachedSerializedEngine(const EngineCacheRequest &request, const EngineCacheEntry &entry, const QByteArray &engineBytes)
{
    if (engineBytes.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT serialized engine cache bytes are empty"));
    QDir directory;
    if (!directory.mkpath(entry.directory)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create TensorRT engine cache directory"));
    QSaveFile engineFile(entry.enginePath);
    if (!engineFile.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open TensorRT cache engine output: ").append(engineFile.errorString()).toStdString()));
    if (engineFile.write(engineBytes) != engineBytes.size() || !engineFile.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically write TensorRT cache engine: ").append(engineFile.errorString()).toStdString()));
    const QByteArray manifestBytes = QJsonDocument(ManifestForRequest(request, entry, engineBytes)).toJson(QJsonDocument::Indented);
    QSaveFile manifestFile(entry.manifestPath);
    if (!manifestFile.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open TensorRT cache manifest output: ").append(manifestFile.errorString()).toStdString()));
    if (manifestFile.write(manifestBytes) != manifestBytes.size() || !manifestFile.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically write TensorRT cache manifest: ").append(manifestFile.errorString()).toStdString()));
    return foundation::Result<void>::Success();
}

void DeleteSingleCacheEntry(const EngineCacheEntry &entry)
{
    QFile::remove(entry.enginePath);
    QFile::remove(entry.manifestPath);
}

foundation::Result<CachedSerializedEngine> BuildOrLoadSerializedEngine(const EngineCacheRequest &request, const std::function<foundation::Result<QByteArray>()> &builder)
{
    const EngineCacheEntry entry = CacheEntryForRequest(request);
    QByteArray cachedBytes;
    QString cacheMissReason;
    if (ReadCachedSerializedEngine(request, entry, &cachedBytes, &cacheMissReason)) return foundation::Result<CachedSerializedEngine>::Success({cachedBytes, entry, true});
    const auto built = builder();
    if (!built.IsSuccess()) return foundation::Result<CachedSerializedEngine>::Failure(built.Failure());
    const auto written = WriteCachedSerializedEngine(request, entry, built.Value());
    if (!written.IsSuccess()) return foundation::Result<CachedSerializedEngine>::Failure(written.Failure());
    return foundation::Result<CachedSerializedEngine>::Success({built.Value(), entry, false});
}

foundation::Result<QByteArray> BuildSerializedClassificationEngine(const QString &onnxPath, const int64_t featureCount)
{
    if (onnxPath.isEmpty() || featureCount <= 0) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "TensorRT ONNX path and feature count must be valid"));
    const QFileInfo fileInfo(onnxPath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "TensorRT ONNX model file does not exist or is not readable"));
    try
    {
        TensorRtLogger logger;
        std::unique_ptr<nvinfer1::IBuilder> builder(nvinfer1::createInferBuilder(logger));
        if (!builder) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT builder creation failed"));
        const uint32_t explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        std::unique_ptr<nvinfer1::INetworkDefinition> network(builder->createNetworkV2(explicitBatch));
        if (!network) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT explicit-batch network creation failed"));
        std::unique_ptr<nvonnxparser::IParser> parser(nvonnxparser::createParser(*network, logger));
        if (!parser) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT ONNX parser creation failed"));
        if (!parser->parseFromFile(onnxPath.toUtf8().constData(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT ONNX parsing failed: " + ParserErrors(*parser)));
        if (network->getNbInputs() != 1 || network->getNbOutputs() != 1) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "TensorRT classification model must have exactly one input and one output"));
        nvinfer1::ITensor *input = network->getInput(0);
        if (input == nullptr || input->getType() != nvinfer1::DataType::kFLOAT) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT classification input must be float32"));
        nvinfer1::Dims dimensions = input->getDimensions();
        if (dimensions.nbDims != 2 || (dimensions.d[1] >= 0 && dimensions.d[1] != featureCount)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT classification input shape does not match the requested feature count"));
        dimensions.d[0] = 1;
        dimensions.d[1] = featureCount;
        nvinfer1::IOptimizationProfile *profile = builder->createOptimizationProfile();
        if (!profile || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMIN, dimensions) || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kOPT, dimensions) || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMAX, dimensions)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT optimization profile creation failed"));
        std::unique_ptr<nvinfer1::IBuilderConfig> configuration(builder->createBuilderConfig());
        if (!configuration) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT builder configuration creation failed"));
        configuration->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 1ULL << 30U);
        if (configuration->addOptimizationProfile(profile) < 0) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT could not attach the optimization profile"));
        std::unique_ptr<nvinfer1::IHostMemory> serialized(builder->buildSerializedNetwork(*network, *configuration));
        if (!serialized || serialized->size() <= 0U) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, logger.LastError().empty() ? "TensorRT engine construction failed" : "TensorRT engine construction failed: " + logger.LastError()));
        return foundation::Result<QByteArray>::Success(QByteArray(static_cast<const char *>(serialized->data()), static_cast<qsizetype>(serialized->size())));
    }
    catch (const std::exception &error) { return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, std::string("TensorRT engine build failed: ") + error.what())); }
}

foundation::Result<QByteArray> BuildSerializedYolo11Engine(const QString &onnxPath, const int channels, const int height, const int width)
{
    if (onnxPath.isEmpty() || channels <= 0 || height <= 0 || width <= 0) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "TensorRT YOLO11 ONNX path and image dimensions must be valid"));
    const QFileInfo fileInfo(onnxPath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "TensorRT YOLO11 ONNX model file does not exist or is not readable"));
    try
    {
        TensorRtLogger logger;
        std::unique_ptr<nvinfer1::IBuilder> builder(nvinfer1::createInferBuilder(logger));
        if (!builder) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT builder creation failed"));
        const uint32_t explicitBatch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        std::unique_ptr<nvinfer1::INetworkDefinition> network(builder->createNetworkV2(explicitBatch));
        if (!network) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT explicit-batch YOLO11 network creation failed"));
        std::unique_ptr<nvonnxparser::IParser> parser(nvonnxparser::createParser(*network, logger));
        if (!parser) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT YOLO11 ONNX parser creation failed"));
        if (!parser->parseFromFile(onnxPath.toUtf8().constData(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 ONNX parsing failed: " + ParserErrors(*parser)));
        if (network->getNbInputs() != 1 || network->getNbOutputs() != 1) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "TensorRT YOLO11 model must have exactly one input and one output"));
        nvinfer1::ITensor *input = network->getInput(0);
        nvinfer1::ITensor *output = network->getOutput(0);
        if (input == nullptr || output == nullptr || input->getType() != nvinfer1::DataType::kFLOAT || output->getType() != nvinfer1::DataType::kFLOAT) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 model requires one float32 input and one float32 output"));
        nvinfer1::Dims inputDimensions = input->getDimensions();
        if (inputDimensions.nbDims != 4 || (inputDimensions.d[0] >= 0 && inputDimensions.d[0] != 1) || (inputDimensions.d[1] >= 0 && inputDimensions.d[1] != channels) || (inputDimensions.d[2] >= 0 && inputDimensions.d[2] != height) || (inputDimensions.d[3] >= 0 && inputDimensions.d[3] != width)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 input shape does not match the requested image shape"));
        inputDimensions.d[0] = 1;
        inputDimensions.d[1] = channels;
        inputDimensions.d[2] = height;
        inputDimensions.d[3] = width;
        const nvinfer1::Dims outputDimensions = output->getDimensions();
        if (outputDimensions.nbDims != 3 || (outputDimensions.d[0] >= 0 && outputDimensions.d[0] != 1) || outputDimensions.d[1] == 0 || outputDimensions.d[2] < 5) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 output must have shape [1, rows, 4 + classCount]"));
        nvinfer1::IOptimizationProfile *profile = builder->createOptimizationProfile();
        if (!profile || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMIN, inputDimensions) || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kOPT, inputDimensions) || !profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMAX, inputDimensions)) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT YOLO11 optimization profile creation failed"));
        std::unique_ptr<nvinfer1::IBuilderConfig> configuration(builder->createBuilderConfig());
        if (!configuration) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT YOLO11 builder configuration creation failed"));
        configuration->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 1ULL << 30U);
        if (configuration->addOptimizationProfile(profile) < 0) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT YOLO11 could not attach the optimization profile"));
        std::unique_ptr<nvinfer1::IHostMemory> serialized(builder->buildSerializedNetwork(*network, *configuration));
        if (!serialized || serialized->size() <= 0U) return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, logger.LastError().empty() ? "TensorRT YOLO11 engine construction failed" : "TensorRT YOLO11 engine construction failed: " + logger.LastError()));
        return foundation::Result<QByteArray>::Success(QByteArray(static_cast<const char *>(serialized->data()), static_cast<qsizetype>(serialized->size())));
    }
    catch (const std::exception &error) { return foundation::Result<QByteArray>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, std::string("TensorRT YOLO11 engine build failed: ") + error.what())); }
}

foundation::Result<void> ResolveEngineIo(nvinfer1::ICudaEngine &engine, const char *contractName, const char **inputName, const char **outputName)
{
    if (engine.getNbIOTensors() != 2) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("TensorRT serialized ") + contractName + " engine has an invalid I/O contract"));
    for (int index = 0; index < engine.getNbIOTensors(); ++index)
    {
        const char *name = engine.getIOTensorName(index);
        if (name == nullptr) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("TensorRT ") + contractName + " engine contains an unnamed I/O tensor"));
        if (engine.getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) *inputName = name;
        if (engine.getTensorIOMode(name) == nvinfer1::TensorIOMode::kOUTPUT) *outputName = name;
    }
    if (*inputName == nullptr || *outputName == nullptr || engine.getTensorDataType(*inputName) != nvinfer1::DataType::kFLOAT || engine.getTensorDataType(*outputName) != nvinfer1::DataType::kFLOAT) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("TensorRT ") + contractName + " engine requires one float32 input and one float32 output"));
    return foundation::Result<void>::Success();
}

int64_t ElementCount(const nvinfer1::Dims &dimensions)
{
    if (dimensions.nbDims <= 0) return -1;
    int64_t count = 1;
    for (int index = 0; index < dimensions.nbDims; ++index)
    {
        if (dimensions.d[index] <= 0) return -1;
        count *= dimensions.d[index];
    }
    return count;
}
}

foundation::Result<void> BuildClassificationEngineFromOnnx(const QString &onnxPath, const int64_t featureCount)
{
    const auto request = CreateEngineCacheRequest(QStringLiteral("classification"), onnxPath, {1, featureCount});
    if (!request.IsSuccess()) return foundation::Result<void>::Failure(request.Failure());
    const auto serialized = BuildOrLoadSerializedEngine(request.Value(), [&]() { return BuildSerializedClassificationEngine(onnxPath, featureCount); });
    if (!serialized.IsSuccess()) return foundation::Result<void>::Failure(serialized.Failure());
    return foundation::Result<void>::Success();
}

foundation::Result<QVector<float>> RunClassificationOnnx(const QString &onnxPath, const QVector<float> &features)
{
    if (features.isEmpty()) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "TensorRT classification features must not be empty"));
    const auto request = CreateEngineCacheRequest(QStringLiteral("classification"), onnxPath, {1, features.size()});
    if (!request.IsSuccess()) return foundation::Result<QVector<float>>::Failure(request.Failure());
    auto serialized = BuildOrLoadSerializedEngine(request.Value(), [&]() { return BuildSerializedClassificationEngine(onnxPath, features.size()); });
    if (!serialized.IsSuccess()) return foundation::Result<QVector<float>>::Failure(serialized.Failure());
    void *deviceInput = nullptr;
    void *deviceOutput = nullptr;
    cudaStream_t stream = nullptr;
    const auto cleanup = [&]() { if (stream != nullptr) cudaStreamDestroy(stream); if (deviceOutput != nullptr) cudaFree(deviceOutput); if (deviceInput != nullptr) cudaFree(deviceInput); };
    try
    {
        TensorRtLogger logger;
        std::unique_ptr<nvinfer1::IRuntime> runtime(nvinfer1::createInferRuntime(logger));
        if (!runtime) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT runtime creation failed"));
        std::unique_ptr<nvinfer1::ICudaEngine> engine(runtime->deserializeCudaEngine(serialized.Value().bytes.constData(), static_cast<size_t>(serialized.Value().bytes.size())));
        if (!engine && serialized.Value().fromCache)
        {
            DeleteSingleCacheEntry(serialized.Value().entry);
            const auto rebuilt = BuildSerializedClassificationEngine(onnxPath, features.size());
            if (!rebuilt.IsSuccess()) return foundation::Result<QVector<float>>::Failure(rebuilt.Failure());
            const auto written = WriteCachedSerializedEngine(request.Value(), serialized.Value().entry, rebuilt.Value());
            if (!written.IsSuccess()) return foundation::Result<QVector<float>>::Failure(written.Failure());
            engine.reset(runtime->deserializeCudaEngine(rebuilt.Value().constData(), static_cast<size_t>(rebuilt.Value().size())));
        }
        if (!engine || engine->getNbIOTensors() != 2) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT serialized classification engine has an invalid I/O contract"));
        const char *inputName = nullptr;
        const char *outputName = nullptr;
        for (int index = 0; index < engine->getNbIOTensors(); ++index)
        {
            const char *name = engine->getIOTensorName(index);
            if (name == nullptr) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT engine contains an unnamed I/O tensor"));
            if (engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) inputName = name;
            if (engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kOUTPUT) outputName = name;
        }
        if (inputName == nullptr || outputName == nullptr || engine->getTensorDataType(inputName) != nvinfer1::DataType::kFLOAT || engine->getTensorDataType(outputName) != nvinfer1::DataType::kFLOAT) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT classification engine requires one float32 input and one float32 output"));
        std::unique_ptr<nvinfer1::IExecutionContext> context(engine->createExecutionContext());
        if (!context || !context->setInputShape(inputName, nvinfer1::Dims2{1, static_cast<int>(features.size())})) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT classification input shape could not be configured"));
        const nvinfer1::Dims outputShape = context->getTensorShape(outputName);
        if (outputShape.nbDims != 2 || outputShape.d[0] != 1 || outputShape.d[1] <= 0) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT classification output shape is invalid"));
        const size_t inputBytes = static_cast<size_t>(features.size()) * sizeof(float);
        const size_t outputBytes = static_cast<size_t>(outputShape.d[1]) * sizeof(float);
        if (cudaStreamCreate(&stream) != cudaSuccess || cudaMalloc(&deviceInput, inputBytes) != cudaSuccess || cudaMalloc(&deviceOutput, outputBytes) != cudaSuccess || cudaMemcpyAsync(deviceInput, features.constData(), inputBytes, cudaMemcpyHostToDevice, stream) != cudaSuccess || !context->setTensorAddress(inputName, deviceInput) || !context->setTensorAddress(outputName, deviceOutput) || !context->enqueueV3(stream)) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT FP32 inference setup or execution failed")); }
        QVector<float> output(outputShape.d[1]);
        if (cudaMemcpyAsync(output.data(), deviceOutput, outputBytes, cudaMemcpyDeviceToHost, stream) != cudaSuccess || cudaStreamSynchronize(stream) != cudaSuccess) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT FP32 inference result transfer failed")); }
        cleanup();
        return foundation::Result<QVector<float>>::Success(std::move(output));
    }
    catch (const std::exception &error) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, std::string("TensorRT classification execution failed: ") + error.what())); }
}

foundation::Result<QVector<float>> RunYolo11RawHeadOnnx(const QString &onnxPath, const QVector<float> &image, const int channels, const int height, const int width)
{
    if (channels <= 0 || height <= 0 || width <= 0) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "TensorRT YOLO11 image dimensions must be positive"));
    const qsizetype expectedElements = static_cast<qsizetype>(channels) * static_cast<qsizetype>(height) * static_cast<qsizetype>(width);
    if (image.size() != expectedElements) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "TensorRT YOLO11 image element count does not match channels, height and width"));
    const auto request = CreateEngineCacheRequest(QStringLiteral("yolo11_detection"), onnxPath, {1, channels, height, width});
    if (!request.IsSuccess()) return foundation::Result<QVector<float>>::Failure(request.Failure());
    auto serialized = BuildOrLoadSerializedEngine(request.Value(), [&]() { return BuildSerializedYolo11Engine(onnxPath, channels, height, width); });
    if (!serialized.IsSuccess()) return foundation::Result<QVector<float>>::Failure(serialized.Failure());
    void *deviceInput = nullptr;
    void *deviceOutput = nullptr;
    cudaStream_t stream = nullptr;
    const auto cleanup = [&]() { if (stream != nullptr) cudaStreamDestroy(stream); if (deviceOutput != nullptr) cudaFree(deviceOutput); if (deviceInput != nullptr) cudaFree(deviceInput); };
    try
    {
        TensorRtLogger logger;
        std::unique_ptr<nvinfer1::IRuntime> runtime(nvinfer1::createInferRuntime(logger));
        if (!runtime) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT YOLO11 runtime creation failed"));
        std::unique_ptr<nvinfer1::ICudaEngine> engine(runtime->deserializeCudaEngine(serialized.Value().bytes.constData(), static_cast<size_t>(serialized.Value().bytes.size())));
        if (!engine && serialized.Value().fromCache)
        {
            DeleteSingleCacheEntry(serialized.Value().entry);
            const auto rebuilt = BuildSerializedYolo11Engine(onnxPath, channels, height, width);
            if (!rebuilt.IsSuccess()) return foundation::Result<QVector<float>>::Failure(rebuilt.Failure());
            const auto written = WriteCachedSerializedEngine(request.Value(), serialized.Value().entry, rebuilt.Value());
            if (!written.IsSuccess()) return foundation::Result<QVector<float>>::Failure(written.Failure());
            engine.reset(runtime->deserializeCudaEngine(rebuilt.Value().constData(), static_cast<size_t>(rebuilt.Value().size())));
        }
        if (!engine) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT serialized YOLO11 engine could not be deserialized"));
        const char *inputName = nullptr;
        const char *outputName = nullptr;
        const auto io = ResolveEngineIo(*engine, "YOLO11", &inputName, &outputName);
        if (!io.IsSuccess()) return foundation::Result<QVector<float>>::Failure(io.Failure());
        std::unique_ptr<nvinfer1::IExecutionContext> context(engine->createExecutionContext());
        if (!context || !context->setInputShape(inputName, nvinfer1::Dims4{1, channels, height, width})) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "TensorRT YOLO11 input shape could not be configured"));
        const nvinfer1::Dims outputShape = context->getTensorShape(outputName);
        if (outputShape.nbDims != 3 || outputShape.d[0] != 1 || outputShape.d[1] <= 0 || outputShape.d[2] < 5) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 output shape is invalid"));
        const int64_t outputElements = ElementCount(outputShape);
        if (outputElements <= 0) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "TensorRT YOLO11 output element count is invalid"));
        const size_t inputBytes = static_cast<size_t>(image.size()) * sizeof(float);
        const size_t outputBytes = static_cast<size_t>(outputElements) * sizeof(float);
        if (cudaStreamCreate(&stream) != cudaSuccess || cudaMalloc(&deviceInput, inputBytes) != cudaSuccess || cudaMalloc(&deviceOutput, outputBytes) != cudaSuccess || cudaMemcpyAsync(deviceInput, image.constData(), inputBytes, cudaMemcpyHostToDevice, stream) != cudaSuccess || !context->setTensorAddress(inputName, deviceInput) || !context->setTensorAddress(outputName, deviceOutput) || !context->enqueueV3(stream)) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT YOLO11 FP32 inference setup or execution failed")); }
        QVector<float> output(static_cast<qsizetype>(outputElements));
        if (cudaMemcpyAsync(output.data(), deviceOutput, outputBytes, cudaMemcpyDeviceToHost, stream) != cudaSuccess || cudaStreamSynchronize(stream) != cudaSuccess) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "TensorRT YOLO11 FP32 inference result transfer failed")); }
        cleanup();
        return foundation::Result<QVector<float>>::Success(std::move(output));
    }
    catch (const std::exception &error) { cleanup(); return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, std::string("TensorRT YOLO11 execution failed: ") + error.what())); }
}
}
