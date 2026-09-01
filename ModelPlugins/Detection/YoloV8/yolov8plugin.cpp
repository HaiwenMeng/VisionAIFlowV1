#include "yolov8plugin.h"

#pragma execution_character_set("utf-8")

#include <QDebug>

#include <chrono>
#include <limits>

namespace visionaiflow::yolov8
{
using plugin_api::BackboneExportConfig;
using plugin_api::DetectionInferConfig;
using plugin_api::DetectionInferRequest;
using plugin_api::DetectionInferResult;
using plugin_api::DetectionMetrics;
using plugin_api::DetectionPluginCapabilities;
using plugin_api::DetectionTrainConfig;
using plugin_api::DetectionTrainProgress;
using plugin_api::ModelExportConfig;
using plugin_api::PluginInfo;
using plugin_api::PluginParameterDefinition;
using plugin_api::TrainState;

YoloV8Plugin::YoloV8Plugin() = default;

YoloV8Plugin::~YoloV8Plugin()
{
    stop();
    waitForStopped(-1);
}

PluginInfo YoloV8Plugin::pluginInfo() const
{
    return {QStringLiteral("visionaiflow.detection.yolov8"),
            QStringLiteral("YOLOv8"),
            QStringLiteral("2.0.0"),
            plugin_api::TrainTaskType::Detection};
}

TrainState YoloV8Plugin::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

QString YoloV8Plugin::errorMessage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool YoloV8Plugin::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == TrainState::Running)
    {
        m_stopRequested.store(true);
        m_state = TrainState::Stopping;
    }
    return true;
}

bool YoloV8Plugin::waitForStopped(const int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_finished)
    {
        if (timeoutMs < 0)
        {
            m_finishedCondition.wait(lock,
                                     [this]()
                                     {
                                         return m_finished;
                                     });
        }
        else if (!m_finishedCondition.wait_for(lock,
                                               std::chrono::milliseconds(timeoutMs),
                                               [this]()
                                               {
                                                   return m_finished;
                                               }))
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

bool YoloV8Plugin::initializeTraining(const DetectionTrainConfig &config)
{
    if (!waitForStopped(0))
    {
        setError(QString(u8"YOLOv8 训练线程仍在运行, 无法重新初始化"));
        return false;
    }

    std::lock_guard<std::mutex> inferenceLock(m_inferenceMutex);
    m_inference.reset();
    QString errorMessage;
    if (!m_trainer.initialize(config, &errorMessage))
    {
        setError(errorMessage);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress = {};
    m_metrics = {std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN()};
    m_errorMessage.clear();
    m_stopRequested.store(false);
    m_finished = true;
    m_state = TrainState::Initialized;
    return true;
}

bool YoloV8Plugin::startTrain()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != TrainState::Initialized)
        {
            m_errorMessage = QString(u8"YOLOv8 训练插件尚未初始化");
            qWarning().noquote() << m_errorMessage;
            return false;
        }

        m_stopRequested.store(false);
        m_finished = false;
        m_state = TrainState::Running;
    }

    std::lock_guard<std::mutex> inferenceLock(m_inferenceMutex);
    m_inference.reset();
    m_worker = std::thread(&YoloV8Plugin::runTraining, this);
    return true;
}

DetectionTrainProgress YoloV8Plugin::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

DetectionMetrics YoloV8Plugin::metrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

bool YoloV8Plugin::loadInferenceModel(const DetectionInferConfig &config)
{
    const TrainState trainState = state();
    if (trainState == TrainState::Running || trainState == TrainState::Stopping)
    {
        setError(QString(u8"YOLOv8 训练正在运行或停止中, 无法加载推理模型"));
        return false;
    }

    std::lock_guard<std::mutex> inferenceLock(m_inferenceMutex);
    auto inference = std::make_unique<YoloV8Inference>();
    QString errorMessage;
    if (!inference->loadModel(config, &errorMessage))
    {
        setError(errorMessage);
        return false;
    }

    m_inference = std::move(inference);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorMessage.clear();
    }
    return true;
}

bool YoloV8Plugin::infer(const DetectionInferRequest &request, DetectionInferResult *result)
{
    const TrainState trainState = state();
    if (trainState == TrainState::Running || trainState == TrainState::Stopping)
    {
        setError(QString(u8"YOLOv8 训练正在运行或停止中, 无法执行推理"));
        return false;
    }

    std::lock_guard<std::mutex> inferenceLock(m_inferenceMutex);
    if (m_inference == nullptr)
    {
        setError(QString(u8"YOLOv8 推理模型尚未加载"));
        return false;
    }

    QString errorMessage;
    if (!m_inference->infer(request, result, &errorMessage))
    {
        setError(errorMessage);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorMessage.clear();
    return true;
}

bool YoloV8Plugin::exportModel(const ModelExportConfig &config)
{
    Q_UNUSED(config)
    setError(QString(u8"YOLOv8 插件不支持模型导出"));
    return false;
}

bool YoloV8Plugin::exportBackbone(const BackboneExportConfig &config)
{
    Q_UNUSED(config)
    setError(QString(u8"YOLOv8 插件不支持 Backbone 导出"));
    return false;
}

DetectionPluginCapabilities YoloV8Plugin::capabilities() const
{
    DetectionPluginCapabilities values;
    values.supportsResume = true;
    values.supportsPretrained = true;
    values.supportsFp16 = true;
    return values;
}

QVector<PluginParameterDefinition> YoloV8Plugin::parameterDefinitions() const
{
    return {
        {QStringLiteral("model_variant"), QString(u8"模型规格"), QStringLiteral("yolov8n"), {}, {}, QString(u8"模型")}};
}

void YoloV8Plugin::runTraining()
{
    QString errorMessage;
    const TrainRunResult result = m_trainer.train(
        m_stopRequested,
        [this](const DetectionTrainProgress &progress)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progress = progress;
        },
        &errorMessage);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (result == TrainRunResult::Failed)
        {
            m_errorMessage = errorMessage.isEmpty() ? QString(u8"YOLOv8 训练失败") : errorMessage;
            qWarning().noquote() << m_errorMessage;
            m_state = TrainState::Failed;
        }
        else
        {
            m_progress.train.message =
                result == TrainRunResult::Cancelled ? QStringLiteral("cancelled") : QStringLiteral("completed");
            m_state = TrainState::Completed;
        }
        m_finished = true;
    }
    m_finishedCondition.notify_all();
}

void YoloV8Plugin::setError(const QString &errorMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorMessage = errorMessage;
    qWarning().noquote() << errorMessage;
}
} // namespace visionaiflow::yolov8
