#include "yolov11validator.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace visionaiflow::yolov11
{
namespace
{
torch::Tensor dist2Bbox(const torch::Tensor &distance,
                        const torch::Tensor &anchorPoints)
{
    const std::vector<torch::Tensor> split = distance.split(2, -1);
    const torch::Tensor topLeft = anchorPoints - split[0];
    const torch::Tensor bottomRight = anchorPoints + split[1];
    return torch::cat({topLeft, bottomRight}, -1);
}
}

Yolo11Validator::Yolo11Validator(const Yolo11ValidatorOptions &options)
    : m_options(options)
{
    if (m_options.classCount <= 0)
    {
        throw std::invalid_argument("Yolo11Validator: classCount must be > 0.");
    }
    if (m_options.regMax <= 1)
    {
        throw std::invalid_argument("Yolo11Validator: regMax must be > 1.");
    }
}

Yolo11Validator::AnchorData
Yolo11Validator::makeAnchors(const std::vector<torch::Tensor> &features) const
{
    if (features.size() != 3)
    {
        throw std::runtime_error("Yolo11Validator: YOLO11 Detect expects exactly 3 feature maps.");
    }

    static constexpr int64_t strides[3] = {8, 16, 32};

    std::vector<torch::Tensor> anchorPoints;
    std::vector<torch::Tensor> strideTensor;

    const torch::TensorOptions options = features.front().options();

    for (size_t featureIndex = 0; featureIndex < features.size(); ++featureIndex)
    {
        const int64_t height = features[featureIndex].size(2);
        const int64_t width = features[featureIndex].size(3);

        const torch::Tensor sx = torch::arange(width, options) + 0.5;
        const torch::Tensor sy = torch::arange(height, options) + 0.5;
        const std::vector<torch::Tensor> mesh = torch::meshgrid({sy, sx});

        anchorPoints.push_back(
            torch::stack({mesh[1], mesh[0]}, -1).view({-1, 2}));

        strideTensor.push_back(
            torch::full({height * width, 1},
                        static_cast<double>(strides[featureIndex]),
                        options));
    }

    return {torch::cat(anchorPoints, 0), torch::cat(strideTensor, 0)};
}

float Yolo11Validator::boxIou(const Yolo11Box &a, const Yolo11Box &b)
{
    const float interLeft = std::max(a.x1, b.x1);
    const float interTop = std::max(a.y1, b.y1);
    const float interRight = std::min(a.x2, b.x2);
    const float interBottom = std::min(a.y2, b.y2);

    const float interWidth = std::max(0.0f, interRight - interLeft);
    const float interHeight = std::max(0.0f, interBottom - interTop);
    const float intersection = interWidth * interHeight;

    const float areaA = std::max(0.0f, a.x2 - a.x1) *
                        std::max(0.0f, a.y2 - a.y1);
    const float areaB = std::max(0.0f, b.x2 - b.x1) *
                        std::max(0.0f, b.y2 - b.y1);
    const float unionArea = areaA + areaB - intersection;

    return unionArea > 0.0f ? intersection / unionArea : 0.0f;
}

std::vector<std::vector<Yolo11Detection>>
Yolo11Validator::decodeAndNms(const std::vector<torch::Tensor> &features,
                              const int imageHeight,
                              const int imageWidth) const
{
    if (features.size() != 3)
    {
        throw std::runtime_error("Yolo11Validator: forward() must return 3 raw feature maps.");
    }

    const int64_t batchSize = features.front().size(0);
    const int64_t channels = 4LL * m_options.regMax + m_options.classCount;

    for (const torch::Tensor &feature : features)
    {
        if (feature.dim() != 4 || feature.size(0) != batchSize || feature.size(1) != channels)
        {
            throw std::runtime_error(
                "Yolo11Validator: invalid raw Detect feature shape; expected [B, 4*regMax+nc, H, W].");
        }
    }

    const AnchorData anchorData = makeAnchors(features);

    const torch::Tensor prediction = torch::cat(
        {features[0].view({batchSize, channels, -1}),
         features[1].view({batchSize, channels, -1}),
         features[2].view({batchSize, channels, -1})},
        2);

    const torch::Tensor distribution =
        prediction.slice(1, 0, 4LL * m_options.regMax)
            .permute({0, 2, 1})
            .contiguous()
            .view({batchSize, -1, 4, m_options.regMax});

    const torch::Tensor classProbabilities =
        prediction.slice(1, 4LL * m_options.regMax)
            .permute({0, 2, 1})
            .contiguous()
            .sigmoid();

    const torch::Tensor project =
        torch::arange(m_options.regMax, distribution.options());

    const torch::Tensor distances =
        (distribution.softmax(-1) * project).sum(-1);

    const torch::Tensor boxes =
        dist2Bbox(distances, anchorData.points) * anchorData.strides;

    std::vector<std::vector<Yolo11Detection>> batchDetections(
        static_cast<size_t>(batchSize));

    for (int64_t batchIndex = 0; batchIndex < batchSize; ++batchIndex)
    {
        const torch::Tensor imageBoxes = boxes[batchIndex];
        const torch::Tensor imageScores = classProbabilities[batchIndex];

        torch::Tensor anchorIndices;
        torch::Tensor classIndices;
        torch::Tensor confidences;

        if (m_options.multiLabel)
        {
            const torch::Tensor candidates =
                imageScores.gt(m_options.confidenceThreshold).nonzero();

            if (candidates.numel() == 0)
            {
                continue;
            }

            anchorIndices = candidates.select(1, 0).to(torch::kLong);
            classIndices = candidates.select(1, 1).to(torch::kLong);
            confidences = imageScores.index({anchorIndices, classIndices});
        }
        else
        {
            const auto maxResult = imageScores.max(1);
            const torch::Tensor maxScores = std::get<0>(maxResult);
            const torch::Tensor maxClasses = std::get<1>(maxResult);
            const torch::Tensor keep =
                maxScores.gt(m_options.confidenceThreshold).nonzero().view({-1});

            if (keep.numel() == 0)
            {
                continue;
            }

            anchorIndices = keep.to(torch::kLong);
            classIndices = maxClasses.index_select(0, anchorIndices).to(torch::kLong);
            confidences = maxScores.index_select(0, anchorIndices);
        }

        torch::Tensor candidateBoxes = imageBoxes.index_select(0, anchorIndices);

        candidateBoxes = candidateBoxes.detach().to(torch::kCPU).to(torch::kFloat32).contiguous();
        classIndices = classIndices.detach().to(torch::kCPU).to(torch::kLong).contiguous();
        confidences = confidences.detach().to(torch::kCPU).to(torch::kFloat32).contiguous();

        const auto boxAccessor = candidateBoxes.accessor<float, 2>();
        const auto classAccessor = classIndices.accessor<int64_t, 1>();
        const auto confidenceAccessor = confidences.accessor<float, 1>();

        std::vector<Yolo11Detection> candidates;
        candidates.reserve(static_cast<size_t>(candidateBoxes.size(0)));

        for (int64_t index = 0; index < candidateBoxes.size(0); ++index)
        {
            Yolo11Detection detection;
            detection.box.x1 = std::clamp(boxAccessor[index][0], 0.0f, static_cast<float>(imageWidth));
            detection.box.y1 = std::clamp(boxAccessor[index][1], 0.0f, static_cast<float>(imageHeight));
            detection.box.x2 = std::clamp(boxAccessor[index][2], 0.0f, static_cast<float>(imageWidth));
            detection.box.y2 = std::clamp(boxAccessor[index][3], 0.0f, static_cast<float>(imageHeight));
            detection.confidence = confidenceAccessor[index];
            detection.classId = static_cast<int>(classAccessor[index]);

            if (detection.classId < 0 || detection.classId >= m_options.classCount)
            {
                continue;
            }
            if (detection.box.x2 <= detection.box.x1 || detection.box.y2 <= detection.box.y1)
            {
                continue;
            }

            candidates.push_back(detection);
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const Yolo11Detection &a, const Yolo11Detection &b)
                  {
                      return a.confidence > b.confidence;
                  });

        if (static_cast<int>(candidates.size()) > m_options.maxNmsCandidates)
        {
            candidates.resize(static_cast<size_t>(m_options.maxNmsCandidates));
        }

        // Class-aware NMS. This is equivalent to class offsets in Ultralytics,
        // but is clearer and avoids dependence on torchvision::nms in C++.
        std::vector<Yolo11Detection> selected;
        selected.reserve(static_cast<size_t>(m_options.maxDetections));

        for (const Yolo11Detection &candidate : candidates)
        {
            bool suppressed = false;
            for (const Yolo11Detection &kept : selected)
            {
                if (candidate.classId == kept.classId &&
                    boxIou(candidate.box, kept.box) > m_options.nmsIouThreshold)
                {
                    suppressed = true;
                    break;
                }
            }

            if (!suppressed)
            {
                selected.push_back(candidate);
                if (static_cast<int>(selected.size()) >= m_options.maxDetections)
                {
                    break;
                }
            }
        }

        batchDetections[static_cast<size_t>(batchIndex)] = std::move(selected);
    }

    return batchDetections;
}

