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
void SetFloatTensorShape(ONNX_NAMESPACE::TypeProto *type, const std::vector<int64_t> &dimensions)
{
    type->mutable_tensor_type()->set_elem_type(ONNX_NAMESPACE::TensorProto_DataType_FLOAT);
    auto *shape = type->mutable_tensor_type()->mutable_shape();
    for (const int64_t dimension : dimensions)
        shape->add_dim()->set_dim_value(dimension);
}

int64_t ElementCount(const std::vector<int64_t> &dimensions)
{
    int64_t count = 1;
    for (const int64_t dimension : dimensions)
    {
        if (dimension <= 0)
            return -1;
        count *= dimension;
    }
    return count;
}

foundation::Result<void> AddFloatInitializer(ONNX_NAMESPACE::GraphProto *graph,
                                             const char *name,
                                             const torch::Tensor &tensor,
                                             const std::vector<int64_t> &dimensions)
{
    const torch::Tensor materialized = tensor.detach().to(torch::kCPU).contiguous();
    const int64_t expectedElements = ElementCount(dimensions);
    if (!materialized.defined() || materialized.scalar_type() != torch::kFloat32 || expectedElements <= 0 ||
        materialized.numel() != expectedElements)
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::InvalidState,
            "ONNX exporter received a parameter tensor that does not match the requested float32 initializer shape"));
    auto *initializer = graph->add_initializer();
    initializer->set_name(name);
    initializer->set_data_type(ONNX_NAMESPACE::TensorProto_DataType_FLOAT);
    for (const int64_t dimension : dimensions)
        initializer->add_dims(dimension);
    const size_t byteCount = static_cast<size_t>(materialized.numel()) * sizeof(float);
    initializer->set_raw_data(std::string(static_cast<const char *>(materialized.data_ptr()), byteCount));
    return foundation::Result<void>::Success();
}

foundation::Result<void> AddInt64Initializer(ONNX_NAMESPACE::GraphProto *graph,
                                             const char *name,
                                             const std::vector<int64_t> &values,
                                             const std::vector<int64_t> &dimensions)
{
    const int64_t expectedElements = ElementCount(dimensions);
    if (values.empty() || expectedElements <= 0 || static_cast<int64_t>(values.size()) != expectedElements)
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::InvalidState,
            "ONNX exporter received an int64 initializer that does not match the requested shape"));
    auto *initializer = graph->add_initializer();
    initializer->set_name(name);
    initializer->set_data_type(ONNX_NAMESPACE::TensorProto_DataType_INT64);
    for (const int64_t dimension : dimensions)
        initializer->add_dims(dimension);
    initializer->set_raw_data(
        std::string(reinterpret_cast<const char *>(values.data()), values.size() * sizeof(int64_t)));
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
    for (const int64_t value : values)
        attribute->add_ints(value);
}

foundation::Result<void> ValidateModelProtoContract(const ONNX_NAMESPACE::ModelProto &modelProto,
                                                    const OnnxFileContract &contract)
{
    if (contract.irVersion <= 0 || contract.opsetVersion <= 0)
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "ONNX file contract versions must be positive"));
    if (modelProto.ir_version() != contract.irVersion)
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      "ONNX model IR version does not match the export contract"));
    if (modelProto.opset_import_size() != 1)
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      "ONNX model must declare exactly one opset import"));
    const auto &opset = modelProto.opset_import(0);
    if (!opset.domain().empty() && opset.domain() != "ai.onnx")
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      "ONNX model declares an unsupported opset domain"));
    if (opset.version() != contract.opsetVersion)
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      "ONNX model opset version does not match the export contract"));
    if (!contract.allowedOps.isEmpty())
    {
        QSet<QString> allowed;
        for (const QString &op : contract.allowedOps)
        {
            if (op.isEmpty())
                return foundation::Result<void>::Failure(
                    foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                              "ONNX allowed op list must not contain empty entries"));
            allowed.insert(op);
        }
        for (const auto &node : modelProto.graph().node())
        {
            if (!allowed.contains(QString::fromStdString(node.op_type())))
                return foundation::Result<void>::Failure(foundation::Error::Create(
                    foundation::ErrorCode::ProtocolViolation,
                    std::string("ONNX model uses an operator outside the export whitelist: ") + node.op_type()));
        }
    }
    ONNX_NAMESPACE::checker::check_model(modelProto, true);
    return foundation::Result<void>::Success();
}

