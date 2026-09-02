#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"
#include "yolov11inference.h"
#include "yolov11trainer.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <QObject>
#include <thread>

namespace visionaiflow::yolov11
{
class Yolo11Plugin final : public QObject, public plugin_api::IDetectionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID VISIONAIFLOW_DETECTION_PLUGIN_IID
                      FILE "yolov11plugin.json")
    Q_INTERFACES(visionaiflow::plugin_api::IDetectionPlugin)

public:
    Yolo11Plugin();
    ~Yolo11Plugin() override;

    plugin_api::PluginInfo pluginInfo() const override;
    plugin_api::TrainState state() const override;
    QString errorMessage() const override;
    bool stop() override;
    bool waitForStopped(int timeoutMs) override;
    bool initializeTraining(const plugin_api::DetectionTrainConfig &config) override;
    bool startTrain() override;
    plugin_api::DetectionTrainProgress progress() const override;
    plugin_api::DetectionMetrics metrics() const override;
    bool loadInferenceModel(const plugin_api::DetectionInferConfig &config) override;
    bool infer(const plugin_api::DetectionInferRequest &request, plugin_api::DetectionInferResult *result) override;
    bool exportModel(const plugin_api::ModelExportConfig &config) override;
    bool exportBackbone(const plugin_api::BackboneExportConfig &config) override;
    plugin_api::DetectionPluginCapabilities capabilities() const override;
    QVector<plugin_api::PluginParameterDefinition> parameterDefinitions() const override;

private:
    void runTraining();
    void setError(const QString &errorMessage);

    mutable std::mutex m_mutex;
    std::mutex m_inferenceMutex;
    std::condition_variable m_finishedCondition;
    std::thread m_worker;
    std::atomic_bool m_stopRequested{false};
    plugin_api::TrainState m_state{plugin_api::TrainState::Idle};
    plugin_api::DetectionTrainProgress m_progress;
    plugin_api::DetectionMetrics m_metrics;
    QString m_errorMessage;
    bool m_finished{true};
    Yolo11Trainer m_trainer;
    std::unique_ptr<Yolo11Inference> m_inference;
};
} // namespace visionaiflow::yolov11
