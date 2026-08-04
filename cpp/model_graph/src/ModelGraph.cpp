#include "visionaiflow/model_graph/ModelGraph.h"

#include <algorithm>
#include <limits>

namespace visionaiflow::model_graph
{
namespace
{
foundation::Result<int64_t> CheckedMultiply(const int64_t left, const int64_t right)
{
    if (left <= 0 || right <= 0 || left > std::numeric_limits<int64_t>::max() / right) return foundation::Result<int64_t>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Tensor dimension multiplication overflowed"));
    return foundation::Result<int64_t>::Success(left * right);
}

foundation::Result<int64_t> ElementCount(const std::vector<int64_t> &dimensions)
{
    int64_t count = 1;
    for (const int64_t dimension : dimensions)
    {
        const auto multiplied = CheckedMultiply(count, dimension);
        if (!multiplied.IsSuccess()) return multiplied;
        count = multiplied.Value();
    }
    return foundation::Result<int64_t>::Success(count);
}

foundation::Result<int64_t> NormalizeAxis(const int64_t axis, const size_t rank, const bool allowEnd = false)
{
    const int64_t upper = static_cast<int64_t>(rank) + (allowEnd ? 1 : 0);
    const int64_t normalized = axis < 0 ? axis + upper : axis;
    if (normalized < 0 || normalized >= upper) return foundation::Result<int64_t>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Tensor axis is outside the input rank"));
    return foundation::Result<int64_t>::Success(normalized);
}

foundation::Result<std::vector<int64_t>> NormalizeAxes(const std::vector<int64_t> &axes, const size_t rank, const bool allowEnd = false)
{
    std::vector<int64_t> normalizedAxes;
    normalizedAxes.reserve(axes.size());
    for (const int64_t axis : axes)
    {
        const auto normalized = NormalizeAxis(axis, rank, allowEnd);
        if (!normalized.IsSuccess()) return foundation::Result<std::vector<int64_t>>::Failure(normalized.Failure());
        if (std::find(normalizedAxes.begin(), normalizedAxes.end(), normalized.Value()) != normalizedAxes.end()) return foundation::Result<std::vector<int64_t>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Tensor axes must be unique"));
        normalizedAxes.push_back(normalized.Value());
    }
    std::sort(normalizedAxes.begin(), normalizedAxes.end());
    return foundation::Result<std::vector<int64_t>>::Success(std::move(normalizedAxes));
}

foundation::Result<TensorShape> InferPool2dShape(const TensorShape &input, const GraphNode &node, const char *name)
{
    if (input.dimensions.size() != 4U) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(name) + " requires NCHW input"));
    if (node.kernelSize <= 0 || node.stride <= 0 || node.padding < 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(name) + " parameters are invalid"));
    const int64_t heightNumerator = input.dimensions[2] + 2 * node.padding - node.kernelSize;
    const int64_t widthNumerator = input.dimensions[3] + 2 * node.padding - node.kernelSize;
    if (heightNumerator < 0 || widthNumerator < 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(name) + " kernel exceeds padded input"));
    return foundation::Result<TensorShape>::Success({{input.dimensions[0], input.dimensions[1], heightNumerator / node.stride + 1, widthNumerator / node.stride + 1}});
}

