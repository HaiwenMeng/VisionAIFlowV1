#include "visionaiflow/plugin_api/PluginManager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

namespace visionaiflow::plugin_api
{
namespace
{
bool ReadDetectionPluginMetadata(const QString &filePath,
                                 DetectionPluginMetadata *pluginMetadata,
                                 QString *errorMessage)
{
    QPluginLoader pluginLoader(filePath);
    const QJsonObject pluginDocument = pluginLoader.metaData();
    if (pluginDocument.value(QStringLiteral("IID")).toString() != QStringLiteral(VISIONAIFLOW_DETECTION_TRAINER_IID))
    {
        *errorMessage = QString(u8"不是兼容的检测训练 Qt 插件: %1").arg(filePath);
        return false;
    }

    const QJsonObject metadata = pluginDocument.value(QStringLiteral("MetaData")).toObject();
    const QString pluginId = metadata.value(QStringLiteral("plugin_id")).toString();
    const QString displayName = metadata.value(QStringLiteral("display_name")).toString();
    const QString version = metadata.value(QStringLiteral("version")).toString();
    if (pluginId.isEmpty() || displayName.isEmpty() || version.isEmpty() ||
        metadata.value(QStringLiteral("task_type")).toString() != QStringLiteral("detection"))
    {
        *errorMessage = QString(u8"检测训练 Qt 插件元数据不完整: %1").arg(filePath);
        return false;
    }

    pluginMetadata->filePath = filePath;
    pluginMetadata->info = {pluginId, displayName, version, TrainTaskType::Detection};
    pluginMetadata->capabilities.supportsExport = metadata.value(QStringLiteral("supports_export")).toBool(false);
    return true;
}
} // namespace

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() = default;

QVector<DetectionPluginMetadata> PluginManager::scanDetectionPluginMetadata(const QString &directoryPath,
                                                                            QStringList *errorMessages) const
{
    if (errorMessages != nullptr)
    {
        errorMessages->clear();
    }

    const QDir directory(directoryPath);
    if (!directory.exists())
    {
        if (errorMessages != nullptr)
        {
            errorMessages->append(QString(u8"训练插件目录不存在: %1").arg(directoryPath));
        }
        return {};
    }

    QVector<DetectionPluginMetadata> plugins;
    const QFileInfoList entries = directory.entryInfoList({QStringLiteral("*.dll")}, QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries)
    {
        DetectionPluginMetadata metadata;
        QString errorMessage;
        if (ReadDetectionPluginMetadata(entry.absoluteFilePath(), &metadata, &errorMessage))
        {
            plugins.append(std::move(metadata));
        }
        else if (errorMessages != nullptr)
        {
            errorMessages->append(errorMessage);
        }
    }
    return plugins;
}

bool PluginManager::loadDetectionPlugin(const QString &filePath)
{
    if (!QFileInfo::exists(filePath))
    {
        m_errorMessage = QString(u8"检测训练插件 DLL 不存在: %1").arg(filePath);
        return false;
    }

    const auto loadedTrainer = m_loadedTrainers.constFind(filePath);
    if (loadedTrainer != m_loadedTrainers.cend())
    {
        m_detectionTrainer = loadedTrainer.value();
        m_errorMessage.clear();
        return true;
    }

    auto *pluginLoader = new QPluginLoader(filePath);
    QObject *pluginObject = pluginLoader->instance();
    if (pluginObject == nullptr)
    {
        m_errorMessage = QString(u8"无法加载检测训练 Qt 插件 %1: %2").arg(filePath, pluginLoader->errorString());
        delete pluginLoader;
        return false;
    }

    auto *trainer = qobject_cast<IDetectionTrainer *>(pluginObject);
    if (trainer == nullptr)
    {
        m_errorMessage = QString(u8"检测训练 Qt 插件未实现 IDetectionTrainer: %1").arg(filePath);
        m_loadedPluginLoaders.append(pluginLoader);
        return false;
    }
    if (trainer->pluginInfo().taskType != TrainTaskType::Detection)
    {
        m_errorMessage = QString(u8"检测训练 Qt 插件任务类型不匹配: %1").arg(filePath);
        m_loadedPluginLoaders.append(pluginLoader);
        return false;
    }

    m_detectionTrainer = trainer;
    m_loadedTrainers.insert(filePath, trainer);
    m_loadedPluginLoaders.append(pluginLoader);
    m_errorMessage.clear();
    return true;
}

IDetectionTrainer *PluginManager::detectionTrainer() const
{
    return m_detectionTrainer;
}

QString PluginManager::errorMessage() const
{
    return m_errorMessage;
}
} // namespace visionaiflow::plugin_api
