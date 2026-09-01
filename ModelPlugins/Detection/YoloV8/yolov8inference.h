#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"

#ifdef slots
#undef slots
#endif

#include "third_party/koba_jon/networks.hpp"

#include <memory>

namespace visionaiflow::yolov8
{
bool EnsureYoloV8TorchCudaBackend(QString *errorMessage);

class YoloV8Inference final
{
public:
    bool loadModel(const plugin_api::DetectionInferConfig &config, QString *errorMessage);
    bool infer(const plugin_api::DetectionInferRequest &request,
               plugin_api::DetectionInferResult *result,
               QString *errorMessage);

private:
    YOLOv8 m_model{nullptr};
    torch::Device m_device{torch::kCPU};
    QStringList m_classNames;
    int m_imageWidth{0};
    int m_imageHeight{0};
    bool m_useFp16{false};
};
} // namespace visionaiflow::yolov8
