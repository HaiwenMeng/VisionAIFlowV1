#pragma once

#include "yolov11metrics.h"

#include <torch/torch.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace visionaiflow::yolov11
{

// Validation input intentionally uses the same target convention as the
// current YOLO11 loss:
// targets: [N, 6] = [batch_index, class_id, cx, cy, w, h]
// cx/cy/w/h are normalized to the network input image.
struct Yolo11ValidationBatch
{
    torch::Tensor images;  // [B, 3, H, W], already preprocessed, on model device
    torch::Tensor targets; // [N, 6], normalized YOLO labels
};

struct Yolo11ValidatorOptions
{
    int classCount{0};
    int regMax{16};

    // Ultralytics validation defaults. Keep conf very low so the complete
    // precision-recall curve is available for AP calculation.
    float confidenceThreshold{0.001f};
    float nmsIouThreshold{0.70f};
    int maxDetections{300};
    int maxNmsCandidates{30000};

    // Ultralytics detection validation uses multi-label NMS candidates.
    bool multiLabel{true};
};

class Yolo11Validator
{
public:
    using ForwardFunction =
        std::function<std::vector<torch::Tensor>(const torch::Tensor &images)>;

    using BatchProvider =
        std::function<bool(int64_t batchIndex, Yolo11ValidationBatch &batch)>;

    explicit Yolo11Validator(const Yolo11ValidatorOptions &options);

    // The caller owns model state:
    //   model->eval();
    //   auto metrics = validator.validate(...);
    //   model->train();
    //
    // batchProvider must provide exactly batchCount validation batches.
    // forward must return the three raw YOLO11 Detect feature tensors:
    // [B, 4 * regMax + classCount, H_i, W_i].
    Yolo11ValidationMetrics validate(int64_t batchCount,
                                     const BatchProvider &batchProvider,
                                     const ForwardFunction &forward) const;

private:
    struct AnchorData
    {
        torch::Tensor points;  // [A, 2], grid coordinates
        torch::Tensor strides; // [A, 1], pixels
    };

    AnchorData makeAnchors(const std::vector<torch::Tensor> &features) const;

    std::vector<std::vector<Yolo11Detection>>
    decodeAndNms(const std::vector<torch::Tensor> &features,
                 int imageHeight,
                 int imageWidth) const;

    std::vector<Yolo11GroundTruth>
    decodeGroundTruth(const torch::Tensor &targets,
                      int batchIndex,
                      int imageHeight,
                      int imageWidth) const;

    static float boxIou(const Yolo11Box &a, const Yolo11Box &b);

private:
    Yolo11ValidatorOptions m_options;
};

} // namespace visionaiflow::yolov11
