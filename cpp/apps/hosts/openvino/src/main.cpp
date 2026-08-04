#include "visionaiflow/qt_foundation/HostRuntime.h"
#include "visionaiflow/qt_foundation/StructuredLogger.h"
#include "visionaiflow/openvino_host/OpenVinoClassifier.h"
#include "visionaiflow/models/yolo11/Yolo11DetectionDecoder.h"

#include <QCoreApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QTextStream>
#include <QVector>

#include <exception>
#include <string>
#include <vector>

namespace
{
struct ParsedImage final
{
    QVector<float> image;
    int channels{0};
    int height{0};
    int width{0};
};

visionaiflow::foundation::Result<QVector<float>> ParseFeatures(const QCborMap &request, const char *backendName)
{
    const QCborArray values = request.value(QStringLiteral("features")).toArray();
    if (values.isEmpty()) return visionaiflow::foundation::Result<QVector<float>>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " inference requires a non-empty features array"));
    QVector<float> features;
    features.reserve(values.size());
    for (const QCborValue &value : values)
    {
        if (!value.isDouble() && !value.isInteger()) return visionaiflow::foundation::Result<QVector<float>>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " inference features must be numeric"));
        features.append(static_cast<float>(value.toDouble()));
    }
    return visionaiflow::foundation::Result<QVector<float>>::Success(std::move(features));
}

visionaiflow::foundation::Result<ParsedImage> ParseImage(const QCborMap &request, const char *backendName)
{
    const QCborValue channelValue = request.value(QStringLiteral("channels"));
    const QCborValue heightValue = request.value(QStringLiteral("height"));
    const QCborValue widthValue = request.value(QStringLiteral("width"));
    if (!channelValue.isInteger() || !heightValue.isInteger() || !widthValue.isInteger()) return visionaiflow::foundation::Result<ParsedImage>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 inference requires integer channels, height and width"));
    ParsedImage parsed;
    parsed.channels = static_cast<int>(channelValue.toInteger());
    parsed.height = static_cast<int>(heightValue.toInteger());
    parsed.width = static_cast<int>(widthValue.toInteger());
    if (parsed.channels <= 0 || parsed.height <= 0 || parsed.width <= 0) return visionaiflow::foundation::Result<ParsedImage>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image dimensions must be positive"));
    const QCborArray values = request.value(QStringLiteral("image")).toArray();
    const qsizetype expectedElements = static_cast<qsizetype>(parsed.channels) * static_cast<qsizetype>(parsed.height) * static_cast<qsizetype>(parsed.width);
    if (values.size() != expectedElements) return visionaiflow::foundation::Result<ParsedImage>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image element count does not match channels, height and width"));
    parsed.image.reserve(values.size());
    for (const QCborValue &value : values)
    {
        if (!value.isDouble() && !value.isInteger()) return visionaiflow::foundation::Result<ParsedImage>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image values must be numeric"));
        parsed.image.append(static_cast<float>(value.toDouble()));
    }
    return visionaiflow::foundation::Result<ParsedImage>::Success(std::move(parsed));
}

QCborArray FloatsToCbor(const QVector<float> &values)
{
    QCborArray array;
    for (const float value : values) array.append(value);
    return array;
}

QCborArray DetectionsToCbor(const std::vector<visionaiflow::models::yolo11::Detection> &detections)
{
    QCborArray array;
    for (const auto &detection : detections)
    {
        QCborMap item;
        item.insert(QStringLiteral("classIndex"), detection.classIndex);
        item.insert(QStringLiteral("score"), detection.score);
        item.insert(QStringLiteral("x1"), detection.box.x1);
        item.insert(QStringLiteral("y1"), detection.box.y1);
        item.insert(QStringLiteral("x2"), detection.box.x2);
        item.insert(QStringLiteral("y2"), detection.box.y2);
        array.append(item);
    }
    return array;
}

QCborMap RuntimeToCbor(const visionaiflow::openvino_host::OpenVinoRuntimeMetadata &metadata)
{
    QCborMap runtime;
    runtime.insert(QStringLiteral("requestedDevice"), metadata.requestedDevice);
    QCborArray executionDevices;
    for (const QString &device : metadata.executionDevices) executionDevices.append(device);
    runtime.insert(QStringLiteral("executionDevices"), executionDevices);
    runtime.insert(QStringLiteral("fullDeviceName"), metadata.fullDeviceName);
    runtime.insert(QStringLiteral("inferencePrecision"), metadata.inferencePrecision);
    runtime.insert(QStringLiteral("performanceHint"), metadata.performanceHint);
    runtime.insert(QStringLiteral("inferenceNumThreads"), metadata.inferenceNumThreads);
    return runtime;
}

