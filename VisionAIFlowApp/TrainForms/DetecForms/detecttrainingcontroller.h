#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
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
    QVector<DetectionPluginDescriptor> DiscoverPlugins(QStringList *errorMessages) const;

signals:
    void
    EpochProgress(int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double meanIou);
    void Completed(const QString &runDirectory, const QString &modelPath, const QString &bestCheckpointPath);
    void Failed(const QString &errorMessage);
    void Cancelled();
    void StateChanged(bool running);

private:
    void PollPluginState();

    std::unique_ptr<visionaiflow::plugin_api::PluginManager> m_pluginManager;
    QTimer *m_pollTimer{nullptr};
    QString m_outputDirectory;
    int m_lastReportedEpoch{0};
};
