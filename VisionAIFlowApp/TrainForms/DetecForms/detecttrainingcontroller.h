#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <memory>

class QTimer;

namespace visionaiflow::plugin_api
{
class PluginManager;
}

struct DetectTrainingRequest
{
    QString taskName;
    QString pluginPath;
    QString modelVariant;
    int epochs{0};
    int batchSize{0};
    double learningRate{0.0};
    bool horizontalFlip{true};
    QString resumeCheckpointPath;
};

struct DetectModelExportRequest
{
    QString pluginPath;
    QString checkpointPath;
    QString outputPath;
    int imageWidth{0};
    int imageHeight{0};
    int batchSize{1};
    QVariantMap metadata;
};

struct DetectionPluginDescriptor
{
    QString filePath;
    QString id;
    QString displayName;
    QString version;
    bool supportsExport{false};
};

class DetectTrainingController final : public QObject
{
    Q_OBJECT

public:
    explicit DetectTrainingController(QObject *parent = nullptr);
    ~DetectTrainingController() override;

    void Start(const DetectTrainingRequest &request);
    void Cancel();
    bool IsRunning() const noexcept;
    bool ExportModel(const DetectModelExportRequest &request, QString *errorMessage);
    QVector<DetectionPluginDescriptor> DiscoverPlugins(QStringList *errorMessages) const;

signals:
    void
    EpochProgress(int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double meanIou);
    void Completed(const QString &runDirectory,
                   const QString &modelPath,
                   const QString &bestCheckpointPath,
                   const QString &durationMessage);
    void Failed(const QString &errorMessage);
    void Cancelled();
    void StateChanged(bool running);

private:
    void PollPluginState();

    std::unique_ptr<visionaiflow::plugin_api::PluginManager> m_pluginManager;
    QTimer *m_pollTimer{nullptr};
    QElapsedTimer m_elapsedTimer;
    QString m_outputDirectory;
    int m_lastReportedEpoch{0};
    int m_totalEpochs{0};
};
