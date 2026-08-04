#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/training/LinearClassifierTrainer.h"
#include "visionaiflow/training/Yolo11DetectionTraining.h"

#include <QString>
#include <QStringList>

#include <cstdint>

namespace visionaiflow::exporter
{
struct OnnxFileContract final
{
    int64_t irVersion{9};
    int64_t opsetVersion{12};
    QStringList allowedOps;
};

foundation::Result<void> ValidateOnnxFileContract(const QString &path, const OnnxFileContract &contract);
foundation::Result<void> ExportLinearClassifierOnnx(const QString &path, training::LinearClassifier &model, int64_t inputFeatures, int64_t classCount);
foundation::Result<void> ExportYolo11TinyDetectorOnnx(const QString &path, training::Yolo11TinyDetector &model, int64_t inputChannels, int64_t imageHeight, int64_t imageWidth, int64_t rowCount, int64_t classCount);
}
