#include "detecttrainingcontroller.h"

#include "taskrepository.h"
#include "ytyolodefine.h"
#include "visionaiflow/plugin_api/PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

using visionaiflow::plugin_api::DetectionTrainConfig;
using visionaiflow::plugin_api::PluginManager;
using visionaiflow::plugin_api::TrainState;

DetectTrainingController::DetectTrainingController(QObject *parent)
    : QObject(parent), m_pluginManager(std::make_unique<PluginManager>()), m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, &DetectTrainingController::PollPluginState);
}

DetectTrainingController::~DetectTrainingController()
{
    Cancel();
}

void DetectTrainingController::Start(const DetectTrainingRequest &request)
{
    if (IsRunning())
    {
        emit Failed(QString(u8"训练任务正在运行"));
        return;
    }
    if (request.pluginPath.isEmpty() || !QFileInfo::exists(request.pluginPath))
    {
        emit Failed(QString(u8"未选择有效的检测训练插件"));
        return;
    }
    if (request.modelVariant.isEmpty())
    {
        emit Failed(QString(u8"未选择模型规格"));
        return;
    }

    const QString datasetPath =
        QDir(YtYoloDefine::toGetDataPath()).filePath(request.taskName + QStringLiteral("/train.csv"));
    if (!QFileInfo::exists(datasetPath))
    {
        emit Failed(QString(u8"未找到已生成的数据集: %1").arg(datasetPath));
        return;
    }

    TaskDefinition task;
    QString taskError;
    if (!TaskRepository::LoadTask(request.taskName, &task, &taskError) || task.labels.isEmpty())
    {
        emit Failed(taskError.isEmpty() ? QString(u8"检测训练任务缺少类别标签") : taskError);
        return;
    }

    if (!m_pluginManager->loadDetectionPlugin(request.pluginPath))
    {
        emit Failed(m_pluginManager->errorMessage());
        return;
    }

    DetectionTrainConfig config;
    config.datasetPath = datasetPath;
    config.outputPath =
        QDir(YtYoloDefine::toGetTrainPath()).filePath(request.taskName + QStringLiteral("/detect/train"));
    config.classNames = task.labels;
    config.epochs = request.epochs;
    config.batchSize = request.batchSize;
    config.imageWidth = 640;
    config.imageHeight = 640;
    config.learningRate = request.learningRate;
    config.gpuId = 0;
    config.resumeCheckpointPath = request.resumeCheckpointPath;
    config.algorithmOptions.insert(QStringLiteral("model_variant"), request.modelVariant);
    if (!m_pluginManager->detectionTrainer()->initialize(config))
    {
        const QString errorMessage = m_pluginManager->detectionTrainer()->errorMessage();
        emit Failed(errorMessage);
        return;
    }
    if (!m_pluginManager->detectionTrainer()->startTrain())
    {
        const QString errorMessage = m_pluginManager->detectionTrainer()->errorMessage();
        emit Failed(errorMessage);
        return;
    }

    m_outputDirectory = config.outputPath;
    m_lastReportedEpoch = 0;
    m_pollTimer->start();
    emit StateChanged(true);
}

QVector<DetectionPluginDescriptor> DetectTrainingController::DiscoverPlugins(QStringList *errorMessages) const
{
    if (errorMessages != nullptr)
    {
        errorMessages->clear();
    }

    const QString pluginDirectory =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("AIModelPlugins"));
    const QVector<visionaiflow::plugin_api::DetectionPluginMetadata> plugins =
        m_pluginManager->scanDetectionPluginMetadata(pluginDirectory, errorMessages);
    QVector<DetectionPluginDescriptor> descriptors;
    for (const visionaiflow::plugin_api::DetectionPluginMetadata &plugin : plugins)
    {
        descriptors.append({plugin.filePath,
                            plugin.info.id,
                            plugin.info.displayName,
                            plugin.info.version,
                            plugin.capabilities.supportsExport});
    }
    return descriptors;
}

void DetectTrainingController::Cancel()
{
    if (m_pluginManager->detectionTrainer() != nullptr && !m_pluginManager->detectionTrainer()->stop())
    {
        emit Failed(m_pluginManager->detectionTrainer()->errorMessage());
    }
}

bool DetectTrainingController::IsRunning() const noexcept
{
    if (m_pluginManager->detectionTrainer() == nullptr)
    {
        return false;
    }
    const TrainState pluginState = m_pluginManager->detectionTrainer()->state();
    return pluginState == TrainState::Running || pluginState == TrainState::Stopping;
}

void DetectTrainingController::PollPluginState()
{
    auto *trainer = m_pluginManager->detectionTrainer();
    if (trainer == nullptr)
    {
        m_pollTimer->stop();
        return;
    }

    const auto progress = trainer->progress();
    if (progress.train.epoch > m_lastReportedEpoch)
    {
        m_lastReportedEpoch = progress.train.epoch;
        emit EpochProgress(progress.train.epoch,
                           progress.train.step,
                           progress.train.loss,
                           progress.boxLoss,
                           progress.classLoss,
                           progress.positiveCount,
                           progress.meanIou);
    }

    const TrainState pluginState = trainer->state();
    if (pluginState == TrainState::Running || pluginState == TrainState::Stopping)
    {
        return;
    }

    m_pollTimer->stop();
    if (pluginState == TrainState::Failed)
    {
        const QString errorMessage = trainer->errorMessage();
        emit Failed(errorMessage);
    }
    else if (progress.train.message == QStringLiteral("cancelled"))
    {
        emit Cancelled();
    }
    else
    {
        const QString bestCheckpointPath = progress.bestCheckpointPath;
        const QString modelPath = progress.modelPath;
        emit Completed(m_outputDirectory, modelPath, bestCheckpointPath);
    }
    emit StateChanged(false);
}
