#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"
#include "yolov11network.h"

#ifdef slots
#undef slots
#endif

namespace visionaiflow::yolov11
{
bool EnsureYolo11TorchCudaBackend(QString *errorMessage);

bool InitializeYolo11CudaDevice(int gpuId, torch::Device *device, QString *errorMessage);

struct Yolo11CheckpointMetadata final
{
    QStringList classNames;
    int imageWidth{0};
    int imageHeight{0};
};

bool ReadYolo11CheckpointMetadata(const QString &checkpointPath,
                                  Yolo11CheckpointMetadata *metadata,
                                  QString *errorMessage);

class Yolo11Inference final
{
public:
    bool loadModel(const plugin_api::DetectionInferConfig &config, QString *errorMessage);

    bool infer(const plugin_api::DetectionInferRequest &request,
               plugin_api::DetectionInferResult *result,
               QString *errorMessage);

private:
    Yolo11Network m_model{nullptr};
    torch::Device m_device{torch::kCPU};
    QStringList m_classNames;
    int m_imageWidth{640};
    int m_imageHeight{640};
};
} // namespace visionaiflow::yolov11