foundation::Result<void>
WriteAndVerifyOnnx(const QString &path, const ONNX_NAMESPACE::ModelProto &modelProto, const OnnxFileContract &contract)
{
    std::string serialized;
    if (!modelProto.SerializeToString(&serialized) || serialized.empty())
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState, "ONNX model serialization failed"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::IoFailure,
            QStringLiteral("Unable to open ONNX output: ").append(file.errorString()).toStdString()));
    if (file.write(serialized.data(), static_cast<qint64>(serialized.size())) !=
            static_cast<qint64>(serialized.size()) ||
        !file.commit())
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::IoFailure,
            QStringLiteral("Unable to atomically write ONNX output: ").append(file.errorString()).toStdString()));
    QFile verificationFile(path);
    if (!verificationFile.open(QIODevice::ReadOnly))
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::IoFailure,
            QStringLiteral("Unable to reopen ONNX output: ").append(verificationFile.errorString()).toStdString()));
    ONNX_NAMESPACE::ModelProto verified;
    const QByteArray verificationBytes = verificationFile.readAll();
    if (!verified.ParseFromArray(verificationBytes.constData(), verificationBytes.size()))
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Written ONNX model cannot be parsed"));
    return ValidateModelProtoContract(verified, contract);
}

foundation::Result<torch::Tensor> FindNamedTensor(const torch::OrderedDict<std::string, torch::Tensor> &tensors,
                                                  const char *name)
{
    for (const auto &item : tensors)
    {
        if (item.key() == name)
        {
            return foundation::Result<torch::Tensor>::Success(item.value());
        }
    }
    return foundation::Result<torch::Tensor>::Failure(
        foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                  std::string("YOLO11 grid detector is missing required tensor: ") + name));
}

foundation::Result<void> AddBatchNormInitializers(ONNX_NAMESPACE::GraphProto *graph,
                                                  const char *prefix,
                                                  const torch::OrderedDict<std::string, torch::Tensor> &parameters,
                                                  const torch::OrderedDict<std::string, torch::Tensor> &buffers,
                                                  const int64_t channelCount)
{
    const auto weight = FindNamedTensor(parameters, (std::string(prefix) + ".weight").c_str());
    const auto bias = FindNamedTensor(parameters, (std::string(prefix) + ".bias").c_str());
    const auto mean = FindNamedTensor(buffers, (std::string(prefix) + ".running_mean").c_str());
    const auto variance = FindNamedTensor(buffers, (std::string(prefix) + ".running_var").c_str());
    if (!weight.IsSuccess())
        return foundation::Result<void>::Failure(weight.Failure());
    if (!bias.IsSuccess())
        return foundation::Result<void>::Failure(bias.Failure());
    if (!mean.IsSuccess())
        return foundation::Result<void>::Failure(mean.Failure());
    if (!variance.IsSuccess())
        return foundation::Result<void>::Failure(variance.Failure());
    const std::string base(prefix);
    const auto weightAdded = AddFloatInitializer(graph, (base + ".weight").c_str(), weight.Value(), {channelCount});
    if (!weightAdded.IsSuccess())
        return weightAdded;
    const auto biasAdded = AddFloatInitializer(graph, (base + ".bias").c_str(), bias.Value(), {channelCount});
    if (!biasAdded.IsSuccess())
        return biasAdded;
    const auto meanAdded = AddFloatInitializer(graph, (base + ".running_mean").c_str(), mean.Value(), {channelCount});
    if (!meanAdded.IsSuccess())
        return meanAdded;
    return AddFloatInitializer(graph, (base + ".running_var").c_str(), variance.Value(), {channelCount});
}

