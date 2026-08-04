#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QVector>

namespace visionaiflow::tensorrt_host
{
foundation::Result<void> BuildClassificationEngineFromOnnx(const QString &onnxPath, int64_t featureCount);
foundation::Result<QVector<float>> RunClassificationOnnx(const QString &onnxPath, const QVector<float> &features);
foundation::Result<QVector<float>> RunYolo11RawHeadOnnx(const QString &onnxPath, const QVector<float> &image, int channels, int height, int width);
}
