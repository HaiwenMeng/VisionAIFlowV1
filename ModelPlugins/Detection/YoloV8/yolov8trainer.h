#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"

#include <atomic>
#include <functional>

namespace visionaiflow::yolov8
{
enum class TrainRunResult
{
    Completed,
    Cancelled,
    Failed
};

class YoloV8Trainer final
{
public:
    using ProgressCallback = std::function<void(const plugin_api::DetectionTrainProgress &progress)>;

    bool initialize(const plugin_api::DetectionTrainConfig &config, QString *errorMessage);
    TrainRunResult train(std::atomic_bool &stopRequested, const ProgressCallback &onProgress, QString *errorMessage);

private:
    plugin_api::DetectionTrainConfig m_config;
    QString m_validationDatasetPath;
    QString m_modelVariant;
};
} // namespace visionaiflow::yolov8
