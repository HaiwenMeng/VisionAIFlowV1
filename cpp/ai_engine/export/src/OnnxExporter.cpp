#include "visionaiflow/export/OnnxExporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

#include <onnx/checker.h>
#include <onnx/onnx_pb.h>

#include <exception>
#include <string>
#include <vector>

namespace visionaiflow::exporter
{
namespace
{
void SetTensorShape(ONNX_NAMESPACE::TypeProto *type, const int64_t featureCount)
{
    type->mutable_tensor_type()->set_elem_type(ONNX_NAMESPACE::TensorProto_DataType_FLOAT);
    auto *shape = type->mutable_tensor_type()->mutable_shape();
    shape->add_dim()->set_dim_param("batch");
    shape->add_dim()->set_dim_value(featureCount);
}

void SetFloatTensorShape(ONNX_NAMESPACE::TypeProto *type, const std::vector<int64_t> &dimensions)
{
    type->mutable_tensor_type()->set_elem_type(ONNX_NAMESPACE::TensorProto_DataType_FLOAT);
    auto *shape = type->mutable_tensor_type()->mutable_shape();
    for (const int64_t dimension : dimensions) shape->add_dim()->set_dim_value(dimension);
}

int64_t ElementCount(const std::vector<int64_t> &dimensions)
{
    int64_t count = 1;
    for (const int64_t dimension : dimensions)
    {
        if (dimension <= 0) return -1;
        count *= dimension;
    }
    return count;
}

foundation::Result<void> AddFloatInitializer(ONNX_NAMESPACE::GraphProto *graph, const char *name, const torch::Tensor &tensor, const std::vector<int64_t> &dimensions)
{
    const torch::Tensor materialized = tensor.detach().to(torch::kCPU).contiguous();
    const int64_t expectedElements = ElementCount(dimensions);
    if (!materialized.defined() || materialized.scalar_type() != torch::kFloat32 || expectedElements <= 0 || materialized.numel() != expectedElements) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "ONNX exporter received a parameter tensor that does not match the requested float32 initializer shape"));
    auto *initializer = graph->add_initializer();
    initializer->set_name(name);
    initializer->set_data_type(ONNX_NAMESPACE::TensorProto_DataType_FLOAT);
    for (const int64_t dimension : dimensions) initializer->add_dims(dimension);
    const size_t byteCount = static_cast<size_t>(materialized.numel()) * sizeof(float);
    initializer->set_raw_data(std::string(static_cast<const char *>(materialized.data_ptr()), byteCount));
    return foundation::Result<void>::Success();
}

foundation::Result<void> AddInt64Initializer(ONNX_NAMESPACE::GraphProto *graph, const char *name, const std::vector<int64_t> &values, const std::vector<int64_t> &dimensions)
{
    const int64_t expectedElements = ElementCount(dimensions);
    if (values.empty() || expectedElements <= 0 || static_cast<int64_t>(values.size()) != expectedElements) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "ONNX exporter received an int64 initializer that does not match the requested shape"));
    auto *initializer = graph->add_initializer();
    initializer->set_name(name);
    initializer->set_data_type(ONNX_NAMESPACE::TensorProto_DataType_INT64);
    for (const int64_t dimension : dimensions) initializer->add_dims(dimension);
    initializer->set_raw_data(std::string(reinterpret_cast<const char *>(values.data()), values.size() * sizeof(int64_t)));
    return foundation::Result<void>::Success();
}

void AddIntAttribute(ONNX_NAMESPACE::NodeProto *node, const char *name, const int64_t value)
{
    auto *attribute = node->add_attribute();
    attribute->set_name(name);
    attribute->set_type(ONNX_NAMESPACE::AttributeProto_AttributeType_INT);
    attribute->set_i(value);
}

void AddIntsAttribute(ONNX_NAMESPACE::NodeProto *node, const char *name, const std::vector<int64_t> &values)
{
    auto *attribute = node->add_attribute();
    attribute->set_name(name);
    attribute->set_type(ONNX_NAMESPACE::AttributeProto_AttributeType_INTS);
    for (const int64_t value : values) attribute->add_ints(value);
}

