#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace visionaiflow::openvino_host
{
struct OpenVinoRuntimeMetadata final
{
    QString requestedDevice;
    QStringList executionDevices;
    QString fullDeviceName;
    QString inferencePrecision;
    QString performanceHint;
    int inferenceNumThreads{-1};
};

struct OpenVinoInferenceResult final
{
    QVector<float> values;
    OpenVinoRuntimeMetadata runtime;
};

foundation::Result<OpenVinoInferenceResult> RunClassificationOnnxWithMetadata(const QString &onnxPath, const QVector<float> &features);
foundation::Result<QVector<float>> RunClassificationOnnx(const QString &onnxPath, const QVector<float> &features);
foundation::Result<OpenVinoInferenceResult> RunYolo11RawHeadOnnxWithMetadata(const QString &onnxPath, const QVector<float> &image, int channels, int height, int width);
foundation::Result<QVector<float>> RunYolo11RawHeadOnnx(const QString &onnxPath, const QVector<float> &image, int channels, int height, int width);
}
