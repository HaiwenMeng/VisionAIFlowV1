#include "visionaiflow/openvino_host/OpenVinoClassifier.h"

#include <QFileInfo>

#include <openvino/openvino.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <sstream>

namespace visionaiflow::openvino_host
{
namespace
{
QString PerformanceHintToString(const ov::hint::PerformanceMode mode)
{
    std::ostringstream stream;
    stream << mode;
    return QString::fromStdString(stream.str());
}

foundation::Result<OpenVinoRuntimeMetadata> ReadRuntimeMetadata(ov::Core &core, const ov::CompiledModel &compiled)
{
    try
    {
        OpenVinoRuntimeMetadata metadata;
        metadata.requestedDevice = QStringLiteral("CPU");
        const std::vector<std::string> executionDevices = compiled.get_property(ov::execution_devices);
        for (const std::string &device : executionDevices) metadata.executionDevices.append(QString::fromStdString(device));
        if (metadata.executionDevices.isEmpty()) return foundation::Result<OpenVinoRuntimeMetadata>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "OpenVINO compiled model did not report execution devices"));
        metadata.fullDeviceName = QString::fromStdString(core.get_property("CPU", ov::device::full_name));
        const ov::element::Type precision = compiled.get_property(ov::hint::inference_precision);
        metadata.inferencePrecision = QString::fromStdString(precision.get_type_name());
        metadata.performanceHint = PerformanceHintToString(compiled.get_property(ov::hint::performance_mode));
        metadata.inferenceNumThreads = compiled.get_property(ov::inference_num_threads);
        if (metadata.inferenceNumThreads <= 0) return foundation::Result<OpenVinoRuntimeMetadata>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "OpenVINO compiled model reported an invalid CPU inference thread count"));
        return foundation::Result<OpenVinoRuntimeMetadata>::Success(std::move(metadata));
    }
    catch (const ov::Exception &error) { return foundation::Result<OpenVinoRuntimeMetadata>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("OpenVINO runtime metadata query failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<OpenVinoRuntimeMetadata>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("OpenVINO runtime metadata query failed: ") + error.what())); }
}

QVector<float> TensorToVector(const ov::Tensor &outputTensor)
{
    const float *output = outputTensor.data<const float>();
    QVector<float> result;
    result.reserve(static_cast<qsizetype>(outputTensor.get_size()));
    for (size_t index = 0; index < outputTensor.get_size(); ++index) result.append(output[index]);
    return result;
}
}