void AddConvNode(ONNX_NAMESPACE::GraphProto *graph,
                 const char *name,
                 const char *input,
                 const char *weight,
                 const char *output,
                 const int64_t stride,
                 const int64_t padding)
{
    auto *node = graph->add_node();
    node->set_name(name);
    node->set_op_type("Conv");
    node->add_input(input);
    node->add_input(weight);
    node->add_output(output);
    AddIntsAttribute(node, "strides", {stride, stride});
    AddIntsAttribute(node, "pads", {padding, padding, padding, padding});
}

void AddBatchNormSiluNodes(ONNX_NAMESPACE::GraphProto *graph, const char *prefix, const char *input, const char *output)
{
    const std::string base(prefix);
    const std::string normalized = base + ".normalized";
    const std::string sigmoid = base + ".sigmoid";
    auto *batchNorm = graph->add_node();
    batchNorm->set_name((base + ".batch_norm").c_str());
    batchNorm->set_op_type("BatchNormalization");
    batchNorm->add_input(input);
    batchNorm->add_input(base + ".weight");
    batchNorm->add_input(base + ".bias");
    batchNorm->add_input(base + ".running_mean");
    batchNorm->add_input(base + ".running_var");
    batchNorm->add_output(normalized);
    auto *sigmoidNode = graph->add_node();
    sigmoidNode->set_name((base + ".sigmoid").c_str());
    sigmoidNode->set_op_type("Sigmoid");
    sigmoidNode->add_input(normalized);
    sigmoidNode->add_output(sigmoid);
    auto *multiply = graph->add_node();
    multiply->set_name((base + ".silu").c_str());
    multiply->set_op_type("Mul");
    multiply->add_input(normalized);
    multiply->add_input(sigmoid);
    multiply->add_output(output);
}
} // namespace

foundation::Result<void> ValidateOnnxFileContract(const QString &path, const OnnxFileContract &contract)
{
    if (path.isEmpty())
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "ONNX file path must not be empty when validating export contract"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::IoFailure,
                                      QStringLiteral("Unable to read ONNX file for contract validation: ")
                                          .append(file.errorString())
                                          .toStdString()));
    ONNX_NAMESPACE::ModelProto modelProto;
    const QByteArray bytes = file.readAll();
    if (!modelProto.ParseFromArray(bytes.constData(), bytes.size()))
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      "ONNX file cannot be parsed during contract validation"));
    try
    {
        return ValidateModelProtoContract(modelProto, contract);
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolViolation,
                                      std::string("ONNX checker rejected contract validation: ") + error.what()));
    }
    catch (const std::exception &error)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                      std::string("ONNX contract validation failed: ") + error.what()));
    }
}

