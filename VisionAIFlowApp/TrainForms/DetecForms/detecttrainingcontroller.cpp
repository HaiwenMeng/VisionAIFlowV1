#include "detecttrainingcontroller.h"

#include "taskrepository.h"
#include "ytyolodefine.h"
#include "visionaiflow/plugin_api/PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

using visionaiflow::plugin_api::DetectionTrainConfig;
using visionaiflow::plugin_api::ModelExportConfig;
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
    auto *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr)
    {
        emit Failed(QString(u8"检测训练插件加载后为空"));
        return;
    }
    if (plugin->pluginInfo().id == QStringLiteral("visionaiflow.detection.yolov11"))
    {
        config.pretrainedPath =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Pretrained/yolo11n.pt"));
        config.algorithmOptions.insert(QStringLiteral("plots"), true);
        config.algorithmOptions.insert(QStringLiteral("mosaic"), request.mosaic ? 1.0 : 0.0);
        config.algorithmOptions.insert(QStringLiteral("close_mosaic"), 0);
        config.algorithmOptions.insert(QStringLiteral("mixup"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("copy_paste"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("hsv_h"), 0.015);
        config.algorithmOptions.insert(QStringLiteral("hsv_s"), 0.7);
        config.algorithmOptions.insert(QStringLiteral("hsv_v"), 0.4);
        config.algorithmOptions.insert(QStringLiteral("degrees"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("translate"), 0.1);
        config.algorithmOptions.insert(QStringLiteral("scale"), 0.5);
        config.algorithmOptions.insert(QStringLiteral("shear"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("perspective"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("flipud"), 0.0);
        config.algorithmOptions.insert(QStringLiteral("fliplr"), request.horizontalFlip ? 0.5 : 0.0);
        config.algorithmOptions.insert(QStringLiteral("seed"), 0);
    }
    if (!plugin->initializeTraining(config))
    {
        const QString errorMessage = plugin->errorMessage();
        emit Failed(errorMessage);
        return;
    }
    if (!plugin->startTrain())
    {
        const QString errorMessage = plugin->errorMessage();
        emit Failed(errorMessage);
        return;
    }

    m_outputDirectory = config.outputPath;
    m_lastReportedEpoch = 0;
    m_totalEpochs = request.epochs;
    m_elapsedTimer.restart();
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
    auto *plugin = m_pluginManager->detectionPlugin();
    if (plugin != nullptr && !plugin->stop())
    {
        emit Failed(plugin->errorMessage());
    }
}

bool DetectTrainingController::IsRunning() const noexcept
{
    const auto *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr)
    {
        return false;
    }
    const TrainState pluginState = plugin->state();
    return pluginState == TrainState::Running || pluginState == TrainState::Stopping;
}

bool DetectTrainingController::ExportModel(const DetectModelExportRequest &request, QString *errorMessage)
{
    if (errorMessage == nullptr)
    {
        return false;
    }
    if (IsRunning())
    {
        *errorMessage = QString(u8"训练任务正在运行");
        return false;
    }
    if (request.pluginPath.isEmpty() || !QFileInfo::exists(request.pluginPath) || request.checkpointPath.isEmpty() ||
        !QFileInfo::exists(request.checkpointPath) || request.outputPath.isEmpty() || request.imageWidth <= 0 ||
        request.imageHeight <= 0 || request.batchSize <= 0)
    {
        *errorMessage = QString(u8"ONNX 导出参数无效");
        return false;
    }
    if (!m_pluginManager->loadDetectionPlugin(request.pluginPath))
    {
        *errorMessage = m_pluginManager->errorMessage();
        return false;
    }
    auto *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr || !plugin->capabilities().supportsExport)
    {
        *errorMessage = QString(u8"当前检测训练插件不支持 ONNX 导出");
        return false;
    }

    ModelExportConfig config;
    config.checkpointPath = request.checkpointPath;
    config.outputPath = request.outputPath;
    config.format = QStringLiteral("onnx");
    config.imageWidth = request.imageWidth;
    config.imageHeight = request.imageHeight;
    config.batchSize = request.batchSize;
    config.metadata = request.metadata;
    if (!plugin->exportModel(config))
    {
        *errorMessage = plugin->errorMessage();
        return false;
    }
    return true;
}

void DetectTrainingController::PollPluginState()
{
    auto *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr)
    {
        m_pollTimer->stop();
        return;
    }

    const auto progress = plugin->progress();
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

    const TrainState pluginState = plugin->state();
    if (pluginState == TrainState::Running || pluginState == TrainState::Stopping)
    {
        return;
    }

    m_pollTimer->stop();
    if (pluginState == TrainState::Failed)
    {
        const QString errorMessage = plugin->errorMessage();
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
        const double elapsedHours = static_cast<double>(m_elapsedTimer.elapsed()) / 3600000.0;
        const QString durationMessage =
            QStringLiteral("[%1] epochs completed in [%2] hours.").arg(m_totalEpochs).arg(elapsedHours, 0, 'f', 3);
        emit Completed(m_outputDirectory, modelPath, bestCheckpointPath, durationMessage);
    }
    emit StateChanged(false);
}
