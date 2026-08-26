#pragma once

#include "visionaiflow/foundation/Result.h"

#include <cstdint>
#include <vector>

#if defined(VISIONAIFLOW_MODEL_GRAPH_LIBRARY)
#define VISIONAIFLOW_MODEL_GRAPH_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_MODEL_GRAPH_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::model_graph
{
struct TensorShape final
{
    std::vector<int64_t> dimensions;
};

enum class NodeKind
{
    Conv2d,
    BatchNorm,
    Relu,
    Activation,
    MaxPool,
    AveragePool,
    AdaptiveAveragePool,
    Flatten,
    Linear,
    Reshape,
    Transpose,
    Squeeze,
    Unsqueeze,
    Softmax,
    ReduceMean
};

struct GraphNode final
{
    NodeKind kind{NodeKind::Relu};
    int64_t outputChannels{0};
    int64_t kernelSize{0};
    int64_t stride{1};
    int64_t padding{0};
    int64_t outputFeatures{0};
    int flattenStartDimension{1};
    std::vector<int64_t> outputShape;
    std::vector<int64_t> axes;
    std::vector<int64_t> permutation;
    bool keepDimensions{true};
};

VISIONAIFLOW_MODEL_GRAPH_EXPORT foundation::Result<void> ValidateShape(const TensorShape &shape);
VISIONAIFLOW_MODEL_GRAPH_EXPORT foundation::Result<TensorShape> InferNodeShape(const TensorShape &input, const GraphNode &node);

class VISIONAIFLOW_MODEL_GRAPH_EXPORT ModelGraph final
{
public:
    foundation::Result<void> AddNode(const GraphNode &node);
    foundation::Result<TensorShape> InferOutputShape(const TensorShape &input) const;
    const std::vector<GraphNode> &Nodes() const noexcept;

private:
    std::vector<GraphNode> m_nodes;
};
}