std::vector<Yolo11GroundTruth>
Yolo11Validator::decodeGroundTruth(const torch::Tensor &targets,
                                   const int batchIndex,
                                   const int imageHeight,
                                   const int imageWidth) const
{
    std::vector<Yolo11GroundTruth> result;

    if (!targets.defined() || targets.numel() == 0)
    {
        return result;
    }
    if (targets.dim() != 2 || targets.size(1) != 6)
    {
        throw std::runtime_error(
            "Yolo11Validator: targets must have shape [N, 6] = [batch, class, cx, cy, w, h].");
    }

    const torch::Tensor cpuTargets =
        targets.detach().to(torch::kCPU).to(torch::kFloat32).contiguous();
    const auto accessor = cpuTargets.accessor<float, 2>();

    for (int64_t row = 0; row < cpuTargets.size(0); ++row)
    {
        const int targetBatch = static_cast<int>(accessor[row][0]);
        if (targetBatch != batchIndex)
        {
            continue;
        }

        const int classId = static_cast<int>(accessor[row][1]);
        if (classId < 0 || classId >= m_options.classCount)
        {
            continue;
        }

        const float centerX = accessor[row][2] * static_cast<float>(imageWidth);
        const float centerY = accessor[row][3] * static_cast<float>(imageHeight);
        const float width = accessor[row][4] * static_cast<float>(imageWidth);
        const float height = accessor[row][5] * static_cast<float>(imageHeight);

        Yolo11GroundTruth target;
        target.classId = classId;
        target.box.x1 = std::clamp(centerX - width * 0.5f, 0.0f, static_cast<float>(imageWidth));
        target.box.y1 = std::clamp(centerY - height * 0.5f, 0.0f, static_cast<float>(imageHeight));
        target.box.x2 = std::clamp(centerX + width * 0.5f, 0.0f, static_cast<float>(imageWidth));
        target.box.y2 = std::clamp(centerY + height * 0.5f, 0.0f, static_cast<float>(imageHeight));

        if (target.box.x2 > target.box.x1 && target.box.y2 > target.box.y1)
        {
            result.push_back(target);
        }
    }

    return result;
}

