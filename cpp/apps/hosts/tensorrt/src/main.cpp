#include "visionaiflow/qt_foundation/HostRuntime.h"
#include "visionaiflow/tensorrt_host/TensorRtValidator.h"
#include "visionaiflow/models/yolo11/Yolo11DetectionDecoder.h"

#include <QCoreApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCommandLineOption>
#include <QCommandLineParser>
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

visionaiflow::foundation::Result<QCborMap> ExecuteTensorRtOperation(const QCborMap &request)
{
    const QString operation = request.value(QStringLiteral("operation")).toString();
    if (operation == QStringLiteral("classifyOnnx") || operation == QStringLiteral("detectYolo11Onnx"))
    {
        QCborMap response;
        response.insert(QStringLiteral("operation"), operation);
        response.insert(QStringLiteral("precision"), QStringLiteral("fp32"));
        if (operation == QStringLiteral("classifyOnnx"))
        {
            const auto features = ParseFeatures(request, "TensorRT");
            if (!features.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(features.Failure());
            const auto result = visionaiflow::tensorrt_host::RunClassificationOnnx(request.value(QStringLiteral("modelPath")).toString(), features.Value());
            if (!result.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(result.Failure());
            response.insert(QStringLiteral("logits"), FloatsToCbor(result.Value()));
            return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
        }
        const auto parsedImage = ParseImage(request, "TensorRT");
        if (!parsedImage.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(parsedImage.Failure());
        const auto result = visionaiflow::tensorrt_host::RunYolo11RawHeadOnnx(request.value(QStringLiteral("modelPath")).toString(), parsedImage.Value().image, parsedImage.Value().channels, parsedImage.Value().height, parsedImage.Value().width);
        if (!result.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(result.Failure());
        const auto decoded = DecodeYolo11Request(request, result.Value());
        if (!decoded.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(decoded.Failure());
        response.insert(QStringLiteral("rawHead"), FloatsToCbor(result.Value()));
        response.insert(QStringLiteral("detections"), DetectionsToCbor(decoded.Value()));
        return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
    }
    if (operation != QStringLiteral("buildClassificationEngine")) return visionaiflow::foundation::Result<QCborMap>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::UnsupportedOperation, "TensorRT operation is unsupported"));
    const QCborValue featureCount = request.value(QStringLiteral("featureCount"));
    if (!featureCount.isInteger()) return visionaiflow::foundation::Result<QCborMap>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::InvalidArgument, "TensorRT engine build requires an integer featureCount"));
    const auto result = visionaiflow::tensorrt_host::BuildClassificationEngineFromOnnx(request.value(QStringLiteral("modelPath")).toString(), featureCount.toInteger());
    if (!result.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(result.Failure());
    QCborMap response;
    response.insert(QStringLiteral("operation"), QStringLiteral("buildClassificationEngine"));
    response.insert(QStringLiteral("precision"), QStringLiteral("fp32"));
    return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionTensorRtHost"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    try
    {
        QCommandLineParser parser;
        parser.addHelpOption();
        parser.addVersionOption();
        const QCommandLineOption ipcServerOption(QStringLiteral("ipc-server"), QStringLiteral("Local IPC server name"), QStringLiteral("name"));
        const QCommandLineOption validateOption(QStringLiteral("validate-onnx"), QStringLiteral("Parse and build a TensorRT FP32 engine for an ONNX classification model"), QStringLiteral("path"));
        const QCommandLineOption inferOption(QStringLiteral("infer-onnx"), QStringLiteral("Build and run one TensorRT FP32 classification inference"), QStringLiteral("path"));
        const QCommandLineOption inferYolo11Option(QStringLiteral("infer-yolo11-onnx"), QStringLiteral("Build and run one TensorRT FP32 YOLO11 detection inference"), QStringLiteral("path"));
        parser.addOption(ipcServerOption);
        parser.addOption(validateOption);
        parser.addOption(inferOption);
        parser.addOption(inferYolo11Option);
        parser.process(application);
        if (parser.isSet(validateOption))
        {
            const auto result = visionaiflow::tensorrt_host::BuildClassificationEngineFromOnnx(parser.value(validateOption), 3);
            if (!result.IsSuccess())
            {
                qCritical("VisionTensorRtHost ONNX validation failed: %s", result.Failure().message.c_str());
                return 1;
            }
            return 0;
        }
        if (parser.isSet(inferOption) || parser.isSet(inferYolo11Option))
        {
            const QString modelPath = parser.isSet(inferOption) ? parser.value(inferOption) : parser.value(inferYolo11Option);
            const auto result = parser.isSet(inferOption) ? visionaiflow::tensorrt_host::RunClassificationOnnx(modelPath, QVector<float>{0.0F, 0.0F, 0.0F}) : visionaiflow::tensorrt_host::RunYolo11RawHeadOnnx(modelPath, QVector<float>(3 * 16 * 16, 0.0F), 3, 16, 16);
            if (!result.IsSuccess())
            {
                qCritical("VisionTensorRtHost ONNX inference failed: %s", result.Failure().message.c_str());
                return 1;
            }
            QTextStream output(stdout);
            if (parser.isSet(inferYolo11Option))
            {
                visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
                config.scoreThreshold = 0.25F;
                config.nmsIouThreshold = 0.50F;
                const auto decoded = visionaiflow::models::yolo11::DecodeYolo11Detections(std::vector<float>(result.Value().cbegin(), result.Value().cend()), 2, 2, 100.0F, 100.0F, config);
                if (!decoded.IsSuccess())
                {
                    qCritical("VisionTensorRtHost YOLO11 decode failed: %s", decoded.Failure().message.c_str());
                    return 1;
                }
                output << "TensorRT YOLO11 raw output elements: " << result.Value().size() << "\nraw:";
                for (const float value : result.Value()) output << ' ' << value;
                output << '\n';
                PrintDetections(output, decoded.Value());
                return 0;
            }
            output << "TensorRT FP32 inference output elements: " << result.Value().size() << "\nlogits:";
            for (const float value : result.Value()) output << ' ' << value;
            output << '\n';
            return 0;
        }
        return visionaiflow::qt_foundation::RunHostApplication(application, QStringLiteral("tensorrt"), QStringLiteral("0.1.0"), ExecuteTensorRtOperation);
    }
    catch (const std::exception &exception)
    {
        qCritical("VisionTensorRtHost startup failed: %s", exception.what());
        return 1;
    }
    catch (...)
    {
        qCritical("VisionTensorRtHost startup failed with an unknown exception");
        return 1;
    }
}
