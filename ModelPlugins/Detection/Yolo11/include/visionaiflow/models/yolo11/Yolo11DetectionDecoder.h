#pragma once

#include "visionaiflow/models/common/DetectionPostProcessor.h"

#if defined(VISIONAIFLOW_YOLO11_LIBRARY)
#define VISIONAIFLOW_YOLO11_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_YOLO11_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::models::yolo11
{
using Detection = common::Detection;
using DetectionBox = common::DetectionBox;
using LetterboxGeometry = common::LetterboxGeometry;
using Yolo11DetectionDecodeConfig = common::YoloDetectionDecodeConfig;

VISIONAIFLOW_YOLO11_EXPORT foundation::Result<LetterboxGeometry> CreateYolo11LetterboxGeometry(float originalWidth, float originalHeight, float networkWidth, float networkHeight, bool allowScaleUp);
VISIONAIFLOW_YOLO11_EXPORT foundation::Result<std::vector<Detection>> DecodeYolo11Detections(const std::vector<float> &rawOutput, int rowCount, int classCount, float imageWidth, float imageHeight, const Yolo11DetectionDecodeConfig &config);
VISIONAIFLOW_YOLO11_EXPORT foundation::Result<std::vector<Detection>> DecodeYolo11DetectionsFromLetterbox(const std::vector<float> &rawOutput, int rowCount, int classCount, const LetterboxGeometry &geometry, const Yolo11DetectionDecodeConfig &config);
}
