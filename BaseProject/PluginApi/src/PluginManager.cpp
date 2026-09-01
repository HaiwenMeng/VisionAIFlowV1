#include "visionaiflow/plugin_api/PluginManager.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QJsonObject>

namespace visionaiflow::plugin_api
{
namespace
{
bool ReadCapability(const QJsonObject &metadata, const QString &key, bool *value, QString *errorMessage)
{
    const QJsonValue jsonValue = metadata.value(key);
    if (!jsonValue.isBool())
    {
        *errorMessage = QString(u8"检测 Qt 插件 capability 字段无效: %1").arg(key);
        return false;
    }

    *value = jsonValue.toBool();
    return true;
}

bool CapabilitiesMatch(const DetectionPluginCapabilities &metadataCapabilities,
                       const DetectionPluginCapabilities &pluginCapabilities)
{
    return metadataCapabilities.supportsResume == pluginCapabilities.supportsResume &&
           metadataCapabilities.supportsPretrained == pluginCapabilities.supportsPretrained &&
           metadataCapabilities.supportsExport == pluginCapabilities.supportsExport &&
           metadataCapabilities.supportsBackboneExport == pluginCapabilities.supportsBackboneExport &&
           metadataCapabilities.supportsFp16 == pluginCapabilities.supportsFp16 &&
           metadataCapabilities.supportsMultiGpu == pluginCapabilities.supportsMultiGpu;
}

bool UnloadPluginLoader(QPluginLoader *pluginLoader, QString *errorMessage)
{
    if (pluginLoader->unload())
    {
        delete pluginLoader;
        return true;
    }

    *errorMessage = QString(u8"无法卸载检测 Qt 插件 %1: %2").arg(pluginLoader->fileName(), pluginLoader->errorString());
    return false;
}

bool ReadDetectionPluginMetadata(const QString &filePath,
                                 DetectionPluginMetadata *pluginMetadata,
                                 QString *errorMessage)
{
    QPluginLoader pluginLoader(filePath);
    const QJsonObject pluginDocument = pluginLoader.metaData();
    if (pluginDocument.value(QStringLiteral("IID")).toString() != QStringLiteral(VISIONAIFLOW_DETECTION_PLUGIN_IID))
    {
        *errorMessage = QString(u8"不是兼容的检测 Qt 插件: %1").arg(filePath);
        return false;
    }

    const QJsonObject metadata = pluginDocument.value(QStringLiteral("MetaData")).toObject();
    const QString pluginId = metadata.value(QStringLiteral("plugin_id")).toString();
    const QString displayName = metadata.value(QStringLiteral("display_name")).toString();
    const QString version = metadata.value(QStringLiteral("version")).toString();
    if (pluginId.isEmpty() || displayName.isEmpty() || version.isEmpty() ||
        metadata.value(QStringLiteral("task_type")).toString() != QStringLiteral("detection"))
    {
        *errorMessage = QString(u8"检测 Qt 插件元数据不完整: %1").arg(filePath);
        return false;
    }

    DetectionPluginCapabilities capabilities;
    QString capabilityErrorMessage;
    if (!ReadCapability(metadata,
                        QStringLiteral("supports_resume"),
                        &capabilities.supportsResume,
                        &capabilityErrorMessage) ||
        !ReadCapability(metadata,
                        QStringLiteral("supports_pretrained"),
                        &capabilities.supportsPretrained,
                        &capabilityErrorMessage) ||
        !ReadCapability(metadata,
                        QStringLiteral("supports_export"),
                        &capabilities.supportsExport,
                        &capabilityErrorMessage) ||
        !ReadCapability(metadata,
                        QStringLiteral("supports_backbone_export"),
                        &capabilities.supportsBackboneExport,
                        &capabilityErrorMessage) ||
        !ReadCapability(metadata,
                        QStringLiteral("supports_fp16"),
                        &capabilities.supportsFp16,
                        &capabilityErrorMessage) ||
        !ReadCapability(metadata,
                        QStringLiteral("supports_multi_gpu"),
                        &capabilities.supportsMultiGpu,
                        &capabilityErrorMessage))
    {
        *errorMessage = QString(u8"检测 Qt 插件元数据无效: %1, %2").arg(filePath, capabilityErrorMessage);
        return false;
    }

    pluginMetadata->filePath = filePath;
    pluginMetadata->info = {pluginId, displayName, version, TrainTaskType::Detection};
    pluginMetadata->capabilities = capabilities;
    return true;
}
} // namespace

PluginManager::PluginManager() = default;

PluginManager::~PluginManager()
{
    for (QPluginLoader *pluginLoader : m_loadedPluginLoaders)
    {
        IDetectionPlugin *plugin = m_loadedPlugins.value(pluginLoader->fileName(), nullptr);
        if (plugin != nullptr && (plugin->state() == TrainState::Running || plugin->state() == TrainState::Stopping))
        {
            if (!plugin->stop())
            {
                qCritical().noquote() << QString(u8"停止检测 Qt 插件失败, 不卸载 DLL: %1, %2")
                                             .arg(pluginLoader->fileName(), plugin->errorMessage());
                continue;
            }
        }

        if (plugin != nullptr && !plugin->waitForStopped(5000))
        {
            qCritical().noquote() << QString(u8"等待检测 Qt 插件停止超时, 不卸载 DLL: %1, %2")
                                         .arg(pluginLoader->fileName(), plugin->errorMessage());
            continue;
        }

        QString unloadErrorMessage;
        if (!UnloadPluginLoader(pluginLoader, &unloadErrorMessage))
        {
            qCritical().noquote() << unloadErrorMessage;
        }
    }
}

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
            errorMessages->append(QString(u8"检测插件目录不存在: %1").arg(directoryPath));
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
    const QString absoluteFilePath = QFileInfo(filePath).absoluteFilePath();
    if (!QFileInfo::exists(absoluteFilePath))
    {
        m_errorMessage = QString(u8"检测插件 DLL 不存在: %1").arg(absoluteFilePath);
        return false;
    }

