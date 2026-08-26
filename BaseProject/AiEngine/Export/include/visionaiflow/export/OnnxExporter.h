#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/yolo11/Yolo11Detector.h"

#include <QString>
#include <QStringList>

#include <cstdint>

#ifndef VISIONAIFLOW_EXPORT_API
#if defined(VISIONAIFLOW_EXPORT_LIBRARY)
#define VISIONAIFLOW_EXPORT_API __declspec(dllexport)
#else
#define VISIONAIFLOW_EXPORT_API __declspec(dllimport)
#endif
#endif

namespace visionaiflow::exporter
{
struct OnnxFileContract final
{
    int64_t irVersion{9};
    int64_t opsetVersion{12};
    QStringList allowedOps;
};

VISIONAIFLOW_EXPORT_API foundation::Result<void> ValidateOnnxFileContract(const QString &path,
                                                                          const OnnxFileContract &contract);
VISIONAIFLOW_EXPORT_API foundation::Result<void> ExportYolo11TinyDetectorOnnx(const QString &path,
                                                                              models::yolo11::Yolo11TinyDetector &model,
                                                                              int64_t inputChannels,
                                                                              int64_t imageHeight,
                                                                              int64_t imageWidth,
                                                                              int64_t rowCount,
                                                                              int64_t classCount);
VISIONAIFLOW_EXPORT_API foundation::Result<void>
ExportYolo11GridDetectorOnnx(const QString &path, models::yolo11::Yolo11GridDetector &model, int64_t classCount);
} // namespace visionaiflow::exporter
