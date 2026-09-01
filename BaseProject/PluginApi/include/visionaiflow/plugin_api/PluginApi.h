#pragma once

#include <QString>
#include <QStringList>
#include <QtPlugin>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#if defined(VISIONAIFLOW_PLUGIN_API_LIBRARY)
#define VISIONAIFLOW_PLUGIN_API_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_PLUGIN_API_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::plugin_api
{
enum class TrainTaskType
{
    Detection,
    Classification,
    Segmentation
};

enum class TrainState
{
    Idle,
    Initialized,
    Running,
    Stopping,
    Completed,
    Failed
};

struct PluginInfo final
{
    QString id;
    QString displayName;
    QString version;
    TrainTaskType taskType{TrainTaskType::Detection};
};

struct TrainProgress final
{
    int epoch{0};
    int step{0};
    int totalEpochs{0};
    int totalSteps{0};
    double loss{0.0};
    double learningRate{0.0};
    QString message;
};

struct PluginCapabilities final
{
    bool supportsResume{false};
    bool supportsPretrained{false};
    bool supportsExport{false};
    bool supportsFp16{false};
    bool supportsMultiGpu{false};
};

struct PluginParameterDefinition final
{
    QString key;
    QString displayName;
    QVariant defaultValue;
    QVariant minimumValue;
    QVariant maximumValue;
    QString category;
};

class ITrainPlugin
{
public:
    virtual ~ITrainPlugin() = default;

    virtual PluginInfo pluginInfo() const = 0;
    virtual TrainState state() const = 0;
    virtual QString errorMessage() const = 0;
    virtual bool stop() = 0;
    virtual bool waitForStopped(int timeoutMs) = 0;
};

struct DetectionTrainConfig final
{
    QString datasetPath;
    QString outputPath;
    QStringList classNames;
    int epochs{0};
    int batchSize{0};
    int imageWidth{0};
    int imageHeight{0};
    double learningRate{0.0};
    int gpuId{0};
    int numWorkers{0};
    bool useFp16{false};
    QString resumeCheckpointPath;
    QString pretrainedPath;
    QVariantMap algorithmOptions;
};

struct DetectionTrainProgress final
{
    TrainProgress train;
    double boxLoss{0.0};
    double classLoss{0.0};
    double dflLoss{0.0};
    int positiveCount{0};
    double meanIou{0.0};
    QString modelPath;
    QString bestCheckpointPath;
};

struct DetectionMetrics final
{
    double precision{0.0};
    double recall{0.0};
    double meanAveragePrecision{0.0};
};

using DetectionTrainerCapabilities = PluginCapabilities;

class IDetectionTrainer : public ITrainPlugin
{
public:
    ~IDetectionTrainer() override = default;

    virtual bool initialize(const DetectionTrainConfig &config) = 0;
    virtual bool startTrain() = 0;
    virtual DetectionTrainProgress progress() const = 0;
    virtual DetectionMetrics metrics() const = 0;
    virtual DetectionTrainerCapabilities capabilities() const = 0;
    virtual QVector<PluginParameterDefinition> parameterDefinitions() const = 0;
    virtual bool exportModel(const QString &outputPath, const QString &format) = 0;
};

struct ClassificationTrainConfig final
{
    QString datasetPath;
    QString outputPath;
    int epochs{0};
    int batchSize{0};
    double learningRate{0.0};
    int gpuId{0};
    QVariantMap algorithmOptions;
};

struct ClassificationTrainProgress final
{
    TrainProgress train;
};

class IClassificationTrainer : public ITrainPlugin
{
public:
    ~IClassificationTrainer() override = default;

    virtual bool initialize(const ClassificationTrainConfig &config) = 0;
    virtual bool startTrain() = 0;
    virtual ClassificationTrainProgress progress() const = 0;
    virtual PluginCapabilities capabilities() const = 0;
    virtual QVector<PluginParameterDefinition> parameterDefinitions() const = 0;
};

struct SegmentationTrainConfig final
{
    QString datasetPath;
    QString outputPath;
    int epochs{0};
    int batchSize{0};
    double learningRate{0.0};
    int gpuId{0};
    QVariantMap algorithmOptions;
};

struct SegmentationTrainProgress final
{
    TrainProgress train;
};

class ISegmentationTrainer : public ITrainPlugin
{
public:
    ~ISegmentationTrainer() override = default;

    virtual bool initialize(const SegmentationTrainConfig &config) = 0;
    virtual bool startTrain() = 0;
    virtual SegmentationTrainProgress progress() const = 0;
    virtual PluginCapabilities capabilities() const = 0;
    virtual QVector<PluginParameterDefinition> parameterDefinitions() const = 0;
};

} // namespace visionaiflow::plugin_api

#define VISIONAIFLOW_DETECTION_TRAINER_IID "visionaiflow.plugin_api.IDetectionTrainer/1.0"
Q_DECLARE_INTERFACE(visionaiflow::plugin_api::IDetectionTrainer, VISIONAIFLOW_DETECTION_TRAINER_IID)
