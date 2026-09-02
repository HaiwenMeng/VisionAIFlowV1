#include "yolov11plugin.h"

#include <QDebug>

#include <chrono>
#include <limits>

namespace visionaiflow::yolov11
{
Yolo11Plugin::Yolo11Plugin() = default;

Yolo11Plugin::~Yolo11Plugin()
{
    stop();
    waitForStopped(-1);
}

plugin_api::PluginInfo Yolo11Plugin::pluginInfo() const
{
    return {QStringLiteral("visionaiflow.detection.yolov11"), QStringLiteral("YOLO11"), QStringLiteral("1.0.0"),
            plugin_api::TrainTaskType::Detection};
}

plugin_api::TrainState Yolo11Plugin::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

QString Yolo11Plugin::errorMessage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool Yolo11Plugin::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == plugin_api::TrainState::Running)
    {
        m_stopRequested.store(true);
        m_state = plugin_api::TrainState::Stopping;
    }
    return true;
}

bool Yolo11Plugin::waitForStopped(const int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_finished)
    {
        if (timeoutMs < 0)
        {
            m_finishedCondition.wait(lock, [this]() { return m_finished; });
        }
        else if (!m_finishedCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { return m_finished; }))
        {
            return false;
        }
    }
    lock.unlock();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
    return true;
}

bool Yolo11Plugin::initializeTraining(const plugin_api::DetectionTrainConfig &config)
{
    if (!waitForStopped(0))
    {
        setError(QString(u8"YOLO11 训练线程仍在运行, 无法重新初始化"));
        return false;
    }
    QString error;
    if (!m_trainer.initialize(config, &error))
    {
        setError(error);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress = {};
    m_metrics = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN()};
    m_errorMessage.clear();
    m_stopRequested.store(false);
    m_finished = true;
    m_state = plugin_api::TrainState::Initialized;
    return true;
}

bool Yolo11Plugin::startTrain()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != plugin_api::TrainState::Initialized)
        {
            m_errorMessage = QString(u8"YOLO11 训练插件尚未初始化");
            return false;
        }
        m_stopRequested.store(false);
        m_finished = false;
        m_state = plugin_api::TrainState::Running;
    }
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inference.reset();
    m_worker = std::thread(&Yolo11Plugin::runTraining, this);
    return true;
}

plugin_api::DetectionTrainProgress Yolo11Plugin::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

plugin_api::DetectionMetrics Yolo11Plugin::metrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

bool Yolo11Plugin::loadInferenceModel(const plugin_api::DetectionInferConfig &config)
{
    if (state() == plugin_api::TrainState::Running || state() == plugin_api::TrainState::Stopping)
    {
        setError(QString(u8"YOLO11 训练运行时不能加载推理模型"));
        return false;
    }
    auto inference = std::make_unique<Yolo11Inference>();
    QString error;
    if (!inference->loadModel(config, &error))
    {
        setError(error);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inference = std::move(inference);
    return true;
}

bool Yolo11Plugin::infer(const plugin_api::DetectionInferRequest &request, plugin_api::DetectionInferResult *result)
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    if (m_inference == nullptr)
    {
        setError(QString(u8"YOLO11 推理模型尚未加载"));
        return false;
    }
    QString error;
    if (!m_inference->infer(request, result, &error))
    {
        setError(error);
        return false;
    }
    return true;
}

bool Yolo11Plugin::exportModel(const plugin_api::ModelExportConfig &config)
{
    Q_UNUSED(config)
    setError(QString(u8"YOLO11 插件不支持模型导出"));
    return false;
}

bool Yolo11Plugin::exportBackbone(const plugin_api::BackboneExportConfig &config)
{
    Q_UNUSED(config)
    setError(QString(u8"YOLO11 插件不支持 Backbone 导出"));
    return false;
}

plugin_api::DetectionPluginCapabilities Yolo11Plugin::capabilities() const
{
    plugin_api::DetectionPluginCapabilities capabilities;
    capabilities.supportsPretrained = true;
    return capabilities;
}

QVector<plugin_api::PluginParameterDefinition> Yolo11Plugin::parameterDefinitions() const
{
    return {{QStringLiteral("model_variant"), QString(u8"模型规格"), QStringLiteral("yolo11n"), {}, {}, QString(u8"模型")}};
}

void Yolo11Plugin::runTraining()
{
    QString error;
    const TrainRunResult result = m_trainer.train(m_stopRequested,
                                                  [this](const plugin_api::DetectionTrainProgress &progress)
                                                  {
                                                      std::lock_guard<std::mutex> lock(m_mutex);
                                                      m_progress = progress;
                                                  },
                                                  &error);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (result == TrainRunResult::Failed)
        {
            m_errorMessage = error.isEmpty() ? QString(u8"YOLO11 训练失败") : error;
            m_state = plugin_api::TrainState::Failed;
        }
        else
        {
            m_progress.train.message = result == TrainRunResult::Cancelled ? QStringLiteral("cancelled") : QStringLiteral("completed");
            m_state = plugin_api::TrainState::Completed;
        }
        m_finished = true;
    }
    m_finishedCondition.notify_all();
}

void Yolo11Plugin::setError(const QString &errorMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorMessage = errorMessage;
    qWarning().noquote() << errorMessage;
}
} // namespace visionaiflow::yolov11