foundation::Result<void> ValidateModelProtoContract(const ONNX_NAMESPACE::ModelProto &modelProto, const OnnxFileContract &contract)
{
    if (contract.irVersion <= 0 || contract.opsetVersion <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX file contract versions must be positive"));
    if (modelProto.ir_version() != contract.irVersion) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "ONNX model IR version does not match the export contract"));
    if (modelProto.opset_import_size() != 1) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "ONNX model must declare exactly one opset import"));
    const auto &opset = modelProto.opset_import(0);
    if (!opset.domain().empty() && opset.domain() != "ai.onnx") return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "ONNX model declares an unsupported opset domain"));
    if (opset.version() != contract.opsetVersion) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "ONNX model opset version does not match the export contract"));
    if (!contract.allowedOps.isEmpty())
    {
        QSet<QString> allowed;
        for (const QString &op : contract.allowedOps)
        {
            if (op.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX allowed op list must not contain empty entries"));
            allowed.insert(op);
        }
        for (const auto &node : modelProto.graph().node())
        {
            if (!allowed.contains(QString::fromStdString(node.op_type()))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("ONNX model uses an operator outside the export whitelist: ") + node.op_type()));
        }
    }
    ONNX_NAMESPACE::checker::check_model(modelProto, true);
    return foundation::Result<void>::Success();
}

foundation::Result<void> WriteAndVerifyOnnx(const QString &path, const ONNX_NAMESPACE::ModelProto &modelProto, const OnnxFileContract &contract)
{
    std::string serialized;
    if (!modelProto.SerializeToString(&serialized) || serialized.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "ONNX model serialization failed"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to open ONNX output: ").append(file.errorString()).toStdString()));
    if (file.write(serialized.data(), static_cast<qint64>(serialized.size())) != static_cast<qint64>(serialized.size()) || !file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to atomically write ONNX output: ").append(file.errorString()).toStdString()));
    QFile verificationFile(path);
    if (!verificationFile.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to reopen ONNX output: ").append(verificationFile.errorString()).toStdString()));
    ONNX_NAMESPACE::ModelProto verified;
    const QByteArray verificationBytes = verificationFile.readAll();
    if (!verified.ParseFromArray(verificationBytes.constData(), verificationBytes.size())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Written ONNX model cannot be parsed"));
    return ValidateModelProtoContract(verified, contract);
}
}

foundation::Result<void> ValidateOnnxFileContract(const QString &path, const OnnxFileContract &contract)
{
    if (path.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX file path must not be empty when validating export contract"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Unable to read ONNX file for contract validation: ").append(file.errorString()).toStdString()));
    ONNX_NAMESPACE::ModelProto modelProto;
    const QByteArray bytes = file.readAll();
    if (!modelProto.ParseFromArray(bytes.constData(), bytes.size())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "ONNX file cannot be parsed during contract validation"));
    try
    {
        return ValidateModelProtoContract(modelProto, contract);
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("ONNX checker rejected contract validation: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("ONNX contract validation failed: ") + error.what())); }
}

foundation::Result<void> ExportLinearClassifierOnnx(const QString &path, training::LinearClassifier &model, const int64_t inputFeatures, const int64_t classCount)
{
    if (path.isEmpty() || !model || inputFeatures <= 0 || classCount < 2) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX export arguments are invalid"));
    if (!QFileInfo(path).dir().exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ONNX export parent directory does not exist"));
    try
    {
        const torch::Tensor weight = model->Weight();
        const torch::Tensor bias = model->Bias();
        if (weight.dim() != 2 || weight.size(0) != classCount || weight.size(1) != inputFeatures || bias.dim() != 1 || bias.size(0) != classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Linear classifier parameters do not match the requested ONNX contract"));
        ONNX_NAMESPACE::ModelProto modelProto;
        modelProto.set_ir_version(9);
        modelProto.set_producer_name("VisionAIFlow");
        modelProto.set_producer_version("0.1.0");
        auto *opset = modelProto.add_opset_import();
        opset->set_domain("");
        opset->set_version(12);
        auto *graph = modelProto.mutable_graph();
        graph->set_name("linear_classifier");
        auto *input = graph->add_input();
        input->set_name("input");
        SetTensorShape(input->mutable_type(), inputFeatures);
        auto *output = graph->add_output();
        output->set_name("logits");
        SetTensorShape(output->mutable_type(), classCount);
        const auto weightResult = AddFloatInitializer(graph, "linear.weight", weight, {classCount, inputFeatures});
        if (!weightResult.IsSuccess()) return weightResult;
        const auto biasResult = AddFloatInitializer(graph, "linear.bias", bias, {classCount});
        if (!biasResult.IsSuccess()) return biasResult;
        auto *gemm = graph->add_node();
        gemm->set_name("linear");
        gemm->set_op_type("Gemm");
        gemm->add_input("input");
        gemm->add_input("linear.weight");
        gemm->add_input("linear.bias");
        gemm->add_output("logits");
        auto *transpose = gemm->add_attribute();
        transpose->set_name("transB");
        transpose->set_type(ONNX_NAMESPACE::AttributeProto_AttributeType_INT);
        transpose->set_i(1);
        ONNX_NAMESPACE::checker::check_model(modelProto, true);
        return WriteAndVerifyOnnx(path, modelProto, {9, 12, {QStringLiteral("Gemm")}});
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("ONNX checker rejected export: ") + error.what())); }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch ONNX export failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("ONNX export failed: ") + error.what())); }
}