foundation::Result<void> ExportYolo11TinyDetectorOnnx(const QString &path,
                                                      models::yolo11::Yolo11TinyDetector &model,
                                                      const int64_t inputChannels,
                                                      const int64_t imageHeight,
                                                      const int64_t imageWidth,
                                                      const int64_t rowCount,
                                                      const int64_t classCount)
{
    if (path.isEmpty() || !model || inputChannels <= 0 || imageHeight <= 0 || imageWidth <= 0 || rowCount <= 0 ||
        classCount <= 0)
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "YOLO11 tiny detector ONNX export arguments are invalid"));
    if (!QFileInfo(path).dir().exists())
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "YOLO11 tiny detector ONNX export parent directory does not exist"));
    try
    {
        if (model->RowCount() != rowCount || model->ClassCount() != classCount)
            return foundation::Result<void>::Failure(foundation::Error::Create(
                foundation::ErrorCode::InvalidState,
                "YOLO11 tiny detector module contract does not match requested ONNX output shape"));
        const torch::Tensor conv1Weight = model->Conv1Weight();
        const torch::Tensor conv1Bias = model->Conv1Bias();
        const torch::Tensor conv2Weight = model->Conv2Weight();
        const torch::Tensor conv2Bias = model->Conv2Bias();
        const torch::Tensor headWeight = model->HeadWeight();
        const torch::Tensor headBias = model->HeadBias();
        if (conv1Weight.dim() != 4 || conv1Weight.size(0) != 16 || conv1Weight.size(1) != inputChannels ||
            conv1Weight.size(2) != 3 || conv1Weight.size(3) != 3 || conv1Bias.dim() != 1 || conv1Bias.size(0) != 16)
            return foundation::Result<void>::Failure(
                foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                          "YOLO11 tiny detector conv1 parameters do not match the ONNX contract"));
        if (conv2Weight.dim() != 4 || conv2Weight.size(0) != 32 || conv2Weight.size(1) != 16 ||
            conv2Weight.size(2) != 3 || conv2Weight.size(3) != 3 || conv2Bias.dim() != 1 || conv2Bias.size(0) != 32)
            return foundation::Result<void>::Failure(
                foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                          "YOLO11 tiny detector conv2 parameters do not match the ONNX contract"));
        const int64_t rowWidth = 4 + classCount;
        const int64_t flatHeadCount = rowCount * rowWidth;
        if (headWeight.dim() != 2 || headWeight.size(0) != flatHeadCount || headWeight.size(1) != 32 ||
            headBias.dim() != 1 || headBias.size(0) != flatHeadCount)
            return foundation::Result<void>::Failure(
                foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                          "YOLO11 tiny detector head parameters do not match the ONNX contract"));

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

        const auto conv1WeightResult =
            AddFloatInitializer(graph, "conv1.weight", conv1Weight, {16, inputChannels, 3, 3});
        if (!conv1WeightResult.IsSuccess())
            return conv1WeightResult;
        const auto conv1BiasResult = AddFloatInitializer(graph, "conv1.bias", conv1Bias, {16});
        if (!conv1BiasResult.IsSuccess())
            return conv1BiasResult;
        const auto conv2WeightResult = AddFloatInitializer(graph, "conv2.weight", conv2Weight, {32, 16, 3, 3});
        if (!conv2WeightResult.IsSuccess())
            return conv2WeightResult;
        const auto conv2BiasResult = AddFloatInitializer(graph, "conv2.bias", conv2Bias, {32});
        if (!conv2BiasResult.IsSuccess())
            return conv2BiasResult;
        const auto headWeightResult = AddFloatInitializer(graph, "head.weight", headWeight, {flatHeadCount, 32});
        if (!headWeightResult.IsSuccess())
            return headWeightResult;
        const auto headBiasResult = AddFloatInitializer(graph, "head.bias", headBias, {flatHeadCount});
        if (!headBiasResult.IsSuccess())
            return headBiasResult;
        const auto reshapeShapeResult = AddInt64Initializer(graph, "rawHead.shape", {1, rowCount, rowWidth}, {3});
        if (!reshapeShapeResult.IsSuccess())
            return reshapeShapeResult;

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
        return WriteAndVerifyOnnx(path,
                                  modelProto,
                                  {9,
                                   12,
                                   {QStringLiteral("Conv"),
                                    QStringLiteral("Relu"),
                                    QStringLiteral("GlobalAveragePool"),
                                    QStringLiteral("Flatten"),
                                    QStringLiteral("Gemm"),
                                    QStringLiteral("Reshape")}});
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::ProtocolViolation,
            std::string("ONNX checker rejected YOLO11 tiny detector export: ") + error.what()));
    }
    catch (const c10::Error &error)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::DependencyMissing,
            std::string("LibTorch YOLO11 tiny detector ONNX export failed: ") + error.what()));
    }
    catch (const std::exception &error)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                      std::string("YOLO11 tiny detector ONNX export failed: ") + error.what()));
    }
}

