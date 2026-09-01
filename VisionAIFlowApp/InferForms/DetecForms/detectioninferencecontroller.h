#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace visionaiflow::plugin_api
{
class PluginManager;
struct DetectionInferResult;
} // namespace visionaiflow::plugin_api

struct DetectionInferencePluginDescriptor final
{
    QString filePath;
    QString id;
    QString displayName;
    QString version;
};

struct DetectionInferenceConfig final
{
    QString pluginPath;
    QString modelPath;
    int gpuId{0};
    int imageWidth{640};
    int imageHeight{640};
    bool useFp16{false};
    double confidenceThreshold{0.5};
    double nmsThreshold{0.5};
};

class DetectionInferenceController final : public QObject
{
    Q_OBJECT

public:
    explicit DetectionInferenceController(QObject *parent = nullptr);
    ~DetectionInferenceController() override;

    QVector<DetectionInferencePluginDescriptor> DiscoverPlugins(QStringList *errorMessages) const;
    bool LoadModel(const DetectionInferenceConfig &config, QString *errorMessage);
    bool InferImage(const QString &imagePath, QString *errorMessage);
    bool InferImages(const QStringList &imagePaths, int *completedImageCount, QString *errorMessage);
    bool InferDirectory(const QString &directoryPath,
                        bool includeSubdirectories,
                        int *completedImageCount,
                        QString *errorMessage);

private:
    bool InferImageAndWriteResult(const QString &imagePath, QString *errorMessage);
    bool WriteResultFile(const QString &imagePath,
                         const visionaiflow::plugin_api::DetectionInferResult &result,
                         QString *errorMessage) const;

    std::unique_ptr<visionaiflow::plugin_api::PluginManager> m_pluginManager;
    DetectionInferenceConfig m_config;
    bool m_modelLoaded{false};
};
