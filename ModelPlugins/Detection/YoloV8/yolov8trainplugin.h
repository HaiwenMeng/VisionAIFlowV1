#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"
#include "yolov8trainer.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <QObject>
#include <thread>

namespace visionaiflow::yolov8
{
class YoloV8TrainPlugin final : public QObject, public plugin_api::IDetectionTrainer
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID VISIONAIFLOW_DETECTION_TRAINER_IID FILE "yolov8trainplugin.json")
    Q_INTERFACES(visionaiflow::plugin_api::IDetectionTrainer)

public:
    YoloV8TrainPlugin();
    ~YoloV8TrainPlugin() override;

    plugin_api::PluginInfo pluginInfo() const override;
    plugin_api::TrainState state() const override;
    QString errorMessage() const override;
    bool stop() override;
    bool waitForStopped(int timeoutMs) override;
    bool initialize(const plugin_api::DetectionTrainConfig &config) override;
    bool startTrain() override;
    plugin_api::DetectionTrainProgress progress() const override;
    plugin_api::DetectionMetrics metrics() const override;
    plugin_api::DetectionTrainerCapabilities capabilities() const override;
    QVector<plugin_api::PluginParameterDefinition> parameterDefinitions() const override;
    bool exportModel(const QString &outputPath, const QString &format) override;

private:
    void runTraining();
    void setError(const QString &errorMessage);

    mutable std::mutex m_mutex;
    std::condition_variable m_finishedCondition;
    std::thread m_worker;
    std::atomic_bool m_stopRequested{false};
    plugin_api::TrainState m_state{plugin_api::TrainState::Idle};
    plugin_api::DetectionTrainProgress m_progress;
    plugin_api::DetectionMetrics m_metrics;
    QString m_errorMessage;
    bool m_finished{true};
    YoloV8Trainer m_trainer;
};
} // namespace visionaiflow::yolov8