foundation::Result<TensorShape> InferReshapeShape(const TensorShape &input, const GraphNode &node)
{
    if (node.outputShape.empty()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape requires a target output shape"));
    const auto inputElements = ElementCount(input.dimensions);
    if (!inputElements.IsSuccess()) return foundation::Result<TensorShape>::Failure(inputElements.Failure());
    std::vector<int64_t> result = node.outputShape;
    int inferIndex = -1;
    int64_t knownElements = 1;
    for (size_t index = 0; index < result.size(); ++index)
    {
        if (result[index] == -1)
        {
            if (inferIndex >= 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape may contain at most one inferred dimension"));
            inferIndex = static_cast<int>(index);
        }
        else
        {
            if (result[index] <= 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape target dimensions must be positive except for one -1"));
            const auto multiplied = CheckedMultiply(knownElements, result[index]);
            if (!multiplied.IsSuccess()) return foundation::Result<TensorShape>::Failure(multiplied.Failure());
            knownElements = multiplied.Value();
        }
    }
    if (inferIndex >= 0)
    {
        if (inputElements.Value() % knownElements != 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape inferred dimension is not integral"));
        result[static_cast<size_t>(inferIndex)] = inputElements.Value() / knownElements;
    }
    else if (knownElements != inputElements.Value()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape target element count does not match input"));
    return foundation::Result<TensorShape>::Success({std::move(result)});
}
}

foundation::Result<void> ValidateShape(const TensorShape &shape)
{
    if (shape.dimensions.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Tensor shape must contain at least one dimension"));
    for (const int64_t dimension : shape.dimensions)
    {
        if (dimension <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Tensor dimensions must be positive"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<TensorShape> InferNodeShape(const TensorShape &input, const GraphNode &node)
{
    const auto inputValidation = ValidateShape(input);
    if (!inputValidation.IsSuccess()) return foundation::Result<TensorShape>::Failure(inputValidation.Failure());
    if (node.kind == NodeKind::Relu || node.kind == NodeKind::Activation || node.kind == NodeKind::BatchNorm || node.kind == NodeKind::Softmax)
    {
        if (node.kind == NodeKind::BatchNorm && input.dimensions.size() < 2U) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "BatchNorm requires at least rank-two input"));
        if (node.kind == NodeKind::Softmax && !node.axes.empty())
        {
            const auto axis = NormalizeAxis(node.axes.front(), input.dimensions.size());
            if (!axis.IsSuccess()) return foundation::Result<TensorShape>::Failure(axis.Failure());
            if (node.axes.size() > 1U) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Softmax accepts a single axis"));
        }
        return foundation::Result<TensorShape>::Success(input);
    }
    if (node.kind == NodeKind::Conv2d)
    {
        if (input.dimensions.size() != 4U) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Conv2d requires NCHW input"));
        if (node.outputChannels <= 0 || node.kernelSize <= 0 || node.stride <= 0 || node.padding < 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Conv2d parameters are invalid"));
        const int64_t heightNumerator = input.dimensions[2] + 2 * node.padding - node.kernelSize;
        const int64_t widthNumerator = input.dimensions[3] + 2 * node.padding - node.kernelSize;
        if (heightNumerator < 0 || widthNumerator < 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Conv2d kernel exceeds padded input"));
        return foundation::Result<TensorShape>::Success({{input.dimensions[0], node.outputChannels, heightNumerator / node.stride + 1, widthNumerator / node.stride + 1}});
    }
    if (node.kind == NodeKind::Flatten)
    {
        if (node.flattenStartDimension < 0 || static_cast<size_t>(node.flattenStartDimension) >= input.dimensions.size()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Flatten start dimension is outside the input shape"));
        std::vector<int64_t> result(input.dimensions.begin(), input.dimensions.begin() + node.flattenStartDimension);
        int64_t flattened = 1;
        for (size_t index = static_cast<size_t>(node.flattenStartDimension); index < input.dimensions.size(); ++index)
        {
            const auto multiplied = CheckedMultiply(flattened, input.dimensions[index]);
            if (!multiplied.IsSuccess()) return foundation::Result<TensorShape>::Failure(multiplied.Failure());
            flattened = multiplied.Value();
        }
        result.push_back(flattened);
        return foundation::Result<TensorShape>::Success({std::move(result)});
    }
    if (node.kind == NodeKind::Linear)
    {
        if (input.dimensions.size() != 2U || node.outputFeatures <= 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear requires a rank-two input and positive output features"));
        return foundation::Result<TensorShape>::Success({{input.dimensions[0], node.outputFeatures}});
    }
    if (node.kind == NodeKind::MaxPool) return InferPool2dShape(input, node, "MaxPool");
    if (node.kind == NodeKind::AveragePool) return InferPool2dShape(input, node, "AveragePool");
    if (node.kind == NodeKind::AdaptiveAveragePool)
    {
        if (input.dimensions.size() != 4U) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AdaptiveAveragePool requires NCHW input"));
        if (node.outputShape.size() != 2U || node.outputShape[0] <= 0 || node.outputShape[1] <= 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AdaptiveAveragePool requires positive output height and width"));
        return foundation::Result<TensorShape>::Success({{input.dimensions[0], input.dimensions[1], node.outputShape[0], node.outputShape[1]}});
    }
    if (node.kind == NodeKind::Reshape) return InferReshapeShape(input, node);
    if (node.kind == NodeKind::Transpose)
    {
        std::vector<int64_t> permutation = node.permutation;
        if (permutation.empty())
        {
            permutation.reserve(input.dimensions.size());
            for (size_t index = input.dimensions.size(); index > 0U; --index) permutation.push_back(static_cast<int64_t>(index - 1U));
        }
        if (permutation.size() != input.dimensions.size()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Transpose permutation rank does not match input"));
        for (const int64_t axis : permutation)
        {
            if (axis < 0) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Transpose permutation axes must not be negative"));
        }
        const auto normalized = NormalizeAxes(permutation, input.dimensions.size());
        if (!normalized.IsSuccess()) return foundation::Result<TensorShape>::Failure(normalized.Failure());
        std::vector<int64_t> output;
        output.reserve(permutation.size());
        for (const int64_t axis : permutation) output.push_back(input.dimensions[static_cast<size_t>(axis)]);
        return foundation::Result<TensorShape>::Success({std::move(output)});
    }
    if (node.kind == NodeKind::Squeeze)
    {
        std::vector<int64_t> axes = node.axes;
        if (axes.empty())
        {
            for (size_t index = 0; index < input.dimensions.size(); ++index)
            {
                if (input.dimensions[index] == 1) axes.push_back(static_cast<int64_t>(index));
            }
        }
        const auto normalized = NormalizeAxes(axes, input.dimensions.size());
        if (!normalized.IsSuccess()) return foundation::Result<TensorShape>::Failure(normalized.Failure());
        std::vector<int64_t> output;
        for (size_t index = 0; index < input.dimensions.size(); ++index)
        {
            const bool removed = std::find(normalized.Value().begin(), normalized.Value().end(), static_cast<int64_t>(index)) != normalized.Value().end();
            if (removed && input.dimensions[index] != 1) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Squeeze axis must refer to a dimension of size one"));
            if (!removed) output.push_back(input.dimensions[index]);
        }
        if (output.empty()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Squeeze would produce a scalar tensor, which is not supported by this graph contract"));
        return foundation::Result<TensorShape>::Success({std::move(output)});
    }
    if (node.kind == NodeKind::Unsqueeze)
    {
        if (node.axes.empty()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Unsqueeze requires at least one axis"));
        const size_t outputRank = input.dimensions.size() + node.axes.size();
        const auto normalized = NormalizeAxes(node.axes, outputRank);
        if (!normalized.IsSuccess()) return foundation::Result<TensorShape>::Failure(normalized.Failure());
        std::vector<int64_t> output;
        output.reserve(outputRank);
        size_t inputIndex = 0;
        for (size_t outputIndex = 0; outputIndex < outputRank; ++outputIndex)
        {
            if (std::find(normalized.Value().begin(), normalized.Value().end(), static_cast<int64_t>(outputIndex)) != normalized.Value().end()) output.push_back(1);
            else
            {
                output.push_back(input.dimensions[inputIndex]);
                ++inputIndex;
            }
        }
        return foundation::Result<TensorShape>::Success({std::move(output)});
    }
    if (node.kind == NodeKind::ReduceMean)
    {
        std::vector<int64_t> axes = node.axes;
        if (axes.empty())
        {
            for (size_t index = 0; index < input.dimensions.size(); ++index) axes.push_back(static_cast<int64_t>(index));
        }
        const auto normalized = NormalizeAxes(axes, input.dimensions.size());
        if (!normalized.IsSuccess()) return foundation::Result<TensorShape>::Failure(normalized.Failure());
        std::vector<int64_t> output;
        for (size_t index = 0; index < input.dimensions.size(); ++index)
        {
            const bool reduced = std::find(normalized.Value().begin(), normalized.Value().end(), static_cast<int64_t>(index)) != normalized.Value().end();
            if (reduced)
            {
                if (node.keepDimensions) output.push_back(1);
            }
            else output.push_back(input.dimensions[index]);
        }
        if (output.empty()) return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "ReduceMean would produce a scalar tensor, which is not supported by this graph contract"));
        return foundation::Result<TensorShape>::Success({std::move(output)});
    }
    return foundation::Result<TensorShape>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Model graph node kind is unsupported"));
}

foundation::Result<void> ModelGraph::AddNode(const GraphNode &node)
{
    if (node.kind == NodeKind::Conv2d && (node.outputChannels <= 0 || node.kernelSize <= 0 || node.stride <= 0 || node.padding < 0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Conv2d parameters are invalid"));
    if ((node.kind == NodeKind::MaxPool || node.kind == NodeKind::AveragePool) && (node.kernelSize <= 0 || node.stride <= 0 || node.padding < 0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Pool2d parameters are invalid"));
    if (node.kind == NodeKind::AdaptiveAveragePool && (node.outputShape.size() != 2U || node.outputShape[0] <= 0 || node.outputShape[1] <= 0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AdaptiveAveragePool output shape is invalid"));
    if (node.kind == NodeKind::Reshape && node.outputShape.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Reshape output shape must not be empty"));
    if (node.kind == NodeKind::Unsqueeze && node.axes.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Unsqueeze axes must not be empty"));
    if (node.kind == NodeKind::Flatten && node.flattenStartDimension < 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Flatten start dimension must not be negative"));
    if (node.kind == NodeKind::Linear && node.outputFeatures <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear output features must be positive"));
    m_nodes.push_back(node);
    return foundation::Result<void>::Success();
}

foundation::Result<TensorShape> ModelGraph::InferOutputShape(const TensorShape &input) const
{
    TensorShape current = input;
    for (const GraphNode &node : m_nodes)
    {
        const auto inferred = InferNodeShape(current, node);
        if (!inferred.IsSuccess()) return inferred;
        current = inferred.Value();
    }
    return foundation::Result<TensorShape>::Success(std::move(current));
}

const std::vector<GraphNode> &ModelGraph::Nodes() const noexcept { return m_nodes; }
}
