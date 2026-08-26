#include "visionaiflow/models/common/DetectionPostProcessor.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace visionaiflow::models::common
{
namespace
{
foundation::Result<void> ValidateDecodeArguments(const std::vector<float> &rawOutput, const int rowCount, const int classCount, const float imageWidth, const float imageHeight, const YoloDetectionDecodeConfig &config)
{
    if (rowCount < 0 || classCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO detection row and class counts must be valid"));
    if (!std::isfinite(imageWidth) || !std::isfinite(imageHeight) || imageWidth <= 0.0F || imageHeight <= 0.0F) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO detection image dimensions must be positive finite values"));
    if (!std::isfinite(config.scoreThreshold) || !std::isfinite(config.nmsIouThreshold) || config.scoreThreshold < 0.0F || config.scoreThreshold > 1.0F || config.nmsIouThreshold < 0.0F || config.nmsIouThreshold > 1.0F) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO detection thresholds must be finite values in the closed interval from zero to one"));
    if (config.maxDetections <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO detection maxDetections must be positive"));
    const int stride = 4 + classCount;
    const auto expected = static_cast<size_t>(rowCount) * static_cast<size_t>(stride);
    if (rawOutput.size() != expected) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO detection raw output size does not match row and class counts"));
    for (const float value : rawOutput)
    {
        if (!std::isfinite(value)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO detection raw output contains a non-finite value"));
    }
    return foundation::Result<void>::Success();
}

float Clamp(const float value, const float low, const float high)
{
    return std::max(low, std::min(value, high));
}

DetectionBox CenterToBox(const float centerX, const float centerY, const float width, const float height, const float imageWidth, const float imageHeight, const bool clip)
{
    DetectionBox box{centerX - width * 0.5F, centerY - height * 0.5F, centerX + width * 0.5F, centerY + height * 0.5F};
    if (clip)
    {
        box.x1 = Clamp(box.x1, 0.0F, imageWidth);
        box.y1 = Clamp(box.y1, 0.0F, imageHeight);
        box.x2 = Clamp(box.x2, 0.0F, imageWidth);
        box.y2 = Clamp(box.y2, 0.0F, imageHeight);
    }
    return box;
}

bool HasPositiveArea(const DetectionBox &box)
{
    return box.x2 > box.x1 && box.y2 > box.y1;
}

bool IsFinitePositive(const float value)
{
    return std::isfinite(value) && value > 0.0F;
}

foundation::Result<void> ValidateLetterboxGeometry(const LetterboxGeometry &geometry)
{
    if (!IsFinitePositive(geometry.originalWidth) || !IsFinitePositive(geometry.originalHeight) || !IsFinitePositive(geometry.networkWidth) || !IsFinitePositive(geometry.networkHeight) || !IsFinitePositive(geometry.scale) || !std::isfinite(geometry.padX) || !std::isfinite(geometry.padY)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Letterbox geometry contains invalid dimensions, scale or padding"));
    const float scaledWidth = geometry.originalWidth * geometry.scale;
    const float scaledHeight = geometry.originalHeight * geometry.scale;
    if (scaledWidth - geometry.networkWidth > 1.0e-3F || scaledHeight - geometry.networkHeight > 1.0e-3F || geometry.padX < -1.0e-3F || geometry.padY < -1.0e-3F) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Letterbox geometry is inconsistent with original and network dimensions"));
    return foundation::Result<void>::Success();
}
}

foundation::Result<LetterboxGeometry> CreateLetterboxGeometry(const float originalWidth, const float originalHeight, const float networkWidth, const float networkHeight, const bool allowScaleUp)
{
    if (!IsFinitePositive(originalWidth) || !IsFinitePositive(originalHeight) || !IsFinitePositive(networkWidth) || !IsFinitePositive(networkHeight)) return foundation::Result<LetterboxGeometry>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Letterbox geometry dimensions must be positive finite values"));
    const float rawScale = std::min(networkWidth / originalWidth, networkHeight / originalHeight);
    const float scale = allowScaleUp ? rawScale : std::min(rawScale, 1.0F);
    if (!IsFinitePositive(scale)) return foundation::Result<LetterboxGeometry>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Letterbox geometry scale is invalid"));
    const float scaledWidth = originalWidth * scale;
    const float scaledHeight = originalHeight * scale;
    const float padX = (networkWidth - scaledWidth) * 0.5F;
    const float padY = (networkHeight - scaledHeight) * 0.5F;
    LetterboxGeometry geometry{originalWidth, originalHeight, networkWidth, networkHeight, scale, padX, padY};
    const auto validation = ValidateLetterboxGeometry(geometry);
    if (!validation.IsSuccess()) return foundation::Result<LetterboxGeometry>::Failure(validation.Failure());
    return foundation::Result<LetterboxGeometry>::Success(geometry);
}

foundation::Result<DetectionBox> RestoreLetterboxedBoxToOriginal(const DetectionBox &networkBox, const LetterboxGeometry &geometry, const bool clip)
{
    const auto validation = ValidateLetterboxGeometry(geometry);
    if (!validation.IsSuccess()) return foundation::Result<DetectionBox>::Failure(validation.Failure());
    if (!std::isfinite(networkBox.x1) || !std::isfinite(networkBox.y1) || !std::isfinite(networkBox.x2) || !std::isfinite(networkBox.y2) || !HasPositiveArea(networkBox)) return foundation::Result<DetectionBox>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Letterbox restore input box must have positive finite area"));
    DetectionBox restored{(networkBox.x1 - geometry.padX) / geometry.scale, (networkBox.y1 - geometry.padY) / geometry.scale, (networkBox.x2 - geometry.padX) / geometry.scale, (networkBox.y2 - geometry.padY) / geometry.scale};
    if (clip)
    {
        restored.x1 = Clamp(restored.x1, 0.0F, geometry.originalWidth);
        restored.y1 = Clamp(restored.y1, 0.0F, geometry.originalHeight);
        restored.x2 = Clamp(restored.x2, 0.0F, geometry.originalWidth);
        restored.y2 = Clamp(restored.y2, 0.0F, geometry.originalHeight);
    }
    if (!HasPositiveArea(restored)) return foundation::Result<DetectionBox>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Letterbox restore produced an empty box after clipping"));
    return foundation::Result<DetectionBox>::Success(restored);
}

foundation::Result<std::vector<Detection>> RestoreLetterboxedDetectionsToOriginal(const std::vector<Detection> &networkDetections, const LetterboxGeometry &geometry, const bool clip)
{
    const auto validation = ValidateLetterboxGeometry(geometry);
    if (!validation.IsSuccess()) return foundation::Result<std::vector<Detection>>::Failure(validation.Failure());
    std::vector<Detection> restored;
    restored.reserve(networkDetections.size());
    for (const Detection &detection : networkDetections)
    {
        const auto box = RestoreLetterboxedBoxToOriginal(detection.box, geometry, clip);
        if (!box.IsSuccess()) return foundation::Result<std::vector<Detection>>::Failure(box.Failure());
        restored.push_back({box.Value(), detection.classIndex, detection.score});
    }
    return foundation::Result<std::vector<Detection>>::Success(std::move(restored));
}

foundation::Result<std::vector<DetectionOverlayItem>> CreateDetectionOverlayItems(const std::vector<Detection> &detections, const std::vector<std::string> &classNames, const float imageWidth, const float imageHeight)
{
    if (!IsFinitePositive(imageWidth) || !IsFinitePositive(imageHeight)) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay image dimensions must be positive finite values"));
    if (classNames.empty()) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay requires at least one class name"));
    std::vector<DetectionOverlayItem> items;
    items.reserve(detections.size());
    for (const Detection &detection : detections)
    {
        if (!HasPositiveArea(detection.box) || !std::isfinite(detection.box.x1) || !std::isfinite(detection.box.y1) || !std::isfinite(detection.box.x2) || !std::isfinite(detection.box.y2)) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay boxes must have positive finite area"));
        if (detection.box.x1 < 0.0F || detection.box.y1 < 0.0F || detection.box.x2 > imageWidth || detection.box.y2 > imageHeight) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay boxes must be inside the image bounds"));
        if (detection.classIndex < 0 || detection.classIndex >= static_cast<int>(classNames.size())) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay class index is outside classNames"));
        if (!std::isfinite(detection.score) || detection.score < 0.0F || detection.score > 1.0F) return foundation::Result<std::vector<DetectionOverlayItem>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection overlay score must be finite and in the closed interval from zero to one"));
        std::ostringstream caption;
        caption << classNames[static_cast<size_t>(detection.classIndex)] << ' ' << std::fixed << std::setprecision(3) << detection.score;
        items.push_back({detection.box, detection.classIndex, detection.score, caption.str()});
    }
    return foundation::Result<std::vector<DetectionOverlayItem>>::Success(std::move(items));
}

float IntersectionOverUnion(const DetectionBox &first, const DetectionBox &second)
{
    if (!HasPositiveArea(first) || !HasPositiveArea(second)) return 0.0F;
    const float x1 = std::max(first.x1, second.x1);
    const float y1 = std::max(first.y1, second.y1);
    const float x2 = std::min(first.x2, second.x2);
    const float y2 = std::min(first.y2, second.y2);
    const float intersectionWidth = std::max(0.0F, x2 - x1);
    const float intersectionHeight = std::max(0.0F, y2 - y1);
    const float intersection = intersectionWidth * intersectionHeight;
    const float firstArea = (first.x2 - first.x1) * (first.y2 - first.y1);
    const float secondArea = (second.x2 - second.x1) * (second.y2 - second.y1);
    const float denominator = firstArea + secondArea - intersection;
    if (denominator <= std::numeric_limits<float>::epsilon()) return 0.0F;
    return intersection / denominator;
}

foundation::Result<std::vector<Detection>> DecodeYoloCenterDetections(const std::vector<float> &rawOutput, const int rowCount, const int classCount, const float imageWidth, const float imageHeight, const YoloDetectionDecodeConfig &config)
{
    const auto validation = ValidateDecodeArguments(rawOutput, rowCount, classCount, imageWidth, imageHeight, config);
    if (!validation.IsSuccess()) return foundation::Result<std::vector<Detection>>::Failure(validation.Failure());
    const int stride = 4 + classCount;
    std::vector<Detection> candidates;
    candidates.reserve(static_cast<size_t>(rowCount));
    for (int row = 0; row < rowCount; ++row)
    {
        const size_t offset = static_cast<size_t>(row) * static_cast<size_t>(stride);
        int bestClass = -1;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (int classIndex = 0; classIndex < classCount; ++classIndex)
        {
            const float score = rawOutput[offset + 4U + static_cast<size_t>(classIndex)];
            if (score > bestScore)
            {
                bestScore = score;
                bestClass = classIndex;
            }
        }
        if (bestClass < 0 || bestScore < config.scoreThreshold) continue;
        const DetectionBox box = CenterToBox(rawOutput[offset], rawOutput[offset + 1U], rawOutput[offset + 2U], rawOutput[offset + 3U], imageWidth, imageHeight, config.clipBoxes);
        if (!HasPositiveArea(box)) continue;
        candidates.push_back({box, bestClass, bestScore});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Detection &left, const Detection &right) { return left.score > right.score; });
    std::vector<Detection> kept;
    kept.reserve(std::min(static_cast<size_t>(config.maxDetections), candidates.size()));
    for (const Detection &candidate : candidates)
    {
        bool suppressed = false;
        for (const Detection &accepted : kept)
        {
            if ((config.classAgnosticNms || accepted.classIndex == candidate.classIndex) && IntersectionOverUnion(accepted.box, candidate.box) > config.nmsIouThreshold)
            {
                suppressed = true;
                break;
            }
        }
        if (suppressed) continue;
        kept.push_back(candidate);
        if (static_cast<int>(kept.size()) >= config.maxDetections) break;
    }
    return foundation::Result<std::vector<Detection>>::Success(std::move(kept));
}

foundation::Result<std::vector<Detection>> DecodeYoloCenterDetectionsFromLetterbox(const std::vector<float> &rawOutput, const int rowCount, const int classCount, const LetterboxGeometry &geometry, const YoloDetectionDecodeConfig &config)
{
    const auto validation = ValidateLetterboxGeometry(geometry);
    if (!validation.IsSuccess()) return foundation::Result<std::vector<Detection>>::Failure(validation.Failure());
    const auto decoded = DecodeYoloCenterDetections(rawOutput, rowCount, classCount, geometry.networkWidth, geometry.networkHeight, config);
    if (!decoded.IsSuccess()) return decoded;
    return RestoreLetterboxedDetectionsToOriginal(decoded.Value(), geometry, config.clipBoxes);
}
}
