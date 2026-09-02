#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"
#include "yolov11network.h"

#include <atomic>
#include <functional>
#include <torch/script.h>

namespace visionaiflow::yolov11
{
enum class TrainRunResult
{
    Completed,
    Cancelled,
    Failed
};

class Yolo11Trainer final
{
public:
    using ProgressCallback = std::function<void(const plugin_api::DetectionTrainProgress &progress)>;

    bool initialize(const plugin_api::DetectionTrainConfig &config, QString *errorMessage);
    TrainRunResult train(std::atomic_bool &stopRequested, const ProgressCallback &onProgress, QString *errorMessage);

private:
    plugin_api::DetectionTrainConfig m_config;
    QString m_modelVariant;
};
} // namespace visionaiflow::yolov11
