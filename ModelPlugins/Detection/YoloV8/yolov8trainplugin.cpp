#include "yolov8trainplugin.h"

#pragma execution_character_set("utf-8")

#include <QDebug>

#include <chrono>
#include <cmath>
#include <limits>

namespace visionaiflow::yolov8
{
using plugin_api::DetectionMetrics;
using plugin_api::DetectionTrainConfig;
using plugin_api::DetectionTrainerCapabilities;
using plugin_api::DetectionTrainProgress;
using plugin_api::PluginInfo;
using plugin_api::PluginParameterDefinition;
using plugin_api::TrainState;

YoloV8TrainPlugin::YoloV8TrainPlugin() = default;

YoloV8TrainPlugin::~YoloV8TrainPlugin()
{
    stop();
    waitForStopped(-1);
}

PluginInfo YoloV8TrainPlugin::pluginInfo() const
{
    return {QStringLiteral("visionaiflow.detection.yolov8"),
            QStringLiteral("YOLOv8"),
            QStringLiteral("1.0.0"),
            plugin_api::TrainTaskType::Detection};
}

TrainState YoloV8TrainPlugin::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

QString YoloV8TrainPlugin::errorMessage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool YoloV8TrainPlugin::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == TrainState::Running)
    {
        m_stopRequested.store(true);
        m_state = TrainState::Stopping;
    }
    return true;
}

bool YoloV8TrainPlugin::waitForStopped(const int timeoutMs)
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

bool YoloV8TrainPlugin::initialize(const DetectionTrainConfig &config)
{
    if (!waitForStopped(0))
    {
        setError(QString(u8"YOLOv8 训练线程仍在运行，无法重新初始化"));
        return false;
    }

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

bool YoloV8TrainPlugin::startTrain()
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
    m_worker = std::thread(&YoloV8TrainPlugin::runTraining, this);
    return true;
}

DetectionTrainProgress YoloV8TrainPlugin::progress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_progress;
}

DetectionMetrics YoloV8TrainPlugin::metrics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

DetectionTrainerCapabilities YoloV8TrainPlugin::capabilities() const
{
    DetectionTrainerCapabilities values;
    values.supportsResume = true;
    return values;
}

QVector<PluginParameterDefinition> YoloV8TrainPlugin::parameterDefinitions() const
{
    return {
        {QStringLiteral("model_variant"), QString(u8"模型规格"), QStringLiteral("yolov8n"), {}, {}, QString(u8"模型")}};
}

bool YoloV8TrainPlugin::exportModel(const QString &outputPath, const QString &format)
{
    Q_UNUSED(outputPath)
    Q_UNUSED(format)
    setError(QString(u8"YOLOv8 训练插件当前不支持 ONNX 导出"));
    return false;
}

void YoloV8TrainPlugin::runTraining()
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
            if (result == TrainRunResult::Cancelled)
            {
                m_progress.train.message = QStringLiteral("cancelled");
            }
            else
            {
                m_progress.train.message = QStringLiteral("completed");
            }
            m_state = TrainState::Completed;
        }
        m_finished = true;
    }
    m_finishedCondition.notify_all();
}

void YoloV8TrainPlugin::setError(const QString &errorMessage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorMessage = errorMessage;
    qWarning().noquote() << errorMessage;
}
} // namespace visionaiflow::yolov8
