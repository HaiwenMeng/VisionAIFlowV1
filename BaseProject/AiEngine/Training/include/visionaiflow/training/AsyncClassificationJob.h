#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/training/LinearClassifierTrainer.h"

#include <QObject>
#include <QString>

#include <memory>
#include <functional>
#include <vector>

namespace visionaiflow::training
{
struct AsyncClassificationJobConfig final
{
    using ModelFactory = std::function<foundation::Result<LinearClassifier>(int64_t inputFeatures, int64_t classCount)>;

    int64_t inputFeatures{0};
    int64_t classCount{0};
    int totalSteps{0};
    double learningRate{0.0};
    torch::Device device{torch::kCPU};
    QString checkpointPath;
    QString resumeCheckpointPath;
    uint64_t samplerSeed{0};
    int64_t schedulerStepSize{0};
    double schedulerGamma{1.0};
    ModelFactory modelFactory;
    std::function<foundation::Result<std::vector<torch::Tensor>>()> captureCudaRngStates;
    std::function<foundation::Result<void>(const std::vector<torch::Tensor> &states)> restoreCudaRngStates;
};

class VISIONAIFLOW_LINEAR_EXPORT AsyncClassificationJob final : public QObject
{
    Q_OBJECT

public:
    explicit AsyncClassificationJob(QObject *parent = nullptr);
    foundation::Result<void> Start(const AsyncClassificationJobConfig &config, const torch::Tensor &features, const torch::Tensor &targets);
    foundation::Result<void> RequestCancel();
    [[nodiscard]] bool IsRunning() const noexcept;

signals:
    void Progress(int completedSteps, int totalSteps, const visionaiflow::training::TrainingMetrics &metrics);
    void Completed(const visionaiflow::training::TrainingMetrics &metrics);
    void Cancelled();
    void Failed(const QString &errorCode, const QString &errorMessage);

private slots:
    void ExecuteOneStep();

private:
    void Fail(const foundation::Error &error);
    foundation::Result<void> PersistCheckpoint();
    foundation::Result<void> ApplySchedulerLearningRate();
    foundation::Result<void> AdvanceScheduler();

    LinearClassifier m_model{nullptr};
    std::unique_ptr<torch::optim::SGD> m_optimizer;
    torch::Tensor m_features;
    torch::Tensor m_targets;
    QString m_checkpointPath;
    uint64_t m_samplerSeed{0};
    LearningRateSchedulerState m_schedulerState;
    bool m_captureCudaRng{false};
    std::function<foundation::Result<std::vector<torch::Tensor>>()> m_captureCudaRngStates;
    std::function<foundation::Result<void>(const std::vector<torch::Tensor> &states)> m_restoreCudaRngStates;
    int m_completedSteps{0};
    int m_totalSteps{0};
    bool m_running{false};
    bool m_cancelRequested{false};
};
}
