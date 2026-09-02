#include "yolov11loss.h"

#include <algorithm>

namespace visionaiflow::yolov11
{
namespace
{
std::pair<torch::Tensor, torch::Tensor> MakeAnchors(const std::vector<torch::Tensor> &features, const double offset)
{
    std::vector<torch::Tensor> anchorPoints;
    std::vector<torch::Tensor> strideTensor;
    const torch::Device device = features.front().device();
    const torch::TensorOptions options = torch::TensorOptions().device(device).dtype(torch::kFloat32);
    const std::vector<int64_t> strides{8, 16, 32};
    for (size_t index = 0; index < features.size(); ++index)
    {
        const int64_t height = features[index].size(2);
        const int64_t width = features[index].size(3);
        const torch::Tensor sx = torch::arange(width, options) + offset;
        const torch::Tensor sy = torch::arange(height, options) + offset;
        const std::vector<torch::Tensor> mesh = torch::meshgrid({sy, sx});
        anchorPoints.push_back(torch::stack({mesh[1], mesh[0]}, -1).view({-1, 2}));
        strideTensor.push_back(torch::full({height * width, 1}, static_cast<double>(strides[index]), options));
    }
    return {torch::cat(anchorPoints, 0), torch::cat(strideTensor, 0)};
}

torch::Tensor Dist2Bbox(const torch::Tensor &distance, const torch::Tensor &anchorPoints)
{
    const std::vector<torch::Tensor> split = distance.split(2, -1);
    const torch::Tensor topLeft = anchorPoints - split[0];
    const torch::Tensor bottomRight = anchorPoints + split[1];
    return torch::cat({topLeft, bottomRight}, -1);
}

torch::Tensor Bbox2Dist(const torch::Tensor &anchorPoints, const torch::Tensor &boxes, const int64_t regMax)
{
    return torch::cat({anchorPoints - boxes.slice(-1, 0, 2), boxes.slice(-1, 2, 4) - anchorPoints}, -1).clamp(0, regMax - 0.01);
}

torch::Tensor BboxIou(const torch::Tensor &box1, const torch::Tensor &box2)
{
    const torch::Tensor width1 = box1.select(-1, 2) - box1.select(-1, 0);
    const torch::Tensor height1 = box1.select(-1, 3) - box1.select(-1, 1);
    const torch::Tensor width2 = box2.select(-1, 2) - box2.select(-1, 0);
    const torch::Tensor height2 = box2.select(-1, 3) - box2.select(-1, 1);
    const torch::Tensor intersectionWidth = (torch::min(box1.select(-1, 2), box2.select(-1, 2)) -
                                             torch::max(box1.select(-1, 0), box2.select(-1, 0)))
                                                .clamp_min(0);
    const torch::Tensor intersectionHeight = (torch::min(box1.select(-1, 3), box2.select(-1, 3)) -
                                              torch::max(box1.select(-1, 1), box2.select(-1, 1)))
                                                 .clamp_min(0);
    const torch::Tensor intersection = intersectionWidth * intersectionHeight;
    const torch::Tensor unionArea = width1 * height1 + width2 * height2 - intersection + 1e-7;
    const torch::Tensor iou = intersection / unionArea;
    const torch::Tensor enclosingWidth = (torch::max(box1.select(-1, 2), box2.select(-1, 2)) -
                                           torch::min(box1.select(-1, 0), box2.select(-1, 0)))
                                              .clamp_min(0);
    const torch::Tensor enclosingHeight = (torch::max(box1.select(-1, 3), box2.select(-1, 3)) -
                                            torch::min(box1.select(-1, 1), box2.select(-1, 1)))
                                               .clamp_min(0);
    const torch::Tensor convexDiagonal = enclosingWidth.square() + enclosingHeight.square() + 1e-7;
    const torch::Tensor centerDistance = ((box2.select(-1, 0) + box2.select(-1, 2) - box1.select(-1, 0) - box1.select(-1, 2)).square() +
                                          (box2.select(-1, 1) + box2.select(-1, 3) - box1.select(-1, 1) - box1.select(-1, 3)).square()) /
                                         4.0;
    const torch::Tensor v = (4.0 / (M_PI * M_PI)) *
                            (torch::atan(width2 / (height2 + 1e-7)) - torch::atan(width1 / (height1 + 1e-7))).square();
    const torch::Tensor alpha = v / (1.0 - iou + v + 1e-7);
    return iou - centerDistance / convexDiagonal - alpha * v;
}

torch::Tensor DistributionFocalLoss(const torch::Tensor &prediction, const torch::Tensor &target)
{
    const torch::Tensor left = target.floor().to(torch::kLong);
    const torch::Tensor right = (left + 1).clamp_max(prediction.size(-1) - 1);
    const torch::Tensor weightLeft = right.to(target.dtype()) - target;
    const torch::Tensor weightRight = 1.0 - weightLeft;
    const torch::Tensor leftLoss = torch::nn::functional::cross_entropy(
        prediction.view({-1, prediction.size(-1)}), left.view({-1}), torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone));
    const torch::Tensor rightLoss = torch::nn::functional::cross_entropy(
        prediction.view({-1, prediction.size(-1)}), right.view({-1}), torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone));
    return (leftLoss.view_as(target) * weightLeft + rightLoss.view_as(target) * weightRight).mean(-1, true);
}
}

