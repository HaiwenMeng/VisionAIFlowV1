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
    DetectionTrainerCapabilities capabilities;
};

class VISIONAIFLOW_PLUGIN_API_EXPORT PluginManager final
{
public:
    PluginManager();
    ~PluginManager();

    QVector<DetectionPluginMetadata> scanDetectionPluginMetadata(const QString &directoryPath,
                                                                 QStringList *errorMessages) const;
    bool loadDetectionPlugin(const QString &filePath);
    IDetectionTrainer *detectionTrainer() const;
    QString errorMessage() const;

private:
    QVector<QPluginLoader *> m_loadedPluginLoaders;
    QHash<QString, IDetectionTrainer *> m_loadedTrainers;
    IDetectionTrainer *m_detectionTrainer{nullptr};
    QString m_errorMessage;
};
} // namespace visionaiflow::plugin_api
