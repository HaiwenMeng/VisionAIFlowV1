#pragma once

#include "visionaiflow/models/common/DetectionPostProcessor.h"

namespace visionaiflow::models::yolo11
{
using Detection = common::Detection;
using DetectionBox = common::DetectionBox;
using LetterboxGeometry = common::LetterboxGeometry;
using Yolo11DetectionDecodeConfig = common::YoloDetectionDecodeConfig;

foundation::Result<LetterboxGeometry> CreateYolo11LetterboxGeometry(float originalWidth, float originalHeight, float networkWidth, float networkHeight, bool allowScaleUp);
foundation::Result<std::vector<Detection>> DecodeYolo11Detections(const std::vector<float> &rawOutput, int rowCount, int classCount, float imageWidth, float imageHeight, const Yolo11DetectionDecodeConfig &config);
foundation::Result<std::vector<Detection>> DecodeYolo11DetectionsFromLetterbox(const std::vector<float> &rawOutput, int rowCount, int classCount, const LetterboxGeometry &geometry, const Yolo11DetectionDecodeConfig &config);
}