foundation::Result<OpenVinoInferenceResult> RunClassificationOnnxWithMetadata(const QString &onnxPath, const QVector<float> &features)
{
    if (onnxPath.isEmpty() || features.isEmpty()) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX path and classification features must not be empty"));
    const QFileInfo fileInfo(onnxPath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "ONNX model file does not exist or is not readable"));
    try
    {
        ov::Core core;
        const std::shared_ptr<ov::Model> model = core.read_model(onnxPath.toUtf8().constData());
        if (!model) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "OpenVINO returned an empty model after ONNX parsing"));
        ov::CompiledModel compiled = core.compile_model(model, "CPU", ov::hint::performance_mode(ov::hint::PerformanceMode::LATENCY), ov::hint::inference_precision(ov::element::f32));
        const auto metadata = ReadRuntimeMetadata(core, compiled);
        if (!metadata.IsSuccess()) return foundation::Result<OpenVinoInferenceResult>::Failure(metadata.Failure());
        if (compiled.inputs().size() != 1U || compiled.outputs().size() != 1U) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Classification ONNX model must have exactly one input and one output"));
        const ov::Output<const ov::Node> inputPort = compiled.input();
        const ov::PartialShape inputShape = inputPort.get_partial_shape();
        if (inputPort.get_element_type() != ov::element::f32 || inputShape.rank().is_dynamic() || inputShape.rank().get_length() != 2) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Classification ONNX input must be rank-two float32"));
        const ov::Dimension featureDimension = inputShape[1];
        if (featureDimension.is_static() && featureDimension.get_length() != static_cast<int64_t>(features.size())) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Classification feature count does not match the ONNX input contract"));
        ov::InferRequest request = compiled.create_infer_request();
        ov::Tensor inputTensor(ov::element::f32, ov::Shape{1U, static_cast<size_t>(features.size())});
        float *inputData = inputTensor.data<float>();
        if (inputData == nullptr) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "OpenVINO could not provide writable classification input storage"));
        std::copy(features.cbegin(), features.cend(), inputData);
        request.set_input_tensor(inputTensor);
        request.infer();
        const ov::Tensor outputTensor = request.get_output_tensor();
        if (outputTensor.get_element_type() != ov::element::f32 || outputTensor.get_size() == 0U) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Classification ONNX output must be a non-empty float32 tensor"));
        return foundation::Result<OpenVinoInferenceResult>::Success({TensorToVector(outputTensor), metadata.Value()});
    }
    catch (const ov::Exception &error) { return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("OpenVINO inference failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("OpenVINO classification execution failed: ") + error.what())); }
}

foundation::Result<QVector<float>> RunClassificationOnnx(const QString &onnxPath, const QVector<float> &features)
{
    const auto result = RunClassificationOnnxWithMetadata(onnxPath, features);
    if (!result.IsSuccess()) return foundation::Result<QVector<float>>::Failure(result.Failure());
    return foundation::Result<QVector<float>>::Success(result.Value().values);
}

foundation::Result<OpenVinoInferenceResult> RunYolo11RawHeadOnnxWithMetadata(const QString &onnxPath, const QVector<float> &image, const int channels, const int height, const int width)
{
    if (onnxPath.isEmpty() || channels <= 0 || height <= 0 || width <= 0) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX path and YOLO11 image dimensions must be valid"));
    const qsizetype expectedElements = static_cast<qsizetype>(channels) * static_cast<qsizetype>(height) * static_cast<qsizetype>(width);
    if (image.size() != expectedElements) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 image element count does not match channels, height and width"));
    const QFileInfo fileInfo(onnxPath);
    if (!fileInfo.isFile() || !fileInfo.isReadable()) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "YOLO11 ONNX model file does not exist or is not readable"));
    try
    {
        ov::Core core;
        const std::shared_ptr<ov::Model> model = core.read_model(onnxPath.toUtf8().constData());
        if (!model) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "OpenVINO returned an empty YOLO11 model after ONNX parsing"));
        ov::CompiledModel compiled = core.compile_model(model, "CPU", ov::hint::performance_mode(ov::hint::PerformanceMode::LATENCY), ov::hint::inference_precision(ov::element::f32));
        const auto metadata = ReadRuntimeMetadata(core, compiled);
        if (!metadata.IsSuccess()) return foundation::Result<OpenVinoInferenceResult>::Failure(metadata.Failure());
        if (compiled.inputs().size() != 1U || compiled.outputs().size() != 1U) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "YOLO11 ONNX model must have exactly one input and one output"));
        const ov::Output<const ov::Node> inputPort = compiled.input();
        const ov::PartialShape inputShape = inputPort.get_partial_shape();
        if (inputPort.get_element_type() != ov::element::f32 || inputShape.rank().is_dynamic() || inputShape.rank().get_length() != 4) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO11 ONNX input must be rank-four float32"));
        const std::array<int64_t, 4> requestedShape{1, channels, height, width};
        for (size_t index = 0; index < requestedShape.size(); ++index)
        {
            if (inputShape[index].is_static() && inputShape[index].get_length() != requestedShape[index]) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 image shape does not match the ONNX input contract"));
        }
        ov::InferRequest request = compiled.create_infer_request();
        ov::Tensor inputTensor(ov::element::f32, ov::Shape{1U, static_cast<size_t>(channels), static_cast<size_t>(height), static_cast<size_t>(width)});
        float *inputData = inputTensor.data<float>();
        if (inputData == nullptr) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "OpenVINO could not provide writable YOLO11 input storage"));
        std::copy(image.cbegin(), image.cend(), inputData);
        request.set_input_tensor(inputTensor);
        request.infer();
        const ov::Tensor outputTensor = request.get_output_tensor();
        if (outputTensor.get_element_type() != ov::element::f32 || outputTensor.get_size() == 0U) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO11 ONNX output must be a non-empty float32 tensor"));
        const ov::Shape outputShape = outputTensor.get_shape();
        if (outputShape.size() != 3U || outputShape[0] != 1U || outputShape[1] == 0U || outputShape[2] < 5U) return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO11 ONNX output must have shape [1, rows, 4 + classCount]"));
        return foundation::Result<OpenVinoInferenceResult>::Success({TensorToVector(outputTensor), metadata.Value()});
    }
    catch (const ov::Exception &error) { return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("OpenVINO YOLO11 inference failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<OpenVinoInferenceResult>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("OpenVINO YOLO11 execution failed: ") + error.what())); }
}

foundation::Result<QVector<float>> RunYolo11RawHeadOnnx(const QString &onnxPath, const QVector<float> &image, const int channels, const int height, const int width)
{
    const auto result = RunYolo11RawHeadOnnxWithMetadata(onnxPath, image, channels, height, width);
    if (!result.IsSuccess()) return foundation::Result<QVector<float>>::Failure(result.Failure());
    return foundation::Result<QVector<float>>::Success(result.Value().values);
}
}