foundation::Result<void> ExportYolo11TinyDetectorOnnx(const QString &path, training::Yolo11TinyDetector &model, const int64_t inputChannels, const int64_t imageHeight, const int64_t imageWidth, const int64_t rowCount, const int64_t classCount)
{
    if (path.isEmpty() || !model || inputChannels <= 0 || imageHeight <= 0 || imageWidth <= 0 || rowCount <= 0 || classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector ONNX export arguments are invalid"));
    if (!QFileInfo(path).dir().exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 tiny detector ONNX export parent directory does not exist"));
    try
    {
        if (model->RowCount() != rowCount || model->ClassCount() != classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 tiny detector module contract does not match requested ONNX output shape"));
        const torch::Tensor conv1Weight = model->Conv1Weight();
        const torch::Tensor conv1Bias = model->Conv1Bias();
        const torch::Tensor conv2Weight = model->Conv2Weight();
        const torch::Tensor conv2Bias = model->Conv2Bias();
        const torch::Tensor headWeight = model->HeadWeight();
        const torch::Tensor headBias = model->HeadBias();
        if (conv1Weight.dim() != 4 || conv1Weight.size(0) != 16 || conv1Weight.size(1) != inputChannels || conv1Weight.size(2) != 3 || conv1Weight.size(3) != 3 || conv1Bias.dim() != 1 || conv1Bias.size(0) != 16) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 tiny detector conv1 parameters do not match the ONNX contract"));
        if (conv2Weight.dim() != 4 || conv2Weight.size(0) != 32 || conv2Weight.size(1) != 16 || conv2Weight.size(2) != 3 || conv2Weight.size(3) != 3 || conv2Bias.dim() != 1 || conv2Bias.size(0) != 32) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 tiny detector conv2 parameters do not match the ONNX contract"));
        const int64_t rowWidth = 4 + classCount;
        const int64_t flatHeadCount = rowCount * rowWidth;
        if (headWeight.dim() != 2 || headWeight.size(0) != flatHeadCount || headWeight.size(1) != 32 || headBias.dim() != 1 || headBias.size(0) != flatHeadCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 tiny detector head parameters do not match the ONNX contract"));

        ONNX_NAMESPACE::ModelProto modelProto;
        modelProto.set_ir_version(9);
        modelProto.set_producer_name("VisionAIFlow");
        modelProto.set_producer_version("0.1.0");
        auto *opset = modelProto.add_opset_import();
        opset->set_domain("");
        opset->set_version(12);
        auto *graph = modelProto.mutable_graph();
        graph->set_name("yolo11_tiny_detector");
        auto *input = graph->add_input();
        input->set_name("image");
        SetFloatTensorShape(input->mutable_type(), {1, inputChannels, imageHeight, imageWidth});
        auto *output = graph->add_output();
        output->set_name("rawHead");
        SetFloatTensorShape(output->mutable_type(), {1, rowCount, rowWidth});

        const auto conv1WeightResult = AddFloatInitializer(graph, "conv1.weight", conv1Weight, {16, inputChannels, 3, 3});
        if (!conv1WeightResult.IsSuccess()) return conv1WeightResult;
        const auto conv1BiasResult = AddFloatInitializer(graph, "conv1.bias", conv1Bias, {16});
        if (!conv1BiasResult.IsSuccess()) return conv1BiasResult;
        const auto conv2WeightResult = AddFloatInitializer(graph, "conv2.weight", conv2Weight, {32, 16, 3, 3});
        if (!conv2WeightResult.IsSuccess()) return conv2WeightResult;
        const auto conv2BiasResult = AddFloatInitializer(graph, "conv2.bias", conv2Bias, {32});
        if (!conv2BiasResult.IsSuccess()) return conv2BiasResult;
        const auto headWeightResult = AddFloatInitializer(graph, "head.weight", headWeight, {flatHeadCount, 32});
        if (!headWeightResult.IsSuccess()) return headWeightResult;
        const auto headBiasResult = AddFloatInitializer(graph, "head.bias", headBias, {flatHeadCount});
        if (!headBiasResult.IsSuccess()) return headBiasResult;
        const auto reshapeShapeResult = AddInt64Initializer(graph, "rawHead.shape", {1, rowCount, rowWidth}, {3});
        if (!reshapeShapeResult.IsSuccess()) return reshapeShapeResult;

        auto *conv1 = graph->add_node();
        conv1->set_name("conv1");
        conv1->set_op_type("Conv");
        conv1->add_input("image");
        conv1->add_input("conv1.weight");
        conv1->add_input("conv1.bias");
        conv1->add_output("conv1.out");
        AddIntsAttribute(conv1, "pads", {1, 1, 1, 1});
        AddIntsAttribute(conv1, "strides", {1, 1});

        auto *relu1 = graph->add_node();
        relu1->set_name("relu1");
        relu1->set_op_type("Relu");
        relu1->add_input("conv1.out");
        relu1->add_output("relu1.out");

        auto *conv2 = graph->add_node();
        conv2->set_name("conv2");
        conv2->set_op_type("Conv");
        conv2->add_input("relu1.out");
        conv2->add_input("conv2.weight");
        conv2->add_input("conv2.bias");
        conv2->add_output("conv2.out");
        AddIntsAttribute(conv2, "pads", {1, 1, 1, 1});
        AddIntsAttribute(conv2, "strides", {2, 2});

        auto *relu2 = graph->add_node();
        relu2->set_name("relu2");
        relu2->set_op_type("Relu");
        relu2->add_input("conv2.out");
        relu2->add_output("relu2.out");

        auto *pool = graph->add_node();
        pool->set_name("global_average_pool");
        pool->set_op_type("GlobalAveragePool");
        pool->add_input("relu2.out");
        pool->add_output("pool.out");

        auto *flatten = graph->add_node();
        flatten->set_name("flatten");
        flatten->set_op_type("Flatten");
        flatten->add_input("pool.out");
        flatten->add_output("flatten.out");
        AddIntAttribute(flatten, "axis", 1);

        auto *gemm = graph->add_node();
        gemm->set_name("head");
        gemm->set_op_type("Gemm");
        gemm->add_input("flatten.out");
        gemm->add_input("head.weight");
        gemm->add_input("head.bias");
        gemm->add_output("head.out");
        AddIntAttribute(gemm, "transB", 1);

        auto *reshape = graph->add_node();
        reshape->set_name("reshape_raw_head");
        reshape->set_op_type("Reshape");
        reshape->add_input("head.out");
        reshape->add_input("rawHead.shape");
        reshape->add_output("rawHead");

        ONNX_NAMESPACE::checker::check_model(modelProto, true);
        return WriteAndVerifyOnnx(path, modelProto, {9, 12, {QStringLiteral("Conv"), QStringLiteral("Relu"), QStringLiteral("GlobalAveragePool"), QStringLiteral("Flatten"), QStringLiteral("Gemm"), QStringLiteral("Reshape")}});
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("ONNX checker rejected YOLO11 tiny detector export: ") + error.what())); }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch YOLO11 tiny detector ONNX export failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("YOLO11 tiny detector ONNX export failed: ") + error.what())); }
}
}