void PrintRuntime(QTextStream &output, const visionaiflow::openvino_host::OpenVinoRuntimeMetadata &metadata)
{
    output << "runtime:"
           << " requestedDevice=" << metadata.requestedDevice
           << " executionDevices=" << metadata.executionDevices.join(QStringLiteral(","))
           << " fullDeviceName=" << metadata.fullDeviceName
           << " inferencePrecision=" << metadata.inferencePrecision
           << " performanceHint=" << metadata.performanceHint
           << " inferenceNumThreads=" << metadata.inferenceNumThreads
           << '\n';
}

visionaiflow::foundation::Result<std::vector<visionaiflow::models::yolo11::Detection>> DecodeYolo11Request(const QCborMap &request, const QVector<float> &raw)
{
    const QCborValue rowCountValue = request.value(QStringLiteral("rowCount"));
    const QCborValue classCountValue = request.value(QStringLiteral("classCount"));
    if (!rowCountValue.isInteger() || !classCountValue.isInteger()) return visionaiflow::foundation::Result<std::vector<visionaiflow::models::yolo11::Detection>>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, "YOLO11 detection requires integer rowCount and classCount"));
    const double networkWidth = request.value(QStringLiteral("imageWidth")).toDouble();
    const double networkHeight = request.value(QStringLiteral("imageHeight")).toDouble();
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    if (request.contains(QStringLiteral("scoreThreshold"))) config.scoreThreshold = static_cast<float>(request.value(QStringLiteral("scoreThreshold")).toDouble());
    if (request.contains(QStringLiteral("nmsIouThreshold"))) config.nmsIouThreshold = static_cast<float>(request.value(QStringLiteral("nmsIouThreshold")).toDouble());
    if (request.contains(QStringLiteral("classAgnosticNms"))) config.classAgnosticNms = request.value(QStringLiteral("classAgnosticNms")).toBool();
    if (request.contains(QStringLiteral("maxDetections"))) config.maxDetections = static_cast<int>(request.value(QStringLiteral("maxDetections")).toInteger());
    if (request.contains(QStringLiteral("originalImageWidth")) || request.contains(QStringLiteral("originalImageHeight")))
    {
        if (!request.contains(QStringLiteral("originalImageWidth")) || !request.contains(QStringLiteral("originalImageHeight"))) return visionaiflow::foundation::Result<std::vector<visionaiflow::models::yolo11::Detection>>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, "YOLO11 letterbox restore requires both originalImageWidth and originalImageHeight"));
        const bool allowScaleUp = request.contains(QStringLiteral("letterboxAllowScaleUp")) ? request.value(QStringLiteral("letterboxAllowScaleUp")).toBool(true) : true;
        const auto geometry = visionaiflow::models::yolo11::CreateYolo11LetterboxGeometry(static_cast<float>(request.value(QStringLiteral("originalImageWidth")).toDouble()), static_cast<float>(request.value(QStringLiteral("originalImageHeight")).toDouble()), static_cast<float>(networkWidth), static_cast<float>(networkHeight), allowScaleUp);
        if (!geometry.IsSuccess()) return visionaiflow::foundation::Result<std::vector<visionaiflow::models::yolo11::Detection>>::Failure(geometry.Failure());
        return visionaiflow::models::yolo11::DecodeYolo11DetectionsFromLetterbox(std::vector<float>(raw.cbegin(), raw.cend()), static_cast<int>(rowCountValue.toInteger()), static_cast<int>(classCountValue.toInteger()), geometry.Value(), config);
    }
    return visionaiflow::models::yolo11::DecodeYolo11Detections(std::vector<float>(raw.cbegin(), raw.cend()), static_cast<int>(rowCountValue.toInteger()), static_cast<int>(classCountValue.toInteger()), static_cast<float>(networkWidth), static_cast<float>(networkHeight), config);
}

void PrintDetections(QTextStream &output, const std::vector<visionaiflow::models::yolo11::Detection> &detections)
{
    output << "detections:";
    for (const auto &detection : detections) output << ' ' << detection.classIndex << ',' << detection.score << ',' << detection.box.x1 << ',' << detection.box.y1 << ',' << detection.box.x2 << ',' << detection.box.y2;
    output << '\n';
}