Yolo11ValidationMetrics
Yolo11Validator::validate(const int64_t batchCount,
                          const BatchProvider &batchProvider,
                          const ForwardFunction &forward) const
{
    if (batchCount < 0)
    {
        throw std::invalid_argument("Yolo11Validator: batchCount must be >= 0.");
    }
    if (!batchProvider)
    {
        throw std::invalid_argument("Yolo11Validator: batchProvider is empty.");
    }
    if (!forward)
    {
        throw std::invalid_argument("Yolo11Validator: forward callback is empty.");
    }

    Yolo11Metrics metrics;
    torch::NoGradGuard noGrad;

    for (int64_t batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        Yolo11ValidationBatch batch;
        if (!batchProvider(batchIndex, batch))
        {
            throw std::runtime_error("Yolo11Validator: batchProvider failed to provide a validation batch.");
        }

        if (!batch.images.defined() || batch.images.dim() != 4 || batch.images.size(1) != 3)
        {
            throw std::runtime_error("Yolo11Validator: images must have shape [B, 3, H, W].");
        }

        const int imageHeight = static_cast<int>(batch.images.size(2));
        const int imageWidth = static_cast<int>(batch.images.size(3));
        const int64_t imageCount = batch.images.size(0);

        const std::vector<torch::Tensor> features = forward(batch.images);
        const std::vector<std::vector<Yolo11Detection>> predictions =
            decodeAndNms(features, imageHeight, imageWidth);

        if (static_cast<int64_t>(predictions.size()) != imageCount)
        {
            throw std::runtime_error("Yolo11Validator: decoded prediction batch size mismatch.");
        }

        for (int64_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
        {
            const std::vector<Yolo11GroundTruth> groundTruth =
                decodeGroundTruth(batch.targets,
                                  static_cast<int>(imageIndex),
                                  imageHeight,
                                  imageWidth);

            metrics.addImage(predictions[static_cast<size_t>(imageIndex)], groundTruth);
        }
    }

    return metrics.compute(m_options.classCount);
}

} // namespace visionaiflow::yolov11
