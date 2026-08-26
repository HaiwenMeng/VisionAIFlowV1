#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/training/AmpController.h"

#include <QJsonObject>

#include <cstdint>
#include <functional>
#include <vector>

#if defined(VISIONAIFLOW_TRAINING_STATE_LIBRARY)
#define VISIONAIFLOW_TRAINING_STATE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_TRAINING_STATE_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::training
{
enum class LearningRateSchedulerKind
{
    None,
    Step
};

struct LearningRateSchedulerState final
{
    LearningRateSchedulerKind kind{LearningRateSchedulerKind::None};
    double baseLearningRate{0.0};
    double currentLearningRate{0.0};
    int64_t stepSize{0};
    double gamma{1.0};
    int64_t lastStep{0};
};

struct TrainingCheckpointState final
{
    int64_t epoch{0};
    int64_t step{0};
    uint64_t samplerSeed{0};
    int64_t samplerEpoch{0};
    LearningRateSchedulerState schedulerState;
    AmpState ampState;
    bool captureCpuRng{true};
    bool captureCudaRng{false};
    int64_t cudaRngDeviceCount{0};
    std::vector<torch::Tensor> cudaRngStates;
};

struct TrainingCheckpointLoadOptions final
{
    bool restoreCpuRng{true};
    std::function<foundation::Result<void>(const std::vector<torch::Tensor> &states)> restoreCudaRngStates;
};

VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<void> ValidateTrainingCheckpointState(const TrainingCheckpointState &state);
VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<QJsonObject> TrainingCheckpointStateToJson(const TrainingCheckpointState &state);
VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<TrainingCheckpointState> TrainingCheckpointStateFromJson(const QJsonObject &object);
VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<void> ValidateTrainingCheckpointStateMatch(const TrainingCheckpointState &manifestState, const TrainingCheckpointState &archiveState);
VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<void> WriteTrainingCheckpointStateArchive(torch::serialize::OutputArchive &root, const TrainingCheckpointState &state);
VISIONAIFLOW_TRAINING_STATE_EXPORT foundation::Result<TrainingCheckpointState> ReadTrainingCheckpointStateArchive(torch::serialize::InputArchive &root, bool restoreCpuRng);
}
