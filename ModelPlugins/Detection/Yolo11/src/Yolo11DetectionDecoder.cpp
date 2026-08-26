#include "visionaiflow/models/yolo11/Yolo11DetectionDecoder.h"

namespace visionaiflow::models::yolo11
{
foundation::Result<LetterboxGeometry> CreateYolo11LetterboxGeometry(const float originalWidth, const float originalHeight, const float networkWidth, const float networkHeight, const bool allowScaleUp)
{
    return common::CreateLetterboxGeometry(originalWidth, originalHeight, networkWidth, networkHeight, allowScaleUp);
}

foundation::Result<std::vector<Detection>> DecodeYolo11Detections(const std::vector<float> &rawOutput, const int rowCount, const int classCount, const float imageWidth, const float imageHeight, const Yolo11DetectionDecodeConfig &config)
{
    return common::DecodeYoloCenterDetections(rawOutput, rowCount, classCount, imageWidth, imageHeight, config);
}

foundation::Result<std::vector<Detection>> DecodeYolo11DetectionsFromLetterbox(const std::vector<float> &rawOutput, const int rowCount, const int classCount, const LetterboxGeometry &geometry, const Yolo11DetectionDecodeConfig &config)
{
    return common::DecodeYoloCenterDetectionsFromLetterbox(rawOutput, rowCount, classCount, geometry, config);
}
}