Yolo11DetectionLoss::Yolo11DetectionLoss(const int64_t classCount)
    : m_classCount(classCount)
{
}

Yolo11LossResult Yolo11DetectionLoss::operator()(const std::vector<torch::Tensor> &features, const torch::Tensor &targets,
                                                  const torch::Tensor &imageSize) const
{
    const int64_t batchSize = features.front().size(0);
    const torch::TensorOptions options = features.front().options();
    const torch::Tensor prediction = torch::cat({features[0].view({batchSize, 4 * m_regMax + m_classCount, -1}),
                                                 features[1].view({batchSize, 4 * m_regMax + m_classCount, -1}),
                                                 features[2].view({batchSize, 4 * m_regMax + m_classCount, -1})},
                                                2);
    const torch::Tensor predictionDistribution = prediction.slice(1, 0, 4 * m_regMax);
    const torch::Tensor predictionScores = prediction.slice(1, 4 * m_regMax).permute({0, 2, 1}).contiguous();
    const auto [anchorPoints, strideTensor] = MakeAnchors(features, 0.5);
    const torch::Tensor distribution = predictionDistribution.permute({0, 2, 1}).contiguous().view({batchSize, -1, 4, m_regMax});
    const torch::Tensor project = torch::arange(m_regMax, options.dtype(torch::kFloat));
    const torch::Tensor predictedBoxes = Dist2Bbox((distribution.softmax(-1) * project).sum(-1), anchorPoints).detach() * strideTensor;
    const torch::Tensor predictedBoxesForLoss = Dist2Bbox((distribution.softmax(-1) * project).sum(-1), anchorPoints) * strideTensor;

    const torch::Tensor labels = targets.select(1, 1).to(torch::kLong);
    const torch::Tensor boxesXywh = targets.slice(1, 2, 6) * imageSize.repeat({2});
    const torch::Tensor boxes = torch::stack({boxesXywh.select(1, 0) - boxesXywh.select(1, 2) / 2.0,
                                              boxesXywh.select(1, 1) - boxesXywh.select(1, 3) / 2.0,
                                              boxesXywh.select(1, 0) + boxesXywh.select(1, 2) / 2.0,
                                              boxesXywh.select(1, 1) + boxesXywh.select(1, 3) / 2.0},
                                             1);

    const int64_t maximumTargets = std::max<int64_t>(1, targets.numel() == 0 ? 0 :
                                                              targets.select(1, 0).to(torch::kLong).bincount({}, batchSize).max().item<int64_t>());
    torch::Tensor groundTruthLabels = torch::zeros({batchSize, maximumTargets, 1}, options.dtype(torch::kLong));
    torch::Tensor groundTruthBoxes = torch::zeros({batchSize, maximumTargets, 4}, options);
    torch::Tensor groundTruthMask = torch::zeros({batchSize, maximumTargets, 1}, options.dtype(torch::kBool));
    for (int64_t batchIndex = 0; batchIndex < batchSize; ++batchIndex)
    {
        const torch::Tensor selected = targets.select(1, 0).eq(batchIndex).nonzero().view({-1});
        const int64_t count = selected.numel();
        if (count > 0)
        {
            groundTruthLabels.index_put_({batchIndex, torch::indexing::Slice(0, count), 0}, labels.index_select(0, selected));
            groundTruthBoxes.index_put_({batchIndex, torch::indexing::Slice(0, count)}, boxes.index_select(0, selected));
            groundTruthMask.index_put_({batchIndex, torch::indexing::Slice(0, count), 0}, true);
        }
    }

    const int64_t anchors = predictionScores.size(1);
    const torch::Tensor anchorX = anchorPoints.select(1, 0).view({1, 1, anchors});
    const torch::Tensor anchorY = anchorPoints.select(1, 1).view({1, 1, anchors});
    const torch::Tensor boxLeft = groundTruthBoxes.select(-1, 0).unsqueeze(-1) / strideTensor.view({1, 1, anchors});
    const torch::Tensor boxTop = groundTruthBoxes.select(-1, 1).unsqueeze(-1) / strideTensor.view({1, 1, anchors});
    const torch::Tensor boxRight = groundTruthBoxes.select(-1, 2).unsqueeze(-1) / strideTensor.view({1, 1, anchors});
    const torch::Tensor boxBottom = groundTruthBoxes.select(-1, 3).unsqueeze(-1) / strideTensor.view({1, 1, anchors});
    const torch::Tensor insideGroundTruth = (anchorX - boxLeft).minimum(boxRight - anchorX).minimum(anchorY - boxTop).minimum(boxBottom - anchorY).gt(1e-9);

    const torch::Tensor gtLabelsForScore = groundTruthLabels.squeeze(-1).unsqueeze(1).expand({batchSize, anchors, maximumTargets});
    const torch::Tensor classScores = predictionScores.gather(2, gtLabelsForScore).permute({0, 2, 1});
    const torch::Tensor overlaps = BboxIou(predictedBoxes.unsqueeze(1).expand({batchSize, maximumTargets, anchors, 4}),
                                           groundTruthBoxes.unsqueeze(2).expand({batchSize, maximumTargets, anchors, 4}))
                                       .clamp_min(0);
    torch::Tensor alignMetric = classScores.pow(0.5) * overlaps.pow(6.0) * insideGroundTruth.to(options.dtype()) *
                                groundTruthMask.to(options.dtype());
    const int64_t topk = std::min<int64_t>(10, anchors);
    const torch::Tensor topkIndexes = std::get<1>(alignMetric.topk(topk, -1, true, true));
    torch::Tensor topkMask = torch::zeros_like(alignMetric, options.dtype(torch::kBool));
    topkMask.scatter_(-1, topkIndexes, true);
    const torch::Tensor positiveMask = topkMask & insideGroundTruth & groundTruthMask;
    const torch::Tensor overlapsMasked = overlaps * positiveMask.to(options.dtype());
    const torch::Tensor targetGroundTruthIndex = std::get<1>(overlapsMasked.max(1));
    const torch::Tensor foregroundMask = std::get<0>(overlapsMasked.max(1)).gt(0);

    const torch::Tensor flattenedOffset = torch::arange(batchSize, options.dtype(torch::kLong)).view({batchSize, 1}) * maximumTargets;
    const torch::Tensor flattenedIndex = (targetGroundTruthIndex + flattenedOffset).view({-1});
    const torch::Tensor targetLabels = groundTruthLabels.view({-1, 1}).index_select(0, flattenedIndex).view({batchSize, anchors});
    const torch::Tensor targetBoxes = groundTruthBoxes.view({-1, 4}).index_select(0, flattenedIndex).view({batchSize, anchors, 4});
    torch::Tensor targetScores = torch::zeros({batchSize, anchors, m_classCount}, options);
    targetScores.scatter_(2, targetLabels.unsqueeze(-1), 1.0);
    targetScores = targetScores * foregroundMask.unsqueeze(-1).to(options.dtype());

    alignMetric = alignMetric * positiveMask.to(options.dtype());
    const torch::Tensor normalizedMetric = (alignMetric.amax(-1, true) * overlapsMasked.amax(-1, true) /
                                            (alignMetric.amax(-1, true) + 1e-9));
    const torch::Tensor matchedMetric = normalizedMetric.gather(1, targetGroundTruthIndex);
    targetScores = targetScores * matchedMetric.unsqueeze(-1);
    const torch::Tensor targetScoresSum = targetScores.sum().clamp_min(1.0);
    torch::Tensor classificationLoss = torch::nn::functional::binary_cross_entropy_with_logits(
        predictionScores, targetScores, torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kSum));
    classificationLoss = classificationLoss / targetScoresSum;

    torch::Tensor boxLoss = torch::zeros({}, options);
    torch::Tensor dflLoss = torch::zeros({}, options);
    const torch::Tensor foregroundIndexes = foregroundMask.nonzero();
    if (foregroundIndexes.numel() > 0)
    {
        const torch::Tensor batchIndexes = foregroundIndexes.select(1, 0);
        const torch::Tensor anchorIndexes = foregroundIndexes.select(1, 1);
        const torch::Tensor weight = targetScores.sum(-1).index({batchIndexes, anchorIndexes}).unsqueeze(-1);
        const torch::Tensor ciou = BboxIou(predictedBoxesForLoss.index({batchIndexes, anchorIndexes}),
                                           targetBoxes.index({batchIndexes, anchorIndexes}));
        boxLoss = ((1.0 - ciou).unsqueeze(-1) * weight).sum() / targetScoresSum;
        const torch::Tensor targetDistance = Bbox2Dist(anchorPoints.index_select(0, anchorIndexes),
                                                        targetBoxes.index({batchIndexes, anchorIndexes}) / strideTensor.index_select(0, anchorIndexes),
                                                        m_regMax - 1);
        const torch::Tensor distributionForForeground = distribution.index({batchIndexes, anchorIndexes});
        dflLoss = (DistributionFocalLoss(distributionForForeground.view({-1, m_regMax}), targetDistance.view({-1, 4})) * weight).sum() /
                  targetScoresSum;
    }

    const torch::Tensor items = torch::stack({boxLoss.detach(), classificationLoss.detach(), dflLoss.detach()});
    return {(boxLoss * m_boxGain + classificationLoss * m_classGain + dflLoss * m_dflGain) * batchSize,
            items,
            foregroundIndexes.size(0)};
}
} // namespace visionaiflow::yolov11
