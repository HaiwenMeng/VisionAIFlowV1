#pragma once

#include "visionaiflow/foundation/Result.h"

#include <string>
#include <vector>

namespace visionaiflow::models::common
{
struct DetectionBox final
{
    float x1{0.0F};
    float y1{0.0F};
    float x2{0.0F};
    float y2{0.0F};
};

struct Detection final
{
    DetectionBox box;
    int classIndex{-1};
    float score{0.0F};
};

struct DetectionOverlayItem final
{
    DetectionBox box;
    int classIndex{-1};
    float score{0.0F};
    std::string caption;
};

struct LetterboxGeometry final
{
    float originalWidth{0.0F};
    float originalHeight{0.0F};
    float networkWidth{0.0F};
    float networkHeight{0.0F};
    float scale{0.0F};
    float padX{0.0F};
    float padY{0.0F};
};

struct YoloDetectionDecodeConfig final
{
    float scoreThreshold{0.25F};
    float nmsIouThreshold{0.45F};
    int maxDetections{300};
    bool classAgnosticNms{false};
    bool clipBoxes{true};
};

foundation::Result<LetterboxGeometry> CreateLetterboxGeometry(float originalWidth, float originalHeight, float networkWidth, float networkHeight, bool allowScaleUp);
foundation::Result<DetectionBox> RestoreLetterboxedBoxToOriginal(const DetectionBox &networkBox, const LetterboxGeometry &geometry, bool clip);
foundation::Result<std::vector<Detection>> RestoreLetterboxedDetectionsToOriginal(const std::vector<Detection> &networkDetections, const LetterboxGeometry &geometry, bool clip);
foundation::Result<std::vector<DetectionOverlayItem>> CreateDetectionOverlayItems(const std::vector<Detection> &detections, const std::vector<std::string> &classNames, float imageWidth, float imageHeight);
foundation::Result<std::vector<Detection>> DecodeYoloCenterDetections(const std::vector<float> &rawOutput, int rowCount, int classCount, float imageWidth, float imageHeight, const YoloDetectionDecodeConfig &config);
foundation::Result<std::vector<Detection>> DecodeYoloCenterDetectionsFromLetterbox(const std::vector<float> &rawOutput, int rowCount, int classCount, const LetterboxGeometry &geometry, const YoloDetectionDecodeConfig &config);
float IntersectionOverUnion(const DetectionBox &first, const DetectionBox &second);
}
