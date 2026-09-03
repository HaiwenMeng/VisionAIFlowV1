#include "yolov11onnxconverter.h"

#ifdef slots
#undef slots
#endif

#include <torch/torch.h>
#include <torch/csrc/jit/ir/constants.h>
#include <torch/csrc/jit/ir/ir.h>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace visionaiflow::yolov11
{
namespace
{
using torch::jit::Graph;
using torch::jit::Node;
using torch::jit::Value;

c10::Symbol OnnxSymbol(const char *name)
{
    return c10::Symbol::fromQualString((std::string("onnx::") + name).c_str());
}

c10::Symbol Attribute(const char *name)
{
    return c10::Symbol::attr(name);
}

std::string KindName(const Node *node)
{
    return node->kind().toQualString();
}

std::optional<c10::IValue> ConstantValue(const Value *value)
{
    return torch::jit::toIValue(value);
}

int64_t ConstantInt(const Value *value, const char *argumentName)
{
    const auto constant = ConstantValue(value);
    if (!constant.has_value() || !constant->isInt())
    {
        throw std::runtime_error(std::string(argumentName) + " must be a constant integer");
    }
    return constant->toInt();
}

double ConstantDouble(const Value *value, const char *argumentName)
{
    const auto constant = ConstantValue(value);
    if (!constant.has_value())
    {
        throw std::runtime_error(std::string(argumentName) + " must be a constant number");
    }
    if (constant->isDouble())
    {
        return constant->toDouble();
    }
    if (constant->isInt())
    {
        return static_cast<double>(constant->toInt());
    }
    throw std::runtime_error(std::string(argumentName) + " must be a constant number");
}

bool ConstantBool(const Value *value, const char *argumentName)
{
    const auto constant = ConstantValue(value);
    if (!constant.has_value() || !constant->isBool())
    {
        throw std::runtime_error(std::string(argumentName) + " must be a constant boolean");
    }
    return constant->toBool();
}

bool IsNone(const Value *value)
{
    const auto constant = ConstantValue(value);
    return constant.has_value() && constant->isNone();
}

std::vector<int64_t> ConstantIntList(const Value *value, const char *argumentName)
{
    const auto constant = ConstantValue(value);
    if (constant.has_value() && constant->isIntList())
    {
        return constant->toIntVector();
    }
    const Node *producer = value->node();
    if (KindName(producer) != "prim::ListConstruct")
    {
        throw std::runtime_error(std::string(argumentName) + " must be a constant integer list");
    }
    std::vector<int64_t> values;
    values.reserve(producer->inputs().size());
    for (const Value *input : producer->inputs())
    {
        values.push_back(ConstantInt(input, argumentName));
    }
    return values;
}

std::vector<int64_t> TensorSizes(const Value *value, const char *argumentName)
{
    const auto tensorType = value->type()->cast<c10::TensorType>();
    if (!tensorType)
    {
        throw std::runtime_error(std::string(argumentName) + " is not a tensor");
    }
    const auto sizes = tensorType->sizes().concrete_sizes();
    if (!sizes.has_value())
    {
        throw std::runtime_error(std::string(argumentName) + " has no static shape");
    }
    return *sizes;
}

Node *CreateOnnxNode(Graph *graph,
                     Node *before,
                     const char *name,
                     const std::vector<Value *> &inputs,
                     const size_t outputCount)
{
    Node *node = graph->create(OnnxSymbol(name), inputs, outputCount);
    node->insertBefore(before);
    node->copyMetadata(before);
    return node;
}

Value *CreateTensorConstant(Graph *graph, Node *before, const torch::Tensor &tensor)
{
    Node *constant = graph->create(OnnxSymbol("Constant"), 1);
    constant->t_(Attribute("value"), tensor.detach().cpu().contiguous());
    constant->insertBefore(before);
    constant->output()->setType(c10::TensorType::create(tensor.detach().cpu()));
    return constant->output();
}

Value *CreateInt64Constant(Graph *graph, Node *before, const std::vector<int64_t> &values)
{
    const torch::Tensor tensor = values.empty() ? torch::empty({0}, torch::TensorOptions().dtype(torch::kInt64))
                                                : torch::tensor(values, torch::TensorOptions().dtype(torch::kInt64));
    return CreateTensorConstant(graph, before, tensor);
}

Value *CreateFloatConstant(Graph *graph, Node *before, const std::vector<float> &values)
{
    const torch::Tensor tensor = values.empty() ? torch::empty({0}, torch::TensorOptions().dtype(torch::kFloat32))
                                                : torch::tensor(values, torch::TensorOptions().dtype(torch::kFloat32));
    return CreateTensorConstant(graph, before, tensor);
}

Value *CreateScalarConstant(Graph *graph, Node *before, const Value *value, const Value *reference)
{
    if (value->type()->cast<c10::TensorType>())
    {
        return const_cast<Value *>(value);
    }
    const auto constant = ConstantValue(value);
    if (!constant.has_value())
    {
        throw std::runtime_error("arithmetic scalar must be constant");
    }
    const auto tensorType = reference->type()->cast<c10::TensorType>();
    const at::ScalarType scalarType =
        tensorType && tensorType->scalarType().has_value() ? *tensorType->scalarType() : torch::kFloat32;
    torch::Tensor tensor;
    if (constant->isDouble())
    {
        tensor = torch::scalar_tensor(constant->toDouble(), torch::TensorOptions().dtype(scalarType));
    }
    else if (constant->isInt())
    {
        tensor = torch::scalar_tensor(constant->toInt(), torch::TensorOptions().dtype(scalarType));
    }
    else if (constant->isBool())
    {
        tensor = torch::scalar_tensor(constant->toBool(), torch::TensorOptions().dtype(scalarType));
    }
    else
    {
        throw std::runtime_error("arithmetic scalar has an unsupported constant type");
    }
    return CreateTensorConstant(graph, before, tensor);
}

void ReplaceOutput(Node *oldNode, Value *newValue)
{
    newValue->copyMetadata(oldNode->output());
    oldNode->output()->replaceAllUsesWith(newValue);
    oldNode->destroy();
}

void ReplaceWithUnary(Graph *graph, Node *oldNode, const char *onnxName)
{
    Node *replacement = CreateOnnxNode(graph, oldNode, onnxName, {oldNode->input(0)}, 1);
    ReplaceOutput(oldNode, replacement->output());
}

void ConvertConvolution(Graph *graph, Node *node)
{
    if (node->inputs().size() < 9)
    {
        throw std::runtime_error("aten::_convolution has an invalid signature");
    }
    const bool transposed = ConstantBool(node->input(6), "convolution transposed");
    const std::vector<int64_t> weightSizes = TensorSizes(node->input(1), "convolution weight");
    if (weightSizes.size() < 3)
    {
        throw std::runtime_error("convolution weight rank is invalid");
    }
    std::vector<Value *> inputs{node->input(0), node->input(1)};
    if (!IsNone(node->input(2)))
    {
        inputs.push_back(node->input(2));
    }
    Node *replacement = CreateOnnxNode(graph, node, transposed ? "ConvTranspose" : "Conv", inputs, 1);
    const std::vector<int64_t> stride = ConstantIntList(node->input(3), "convolution stride");
    const std::vector<int64_t> padding = ConstantIntList(node->input(4), "convolution padding");
    const std::vector<int64_t> dilation = ConstantIntList(node->input(5), "convolution dilation");
    std::vector<int64_t> pads = padding;
    pads.insert(pads.end(), padding.begin(), padding.end());
    replacement->is_(Attribute("kernel_shape"), {weightSizes.begin() + 2, weightSizes.end()});
    replacement->is_(Attribute("strides"), stride);
    replacement->is_(Attribute("pads"), pads);
    replacement->is_(Attribute("dilations"), dilation);
    replacement->i_(Attribute("group"), ConstantInt(node->input(8), "convolution groups"));
    if (transposed)
    {
        replacement->is_(Attribute("output_padding"), ConstantIntList(node->input(7), "convolution output padding"));
    }
    ReplaceOutput(node, replacement->output());
}

void ConvertBatchNorm(Graph *graph, Node *node)
{
    if (node->inputs().size() < 8 || ConstantBool(node->input(5), "batch norm training"))
    {
        throw std::runtime_error("only inference-mode batch normalization can be exported");
    }
    Node *replacement = CreateOnnxNode(graph,
                                       node,
                                       "BatchNormalization",
                                       {node->input(0), node->input(1), node->input(2), node->input(3), node->input(4)},
                                       1);
    replacement->f_(Attribute("epsilon"), ConstantDouble(node->input(7), "batch norm epsilon"));
    replacement->f_(Attribute("momentum"), 1.0 - ConstantDouble(node->input(6), "batch norm momentum"));
    ReplaceOutput(node, replacement->output());
}

void ConvertArithmetic(Graph *graph, Node *node, const char *onnxName)
{
    if ((KindName(node) == "aten::add" || KindName(node) == "aten::sub") && node->inputs().size() > 2 &&
        std::abs(ConstantDouble(node->input(2), "arithmetic alpha") - 1.0) > 1.0e-12)
    {
        throw std::runtime_error("ONNX export only supports add/sub alpha=1");
    }
    Value *right = CreateScalarConstant(graph, node, node->input(1), node->input(0));
    Node *replacement = CreateOnnxNode(graph, node, onnxName, {node->input(0), right}, 1);
    ReplaceOutput(node, replacement->output());
}

void ConvertSilu(Graph *graph, Node *node)
{
    Node *sigmoid = CreateOnnxNode(graph, node, "Sigmoid", {node->input(0)}, 1);
    Node *multiply = CreateOnnxNode(graph, node, "Mul", {node->input(0), sigmoid->output()}, 1);
    ReplaceOutput(node, multiply->output());
}

void ConvertMaxPool(Graph *graph, Node *node)
{
    Node *replacement = CreateOnnxNode(graph, node, "MaxPool", {node->input(0)}, 1);
    const std::vector<int64_t> padding = ConstantIntList(node->input(3), "max pool padding");
    std::vector<int64_t> pads = padding;
    pads.insert(pads.end(), padding.begin(), padding.end());
    replacement->is_(Attribute("kernel_shape"), ConstantIntList(node->input(1), "max pool kernel"));
    replacement->is_(Attribute("strides"), ConstantIntList(node->input(2), "max pool stride"));
    replacement->is_(Attribute("pads"), pads);
    replacement->is_(Attribute("dilations"), ConstantIntList(node->input(4), "max pool dilation"));
    replacement->i_(Attribute("ceil_mode"), ConstantBool(node->input(5), "max pool ceil mode") ? 1 : 0);
    ReplaceOutput(node, replacement->output());
}

void ConvertView(Graph *graph, Node *node)
{
    Value *shape = CreateInt64Constant(graph, node, ConstantIntList(node->input(1), "view shape"));
    Node *replacement = CreateOnnxNode(graph, node, "Reshape", {node->input(0), shape}, 1);
    ReplaceOutput(node, replacement->output());
}

void ConvertTranspose(Graph *graph, Node *node)
{
    const std::vector<int64_t> sizes = TensorSizes(node->input(0), "transpose input");
    int64_t first = ConstantInt(node->input(1), "transpose first axis");
    int64_t second = ConstantInt(node->input(2), "transpose second axis");
    const int64_t rank = static_cast<int64_t>(sizes.size());
    first = first < 0 ? first + rank : first;
    second = second < 0 ? second + rank : second;
    if (first < 0 || first >= rank || second < 0 || second >= rank)
    {
        throw std::runtime_error("transpose axis is outside tensor rank");
    }
    std::vector<int64_t> permutation(static_cast<size_t>(rank));
    for (int64_t index = 0; index < rank; ++index)
    {
        permutation.at(static_cast<size_t>(index)) = index;
    }
    std::swap(permutation.at(static_cast<size_t>(first)), permutation.at(static_cast<size_t>(second)));
    Node *replacement = CreateOnnxNode(graph, node, "Transpose", {node->input(0)}, 1);
    replacement->is_(Attribute("perm"), permutation);
    ReplaceOutput(node, replacement->output());
}

void ConvertResize(Graph *graph, Node *node)
{
    const std::vector<int64_t> outputSizes = TensorSizes(node->output(), "upsample output");
    Value *roi = CreateFloatConstant(graph, node, {});
    Value *scales = CreateFloatConstant(graph, node, {});
    Value *sizes = CreateInt64Constant(graph, node, outputSizes);
    Node *replacement = CreateOnnxNode(graph, node, "Resize", {node->input(0), roi, scales, sizes}, 1);
    replacement->s_(Attribute("coordinate_transformation_mode"), "asymmetric");
    replacement->s_(Attribute("mode"), "nearest");
    replacement->s_(Attribute("nearest_mode"), "floor");
    ReplaceOutput(node, replacement->output());
}

void ConvertArange(Graph *graph, Node *node)
{
    if (node->inputs().empty())
    {
        throw std::runtime_error("arange has no end value");
    }
    const int64_t end = ConstantInt(node->input(0), "arange end");
    const torch::Tensor value = torch::arange(end, torch::TensorOptions().dtype(torch::kFloat32));
    ReplaceOutput(node, CreateTensorConstant(graph, node, value));
}

void ConvertSlice(Graph *graph, Node *node)
{
    const int64_t start = IsNone(node->input(2)) ? 0 : ConstantInt(node->input(2), "slice start");
    const int64_t end =
        IsNone(node->input(3)) ? std::numeric_limits<int64_t>::max() : ConstantInt(node->input(3), "slice end");
    Value *starts = CreateInt64Constant(graph, node, {start});
    Value *ends = CreateInt64Constant(graph, node, {end});
    Value *axes = CreateInt64Constant(graph, node, {ConstantInt(node->input(1), "slice axis")});
    Value *steps = CreateInt64Constant(graph, node, {ConstantInt(node->input(4), "slice step")});
    Node *replacement = CreateOnnxNode(graph, node, "Slice", {node->input(0), starts, ends, axes, steps}, 1);
    ReplaceOutput(node, replacement->output());
}

void ConvertUnsqueeze(Graph *graph, Node *node)
{
    Node *replacement = CreateOnnxNode(graph, node, "Unsqueeze", {node->input(0)}, 1);
    replacement->is_(Attribute("axes"), {ConstantInt(node->input(1), "unsqueeze axis")});
    ReplaceOutput(node, replacement->output());
}

void ConvertReduceSum(Graph *graph, Node *node)
{
    Node *replacement = CreateOnnxNode(graph, node, "ReduceSum", {node->input(0)}, 1);
    replacement->is_(Attribute("axes"), ConstantIntList(node->input(1), "sum axes"));
    replacement->i_(Attribute("keepdims"), ConstantBool(node->input(2), "sum keep dimensions") ? 1 : 0);
    ReplaceOutput(node, replacement->output());
}

void ConvertSoftmax(Graph *graph, Node *node)
{
    const int64_t rank = static_cast<int64_t>(TensorSizes(node->input(0), "softmax input").size());
    int64_t axis = ConstantInt(node->input(1), "softmax axis");
    axis = axis < 0 ? axis + rank : axis;
    if (axis < 0 || axis >= rank)
    {
        throw std::runtime_error("softmax axis is outside tensor rank");
    }
    if (axis == rank - 1)
    {
        Node *replacement = CreateOnnxNode(graph, node, "Softmax", {node->input(0)}, 1);
        replacement->i_(Attribute("axis"), axis);
        ReplaceOutput(node, replacement->output());
        return;
    }

    std::vector<int64_t> permutation(static_cast<size_t>(rank));
    for (int64_t index = 0; index < rank; ++index)
    {
        permutation.at(static_cast<size_t>(index)) = index;
    }
    std::swap(permutation.at(static_cast<size_t>(axis)), permutation.back());
    Node *toLast = CreateOnnxNode(graph, node, "Transpose", {node->input(0)}, 1);
    toLast->is_(Attribute("perm"), permutation);
    Node *softmax = CreateOnnxNode(graph, node, "Softmax", {toLast->output()}, 1);
    softmax->i_(Attribute("axis"), rank - 1);
    Node *restore = CreateOnnxNode(graph, node, "Transpose", {softmax->output()}, 1);
    restore->is_(Attribute("perm"), permutation);
    ReplaceOutput(node, restore->output());
}

void ConvertCat(Graph *graph, Node *node)
{
    Node *list = node->input(0)->node();
    if (KindName(list) != "prim::ListConstruct")
    {
        throw std::runtime_error("cat tensor list is not static");
    }
    std::vector<Value *> tensors(list->inputs().begin(), list->inputs().end());
    Node *replacement = CreateOnnxNode(graph, node, "Concat", tensors, 1);
    replacement->i_(Attribute("axis"), ConstantInt(node->input(1), "cat axis"));
    ReplaceOutput(node, replacement->output());
}

void ConvertSplit(Graph *graph, Node *node, std::unordered_set<Node *> *destroyed)
{
    const auto &uses = node->output()->uses();
    if (uses.size() != 1 || KindName(uses.front().user) != "prim::ListUnpack")
    {
        throw std::runtime_error("split/chunk output is not a static ListUnpack");
    }
    Node *unpack = uses.front().user;
    const int64_t axis = KindName(node) == "aten::chunk" ? ConstantInt(node->input(2), "chunk axis")
                                                         : ConstantInt(node->input(2), "split axis");
    std::vector<int64_t> splitSizes;
    if (KindName(node) == "aten::split_with_sizes")
    {
        splitSizes = ConstantIntList(node->input(1), "split sizes");
    }
    else
    {
        splitSizes.reserve(unpack->outputs().size());
        for (Value *output : unpack->outputs())
        {
            const std::vector<int64_t> sizes = TensorSizes(output, "chunk output");
            const int64_t normalizedAxis = axis < 0 ? axis + static_cast<int64_t>(sizes.size()) : axis;
            if (normalizedAxis < 0 || normalizedAxis >= static_cast<int64_t>(sizes.size()))
            {
                throw std::runtime_error("chunk axis is outside tensor rank");
            }
            splitSizes.push_back(sizes.at(static_cast<size_t>(normalizedAxis)));
        }
    }
    if (splitSizes.size() != unpack->outputs().size())
    {
        throw std::runtime_error("split size count does not match output count");
    }
    Node *replacement = CreateOnnxNode(graph, node, "Split", {node->input(0)}, unpack->outputs().size());
    replacement->i_(Attribute("axis"), axis);
    replacement->is_(Attribute("split"), splitSizes);
    for (size_t index = 0; index < unpack->outputs().size(); ++index)
    {
        replacement->output(index)->copyMetadata(unpack->output(index));
        unpack->output(index)->replaceAllUsesWith(replacement->output(index));
    }
    destroyed->insert(unpack);
    destroyed->insert(node);
    unpack->destroy();
    node->destroy();
}

void RemoveUnusedPrimitiveNodes(Graph *graph)
{
    std::vector<Node *> nodes(graph->nodes().begin(), graph->nodes().end());
    for (auto iterator = nodes.rbegin(); iterator != nodes.rend(); ++iterator)
    {
        Node *node = *iterator;
        const std::string kind = KindName(node);
        if (kind != "prim::Constant" && kind != "prim::ListConstruct")
        {
            continue;
        }
        bool hasUses = false;
        for (Value *output : node->outputs())
        {
            hasUses = hasUses || !output->uses().empty();
        }
        if (!hasUses)
        {
            node->destroy();
        }
    }
}

void ConvertTensorConstants(Graph *graph)
{
    std::vector<Node *> nodes(graph->nodes().begin(), graph->nodes().end());
    for (Node *node : nodes)
    {
        if (KindName(node) != "prim::Constant" || node->outputs().empty() || node->output()->uses().empty())
        {
            continue;
        }
        const auto constant = ConstantValue(node->output());
        if (!constant.has_value() || !constant->isTensor())
        {
            std::string users;
            for (const torch::jit::Use &use : node->output()->uses())
            {
                users += users.empty() ? KindName(use.user) : std::string(", ") + KindName(use.user);
            }
            throw std::runtime_error("used prim::Constant has type " + node->output()->type()->str() +
                                     " after YOLO11 ONNX conversion; users: " + users);
        }
        Value *replacement = CreateTensorConstant(graph, node, constant->toTensor());
        replacement->copyMetadata(node->output());
        node->output()->replaceAllUsesWith(replacement);
        node->destroy();
    }
}

void ConvertNode(Graph *graph, Node *node, std::unordered_set<Node *> *destroyed)
{
    const std::string kind = KindName(node);
    if (kind == "aten::_convolution")
    {
        ConvertConvolution(graph, node);
    }
    else if (kind == "aten::batch_norm")
    {
        ConvertBatchNorm(graph, node);
    }
    else if (kind == "aten::silu")
    {
        ConvertSilu(graph, node);
    }
    else if (kind == "aten::max_pool2d")
    {
        ConvertMaxPool(graph, node);
    }
    else if (kind == "aten::view" || kind == "aten::reshape")
    {
        ConvertView(graph, node);
    }
    else if (kind == "aten::transpose")
    {
        ConvertTranspose(graph, node);
    }
    else if (kind == "aten::upsample_nearest2d")
    {
        ConvertResize(graph, node);
    }
    else if (kind == "aten::clone")
    {
        Value *input = node->input(0);
        input->copyMetadata(node->output());
        node->output()->replaceAllUsesWith(input);
        node->destroy();
    }
    else if (kind == "aten::arange")
    {
        ConvertArange(graph, node);
    }
    else if (kind == "aten::slice")
    {
        ConvertSlice(graph, node);
    }
    else if (kind == "aten::unsqueeze")
    {
        ConvertUnsqueeze(graph, node);
    }
    else if (kind == "aten::sum")
    {
        ConvertReduceSum(graph, node);
    }
    else if (kind == "aten::softmax")
    {
        ConvertSoftmax(graph, node);
    }
    else if (kind == "aten::cat")
    {
        ConvertCat(graph, node);
    }
    else if (kind == "aten::chunk" || kind == "aten::split_with_sizes")
    {
        ConvertSplit(graph, node, destroyed);
    }
    else if (kind == "aten::add")
    {
        ConvertArithmetic(graph, node, "Add");
    }
    else if (kind == "aten::sub")
    {
        ConvertArithmetic(graph, node, "Sub");
    }
    else if (kind == "aten::mul")
    {
        ConvertArithmetic(graph, node, "Mul");
    }
    else if (kind == "aten::div")
    {
        ConvertArithmetic(graph, node, "Div");
    }
    else if (kind == "aten::matmul")
    {
        Node *replacement = CreateOnnxNode(graph, node, "MatMul", {node->input(0), node->input(1)}, 1);
        ReplaceOutput(node, replacement->output());
    }
    else if (kind == "aten::sigmoid")
    {
        ReplaceWithUnary(graph, node, "Sigmoid");
    }
}
} // namespace

bool ConvertYolo11TraceToOnnx(const std::shared_ptr<torch::jit::Graph> &graph, QString *errorMessage)
{
    if (!graph || !errorMessage)
    {
        return false;
    }
    try
    {
        std::vector<Node *> nodes(graph->nodes().begin(), graph->nodes().end());
        std::unordered_set<Node *> destroyed;
        for (Node *node : nodes)
        {
            if (destroyed.count(node) == 0)
            {
                ConvertNode(graph.get(), node, &destroyed);
            }
        }
        RemoveUnusedPrimitiveNodes(graph.get());
        ConvertTensorConstants(graph.get());
        RemoveUnusedPrimitiveNodes(graph.get());

        std::set<std::string> unsupported;
        for (Node *node : graph->nodes())
        {
            const std::string kind = KindName(node);
            if (kind.rfind("aten::", 0) == 0 || kind.rfind("prim::", 0) == 0)
            {
                unsupported.insert(kind);
            }
        }
        if (!unsupported.empty())
        {
            QStringList names;
            for (const std::string &kind : unsupported)
            {
                names.append(QString::fromStdString(kind));
            }
            *errorMessage = QString(u8"YOLO11 ONNX 图转换仍包含未支持算子: %1").arg(names.join(QStringLiteral(", ")));
            return false;
        }
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLO11 ONNX 图转换失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLO11 ONNX 图转换失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}
} // namespace visionaiflow::yolov11