foundation::Result<void>
ExportYolo11GridDetectorOnnx(const QString &path, models::yolo11::Yolo11GridDetector &model, const int64_t classCount)
{
    constexpr int64_t inputChannels = 3;
    constexpr int64_t inputSize = 640;
    constexpr int64_t gridSize = 80;
    constexpr int64_t rowCount = gridSize * gridSize;
    if (path.isEmpty() || !model || classCount <= 0)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "YOLO11 grid detector ONNX export arguments are invalid"));
    }
    if (!QFileInfo(path).dir().exists())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument,
                                      "YOLO11 grid detector ONNX export parent directory does not exist"));
    }
    if (model->ClassCount() != classCount || model->OutputStride() != 8)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::InvalidState,
            "YOLO11 grid detector module does not satisfy the fixed 640 / 80x80 output contract"));
    }
    try
    {
        const auto parameters = model->named_parameters();
        const auto buffers = model->named_buffers();
        const auto backbone0 = FindNamedTensor(parameters, "backbone.0.weight");
        const auto backbone3 = FindNamedTensor(parameters, "backbone.3.weight");
        const auto backbone6 = FindNamedTensor(parameters, "backbone.6.weight");
        const auto neck0 = FindNamedTensor(parameters, "neck.0.weight");
        const auto neck3 = FindNamedTensor(parameters, "neck.3.weight");
        const auto headWeight = FindNamedTensor(parameters, "head.weight");
        const auto headBias = FindNamedTensor(parameters, "head.bias");
        if (!backbone0.IsSuccess())
            return foundation::Result<void>::Failure(backbone0.Failure());
        if (!backbone3.IsSuccess())
            return foundation::Result<void>::Failure(backbone3.Failure());
        if (!backbone6.IsSuccess())
            return foundation::Result<void>::Failure(backbone6.Failure());
        if (!neck0.IsSuccess())
            return foundation::Result<void>::Failure(neck0.Failure());
        if (!neck3.IsSuccess())
            return foundation::Result<void>::Failure(neck3.Failure());
        if (!headWeight.IsSuccess())
            return foundation::Result<void>::Failure(headWeight.Failure());
        if (!headBias.IsSuccess())
            return foundation::Result<void>::Failure(headBias.Failure());

        ONNX_NAMESPACE::ModelProto modelProto;
        modelProto.set_ir_version(9);
        modelProto.set_producer_name("VisionAIFlow");
        modelProto.set_producer_version("0.1.0");
        auto *opset = modelProto.add_opset_import();
        opset->set_domain("");
        opset->set_version(12);
        auto *graph = modelProto.mutable_graph();
        graph->set_name("yolo11_grid_detector_640");
        auto *input = graph->add_input();
        input->set_name("image");
        SetFloatTensorShape(input->mutable_type(), {1, inputChannels, inputSize, inputSize});
        auto *output = graph->add_output();
        output->set_name("rawHead");
        SetFloatTensorShape(output->mutable_type(), {1, rowCount, 4 + classCount});

        const auto addBackbone0 =
            AddFloatInitializer(graph, "backbone.0.weight", backbone0.Value(), {16, inputChannels, 3, 3});
        if (!addBackbone0.IsSuccess())
            return addBackbone0;
        const auto addBackbone3 = AddFloatInitializer(graph, "backbone.3.weight", backbone3.Value(), {32, 16, 3, 3});
        if (!addBackbone3.IsSuccess())
            return addBackbone3;
        const auto addBackbone6 = AddFloatInitializer(graph, "backbone.6.weight", backbone6.Value(), {64, 32, 3, 3});
        if (!addBackbone6.IsSuccess())
            return addBackbone6;
        const auto addNeck0 = AddFloatInitializer(graph, "neck.0.weight", neck0.Value(), {64, 64, 1, 1});
        if (!addNeck0.IsSuccess())
            return addNeck0;
        const auto addNeck3 = AddFloatInitializer(graph, "neck.3.weight", neck3.Value(), {64, 64, 3, 3});
        if (!addNeck3.IsSuccess())
            return addNeck3;
        const auto addHeadWeight =
            AddFloatInitializer(graph, "head.weight", headWeight.Value(), {4 + classCount, 64, 1, 1});
        if (!addHeadWeight.IsSuccess())
            return addHeadWeight;
        const auto addHeadBias = AddFloatInitializer(graph, "head.bias", headBias.Value(), {4 + classCount});
        if (!addHeadBias.IsSuccess())
            return addHeadBias;
        const auto addBackbone1 = AddBatchNormInitializers(graph, "backbone.1", parameters, buffers, 16);
        if (!addBackbone1.IsSuccess())
            return addBackbone1;
        const auto addBackbone4 = AddBatchNormInitializers(graph, "backbone.4", parameters, buffers, 32);
        if (!addBackbone4.IsSuccess())
            return addBackbone4;
        const auto addBackbone7 = AddBatchNormInitializers(graph, "backbone.7", parameters, buffers, 64);
        if (!addBackbone7.IsSuccess())
            return addBackbone7;
        const auto addNeck1 = AddBatchNormInitializers(graph, "neck.1", parameters, buffers, 64);
        if (!addNeck1.IsSuccess())
            return addNeck1;
        const auto addNeck4 = AddBatchNormInitializers(graph, "neck.4", parameters, buffers, 64);
        if (!addNeck4.IsSuccess())
            return addNeck4;
        const auto transposeOrder = AddInt64Initializer(graph, "rawHead.transpose_order", {0, 2, 3, 1}, {4});
        if (!transposeOrder.IsSuccess())
            return transposeOrder;
        const auto reshapeShape = AddInt64Initializer(graph, "rawHead.shape", {1, rowCount, 4 + classCount}, {3});
        if (!reshapeShape.IsSuccess())
            return reshapeShape;

        AddConvNode(graph, "backbone.0", "image", "backbone.0.weight", "backbone.0.out", 2, 1);
        AddBatchNormSiluNodes(graph, "backbone.1", "backbone.0.out", "backbone.1.out");
        AddConvNode(graph, "backbone.3", "backbone.1.out", "backbone.3.weight", "backbone.3.out", 2, 1);
        AddBatchNormSiluNodes(graph, "backbone.4", "backbone.3.out", "backbone.4.out");
        AddConvNode(graph, "backbone.6", "backbone.4.out", "backbone.6.weight", "backbone.6.out", 2, 1);
        AddBatchNormSiluNodes(graph, "backbone.7", "backbone.6.out", "backbone.7.out");
        AddConvNode(graph, "neck.0", "backbone.7.out", "neck.0.weight", "neck.0.out", 1, 0);
        AddBatchNormSiluNodes(graph, "neck.1", "neck.0.out", "neck.1.out");
        AddConvNode(graph, "neck.3", "neck.1.out", "neck.3.weight", "neck.3.out", 1, 1);
        AddBatchNormSiluNodes(graph, "neck.4", "neck.3.out", "neck.4.out");
        auto *head = graph->add_node();
        head->set_name("head");
        head->set_op_type("Conv");
        head->add_input("neck.4.out");
        head->add_input("head.weight");
        head->add_input("head.bias");
        head->add_output("head.out");
        auto *transpose = graph->add_node();
        transpose->set_name("transpose_raw_head");
        transpose->set_op_type("Transpose");
        transpose->add_input("head.out");
        transpose->add_output("rawHead.transposed");
        AddIntsAttribute(transpose, "perm", {0, 2, 3, 1});
        auto *reshape = graph->add_node();
        reshape->set_name("reshape_raw_head");
        reshape->set_op_type("Reshape");
        reshape->add_input("rawHead.transposed");
        reshape->add_input("rawHead.shape");
        reshape->add_output("rawHead");

        ONNX_NAMESPACE::checker::check_model(modelProto, true);
        return WriteAndVerifyOnnx(path,
                                  modelProto,
                                  {9,
                                   12,
                                   {QStringLiteral("Conv"),
                                    QStringLiteral("BatchNormalization"),
                                    QStringLiteral("Sigmoid"),
                                    QStringLiteral("Mul"),
                                    QStringLiteral("Transpose"),
                                    QStringLiteral("Reshape")}});
    }
    catch (const ONNX_NAMESPACE::checker::ValidationError &error)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::ProtocolViolation,
            std::string("ONNX checker rejected YOLO11 grid detector export: ") + error.what()));
    }
    catch (const c10::Error &error)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::DependencyMissing,
            std::string("LibTorch YOLO11 grid detector ONNX export failed: ") + error.what()));
    }
    catch (const std::exception &error)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState,
                                      std::string("YOLO11 grid detector ONNX export failed: ") + error.what()));
    }
}
} // namespace visionaiflow::exporter
