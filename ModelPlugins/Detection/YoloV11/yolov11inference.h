#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"

#ifdef slots
#undef slots
#endif

#include <torch/script.h>

namespace visionaiflow::yolov11
{
bool EnsureYolo11TorchCudaBackend(QString *errorMessage);

bool InitializeYolo11CudaDevice(int gpuId,
                                torch::Device *device,
                                QString *errorMessage);

class Yolo11Inference final
{
public:
    bool loadModel(const plugin_api::DetectionInferConfig &config,
                   QString *errorMessage);

    bool infer(const plugin_api::DetectionInferRequest &request,
               plugin_api::DetectionInferResult *result,
               QString *errorMessage);

private:
    torch::jit::script::Module m_model;
    torch::Device m_device{torch::kCPU};
    QStringList m_classNames;
    int m_imageWidth{640};
    int m_imageHeight{640};
    bool m_loaded{false};
};
} // namespace visionaiflow::yolov11
