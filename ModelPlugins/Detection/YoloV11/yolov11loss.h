#pragma once

#include <torch/torch.h>

#include <vector>

namespace visionaiflow::yolov11
{
struct Yolo11LossResult
{
    torch::Tensor total;
    torch::Tensor items;
    int64_t positiveAnchorCount{0};
};

class Yolo11DetectionLoss final
{
public:
    explicit Yolo11DetectionLoss(int64_t classCount);

    Yolo11LossResult operator()(const std::vector<torch::Tensor> &features, const torch::Tensor &targets,
                                const torch::Tensor &imageSize) const;

private:
    int64_t m_classCount{0};
    int64_t m_regMax{16};
    double m_boxGain{7.5};
    double m_classGain{0.5};
    double m_dflGain{1.5};
};
} // namespace visionaiflow::yolov11
