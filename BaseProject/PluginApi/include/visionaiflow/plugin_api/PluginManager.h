#pragma once

#include "visionaiflow/plugin_api/PluginApi.h"

#include <QHash>
#include <QPluginLoader>

namespace visionaiflow::plugin_api
{
struct DetectionPluginMetadata final
{
    QString filePath;
    PluginInfo info;
    DetectionPluginCapabilities capabilities;
};

class VISIONAIFLOW_PLUGIN_API_EXPORT PluginManager final
{
public:
    PluginManager();
    ~PluginManager();

    QVector<DetectionPluginMetadata> scanDetectionPluginMetadata(const QString &directoryPath,
                                                                 QStringList *errorMessages) const;
    bool loadDetectionPlugin(const QString &filePath);
    IDetectionPlugin *detectionPlugin() const;
    QString errorMessage() const;

private:
    QVector<QPluginLoader *> m_loadedPluginLoaders;
    QHash<QString, IDetectionPlugin *> m_loadedPlugins;
    IDetectionPlugin *m_detectionPlugin{nullptr};
    QString m_errorMessage;
};
} // namespace visionaiflow::plugin_api