visionaiflow::foundation::Result<QCborMap> ExecuteOpenVinoOperation(const QCborMap &request)
{
    const QString operation = request.value(QStringLiteral("operation")).toString();
    if (operation != QStringLiteral("classifyOnnx") && operation != QStringLiteral("detectYolo11Onnx")) return visionaiflow::foundation::Result<QCborMap>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::UnsupportedOperation, "OpenVINO operation is unsupported"));
    QCborMap response;
    response.insert(QStringLiteral("operation"), operation);
    if (operation == QStringLiteral("classifyOnnx"))
    {
        const auto features = ParseFeatures(request, "OpenVINO");
        if (!features.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(features.Failure());
        const auto result = visionaiflow::openvino_host::RunClassificationOnnxWithMetadata(request.value(QStringLiteral("modelPath")).toString(), features.Value());
        if (!result.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(result.Failure());
        response.insert(QStringLiteral("logits"), FloatsToCbor(result.Value().values));
        response.insert(QStringLiteral("runtime"), RuntimeToCbor(result.Value().runtime));
        return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
    }
    const auto parsedImage = ParseImage(request, "OpenVINO");
    if (!parsedImage.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(parsedImage.Failure());
    const auto result = visionaiflow::openvino_host::RunYolo11RawHeadOnnxWithMetadata(request.value(QStringLiteral("modelPath")).toString(), parsedImage.Value().image, parsedImage.Value().channels, parsedImage.Value().height, parsedImage.Value().width);
    if (!result.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(result.Failure());
    const auto decoded = DecodeYolo11Request(request, result.Value().values);
    if (!decoded.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(decoded.Failure());
    response.insert(QStringLiteral("rawHead"), FloatsToCbor(result.Value().values));
    response.insert(QStringLiteral("detections"), DetectionsToCbor(decoded.Value()));
    response.insert(QStringLiteral("runtime"), RuntimeToCbor(result.Value().runtime));
    return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionOpenVinoHost"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    try
    {
        QCommandLineParser parser;
        parser.addHelpOption();
        parser.addVersionOption();
        const QCommandLineOption ipcServerOption(QStringLiteral("ipc-server"), QStringLiteral("Local IPC server name"), QStringLiteral("name"));
        const QCommandLineOption validateOption(QStringLiteral("validate-onnx"), QStringLiteral("Load and run one CPU inference for an ONNX classification model"), QStringLiteral("path"));
        const QCommandLineOption inferYolo11Option(QStringLiteral("infer-yolo11-onnx"), QStringLiteral("Load and run one CPU YOLO11 detection inference"), QStringLiteral("path"));
        parser.addOption(ipcServerOption);
        parser.addOption(validateOption);
        parser.addOption(inferYolo11Option);
        parser.process(application);
        if (parser.isSet(validateOption) || parser.isSet(inferYolo11Option))
        {
            const auto initialized = visionaiflow::qt_foundation::StructuredLogger::Initialize(QDir(application.applicationDirPath()).filePath(QStringLiteral("logs")), QStringLiteral("openvino"));
            if (!initialized.IsSuccess())
            {
                QTextStream(stderr) << "OpenVINO log initialization failed: " << QString::fromStdString(initialized.Failure().message) << '\n';
                return 2;
            }
            const QString modelPath = parser.isSet(validateOption) ? parser.value(validateOption) : parser.value(inferYolo11Option);
            const auto result = parser.isSet(validateOption) ? visionaiflow::openvino_host::RunClassificationOnnxWithMetadata(modelPath, QVector<float>{0.0F, 0.0F, 0.0F}) : visionaiflow::openvino_host::RunYolo11RawHeadOnnxWithMetadata(modelPath, QVector<float>(3 * 16 * 16, 0.0F), 3, 16, 16);
            if (!result.IsSuccess())
            {
                visionaiflow::qt_foundation::StructuredLogger::Error(QStringLiteral("openvino"), result.Failure());
                QTextStream(stderr) << "VisionOpenVinoHost ONNX validation failed: " << QString::fromStdString(result.Failure().message) << '\n';
                qCritical("VisionOpenVinoHost ONNX validation failed: %s", result.Failure().message.c_str());
                return 1;
            }
            visionaiflow::qt_foundation::StructuredLogger::Info(QStringLiteral("openvino"), QStringLiteral("OpenVINO CPU ONNX validation succeeded"));
            QTextStream output(stdout);
            PrintRuntime(output, result.Value().runtime);
            if (parser.isSet(inferYolo11Option))
            {
                visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
                config.scoreThreshold = 0.25F;
                config.nmsIouThreshold = 0.50F;
                const auto decoded = visionaiflow::models::yolo11::DecodeYolo11Detections(std::vector<float>(result.Value().values.cbegin(), result.Value().values.cend()), 2, 2, 100.0F, 100.0F, config);
                if (!decoded.IsSuccess())
                {
                    QTextStream(stderr) << "VisionOpenVinoHost YOLO11 decode failed: " << QString::fromStdString(decoded.Failure().message) << '\n';
                    return 1;
                }
                output << "OpenVINO YOLO11 raw output elements: " << result.Value().values.size() << "\nraw:";
                for (const float value : result.Value().values) output << ' ' << value;
                output << '\n';
                PrintDetections(output, decoded.Value());
                return 0;
            }
            output << "OpenVINO CPU inference output elements: " << result.Value().values.size() << "\nlogits:";
            for (const float value : result.Value().values) output << ' ' << value;
            output << '\n';
            return 0;
        }
        return visionaiflow::qt_foundation::RunHostApplication(application, QStringLiteral("openvino"), QStringLiteral("0.1.0"), ExecuteOpenVinoOperation);
    }
    catch (const std::exception &exception)
    {
        qCritical("VisionOpenVinoHost startup failed: %s", exception.what());
        return 1;
    }
    catch (...)
    {
        qCritical("VisionOpenVinoHost startup failed with an unknown exception");
        return 1;
    }
}