    DetectionPluginMetadata pluginMetadata;
    if (!ReadDetectionPluginMetadata(absoluteFilePath, &pluginMetadata, &m_errorMessage))
    {
        return false;
    }

    const auto loadedPlugin = m_loadedPlugins.constFind(absoluteFilePath);
    if (loadedPlugin != m_loadedPlugins.cend())
    {
        m_detectionPlugin = loadedPlugin.value();
        m_errorMessage.clear();
        return true;
    }

    auto *pluginLoader = new QPluginLoader(absoluteFilePath);
    QObject *pluginObject = pluginLoader->instance();
    if (pluginObject == nullptr)
    {
        m_errorMessage = QString(u8"无法加载检测 Qt 插件 %1: %2").arg(absoluteFilePath, pluginLoader->errorString());
        delete pluginLoader;
        return false;
    }

    auto *plugin = qobject_cast<IDetectionPlugin *>(pluginObject);
    if (plugin == nullptr)
    {
        m_errorMessage = QString(u8"检测 Qt 插件未实现 IDetectionPlugin: %1").arg(absoluteFilePath);
        QString unloadErrorMessage;
        if (!UnloadPluginLoader(pluginLoader, &unloadErrorMessage))
        {
            m_errorMessage += QString(u8"; %1").arg(unloadErrorMessage);
        }
        return false;
    }
    if (plugin->pluginInfo().taskType != TrainTaskType::Detection)
    {
        m_errorMessage = QString(u8"检测 Qt 插件任务类型不匹配: %1").arg(absoluteFilePath);
        QString unloadErrorMessage;
        if (!UnloadPluginLoader(pluginLoader, &unloadErrorMessage))
        {
            m_errorMessage += QString(u8"; %1").arg(unloadErrorMessage);
        }
        return false;
    }

    if (!CapabilitiesMatch(pluginMetadata.capabilities, plugin->capabilities()))
    {
        m_errorMessage = QString(u8"检测 Qt 插件元数据 capability 与实现不一致: %1").arg(absoluteFilePath);
        QString unloadErrorMessage;
        if (!UnloadPluginLoader(pluginLoader, &unloadErrorMessage))
        {
            m_errorMessage += QString(u8"; %1").arg(unloadErrorMessage);
        }
        return false;
    }

    m_detectionPlugin = plugin;
    m_loadedPlugins.insert(absoluteFilePath, plugin);
    m_loadedPluginLoaders.append(pluginLoader);
    m_errorMessage.clear();
    return true;
}

IDetectionPlugin *PluginManager::detectionPlugin() const
{
    return m_detectionPlugin;
}

QString PluginManager::errorMessage() const
{
    return m_errorMessage;
}
} // namespace visionaiflow::plugin_api
