#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/training/TrainingCheckpointState.h"

#ifdef slots
#pragma push_macro("slots")
#undef slots
#define VAF_RESTORE_QT_SLOTS_MACRO
#endif
#include <torch/torch.h>
#ifdef VAF_RESTORE_QT_SLOTS_MACRO
#pragma pop_macro("slots")
#undef VAF_RESTORE_QT_SLOTS_MACRO
#endif

#include <QStringList>

#include <cstdint>

class QString;

namespace visionaiflow::training
{
class LinearClassifierImpl final : public torch::nn::Module
{
public:
    LinearClassifierImpl(int64_t inputFeatures, int64_t classCount);
    torch::Tensor forward(const torch::Tensor &features);
    torch::Tensor Weight() const;
    torch::Tensor Bias() const;

private:
    torch::nn::Linear m_linear{nullptr};
};

TORCH_MODULE(LinearClassifier);

struct TrainingMetrics final
{
    double loss{0.0};
    double accuracy{0.0};
    int64_t sampleCount{0};
};

foundation::Result<LinearClassifier> CreateLinearClassifier(int64_t inputFeatures, int64_t classCount);
QStringList LinearClassifierParameterNames();
foundation::Result<TrainingMetrics> TrainClassificationStep(LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Tensor &features, const torch::Tensor &targets);
foundation::Result<TrainingMetrics> EvaluateClassificationBatch(LinearClassifier &model, const torch::Tensor &features, const torch::Tensor &targets);
foundation::Result<TrainingMetrics> TrainMultiLabelClassificationStep(LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Tensor &features, const torch::Tensor &targets);
foundation::Result<TrainingMetrics> EvaluateMultiLabelClassificationBatch(LinearClassifier &model, const torch::Tensor &features, const torch::Tensor &targets);
foundation::Result<void> SaveTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer);
foundation::Result<void> SaveTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state);
foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device);
foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state);
foundation::Result<void> LoadTrainingCheckpoint(const QString &path, LinearClassifier &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options);
}
