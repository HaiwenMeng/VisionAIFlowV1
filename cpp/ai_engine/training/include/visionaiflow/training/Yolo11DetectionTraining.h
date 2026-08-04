#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/common/DetectionPostProcessor.h"
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

#include <vector>

class QString;

namespace visionaiflow::training
{
class Yolo11TinyDetectorImpl final : public torch::nn::Module
{
public:
    Yolo11TinyDetectorImpl(int inputChannels, int rowCount, int classCount);
    torch::Tensor forward(const torch::Tensor &images);
    torch::Tensor Conv1Weight() const;
    torch::Tensor Conv1Bias() const;
    torch::Tensor Conv2Weight() const;
    torch::Tensor Conv2Bias() const;
    torch::Tensor HeadWeight() const;
    torch::Tensor HeadBias() const;
    int RowCount() const noexcept;
    int ClassCount() const noexcept;

private:
    int m_rowCount{0};
    int m_classCount{0};
    torch::nn::Conv2d m_conv1{nullptr};
    torch::nn::Conv2d m_conv2{nullptr};
    torch::nn::Linear m_head{nullptr};
};

TORCH_MODULE(Yolo11TinyDetector);

class Yolo11GridDetectorImpl final : public torch::nn::Module
{
public:
    Yolo11GridDetectorImpl(int inputChannels, int classCount);
    torch::Tensor forward(const torch::Tensor &images);
    int ClassCount() const noexcept;
    int OutputStride() const noexcept;

private:
    int m_classCount{0};
    torch::nn::Sequential m_backbone{nullptr};
    torch::nn::Sequential m_neck{nullptr};
    torch::nn::Conv2d m_head{nullptr};
};

TORCH_MODULE(Yolo11GridDetector);

struct Yolo11GroundTruthDetection final
{
    models::common::DetectionBox box;
    int classIndex{-1};
};

struct Yolo11AssignedTarget final
{
    bool positive{false};
    int groundTruthIndex{-1};
    int classIndex{-1};
    models::common::DetectionBox box;
};

struct Yolo11AssignmentConfig final
{
    float positiveIouThreshold{0.50F};
    bool forceBestGroundTruthMatch{true};
};

struct Yolo11DetectionLossConfig final
{
    double boxWeight{5.0};
    double classWeight{1.0};
};

struct Yolo11DetectionLossMetrics final
{
    torch::Tensor totalLoss;
    double boxLoss{0.0};
    double classLoss{0.0};
    int positiveRows{0};
    int assignedGroundTruthCount{0};
    double meanPositiveIou{0.0};
};

struct Yolo11DetectionBatch final
{
    torch::Tensor images;
    std::vector<std::vector<Yolo11AssignedTarget>> assignments;
};

struct Yolo11DetectionBatchMetrics final
{
    torch::Tensor totalLoss;
    double boxLoss{0.0};
    double classLoss{0.0};
    int positiveRows{0};
    int assignedGroundTruthCount{0};
    double meanPositiveIou{0.0};
    int sampleCount{0};
};

struct Yolo11AugmentedSample final
{
    torch::Tensor image;
    std::vector<Yolo11GroundTruthDetection> targets;
};

struct Yolo11DetectionMetricsConfig final
{
    float iouThreshold{0.50F};
    float scoreThreshold{0.0F};
};

struct Yolo11DetectionEvaluationSummary final
{
    int truePositive{0};
    int falsePositive{0};
    int falseNegative{0};
    double precision{0.0};
    double recall{0.0};
    double meanMatchedIou{0.0};
};

foundation::Result<std::vector<Yolo11AssignedTarget>> AssignYolo11DetectionTargets(const std::vector<models::common::DetectionBox> &candidateBoxes, const std::vector<Yolo11GroundTruthDetection> &groundTruth, int classCount, const Yolo11AssignmentConfig &config);
foundation::Result<Yolo11DetectionLossMetrics> ComputeYolo11DetectionLoss(const torch::Tensor &rawHead, const std::vector<Yolo11AssignedTarget> &assignments, int classCount, const Yolo11DetectionLossConfig &config);
foundation::Result<Yolo11TinyDetector> CreateYolo11TinyDetector(int inputChannels, int rowCount, int classCount);
foundation::Result<Yolo11GridDetector> CreateYolo11GridDetector(int inputChannels, int classCount);
QStringList Yolo11TinyDetectorParameterNames();
QStringList Yolo11GridDetectorParameterNames();
foundation::Result<Yolo11DetectionBatchMetrics> TrainYolo11TinyDetectionStep(Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const Yolo11DetectionBatch &batch, int classCount, const Yolo11DetectionLossConfig &config);
foundation::Result<Yolo11DetectionBatchMetrics> EvaluateYolo11TinyDetectionBatch(Yolo11TinyDetector &model, const Yolo11DetectionBatch &batch, int classCount, const Yolo11DetectionLossConfig &config);
foundation::Result<Yolo11DetectionBatchMetrics> TrainYolo11GridDetectionStep(Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const Yolo11DetectionBatch &batch, int classCount, const Yolo11DetectionLossConfig &config);
foundation::Result<Yolo11DetectionBatchMetrics> EvaluateYolo11GridDetectionBatch(Yolo11GridDetector &model, const Yolo11DetectionBatch &batch, int classCount, const Yolo11DetectionLossConfig &config);
foundation::Result<void> SaveYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer);
foundation::Result<void> SaveYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state);
foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device);
foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state);
foundation::Result<void> LoadYolo11TinyDetectorCheckpoint(const QString &path, Yolo11TinyDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options);
foundation::Result<void> SaveYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer);
foundation::Result<void> SaveYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const TrainingCheckpointState &state);
foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device);
foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state);
foundation::Result<void> LoadYolo11GridDetectorCheckpoint(const QString &path, Yolo11GridDetector &model, torch::optim::Optimizer &optimizer, const torch::Device &device, TrainingCheckpointState &state, const TrainingCheckpointLoadOptions &options);
foundation::Result<Yolo11AugmentedSample> FlipYolo11DetectionSampleHorizontally(const torch::Tensor &image, const std::vector<Yolo11GroundTruthDetection> &targets, int classCount, float imageWidth);
foundation::Result<Yolo11DetectionEvaluationSummary> EvaluateYolo11DetectionMetrics(const std::vector<models::common::Detection> &predictions, const std::vector<Yolo11GroundTruthDetection> &groundTruth, int classCount, const Yolo11DetectionMetricsConfig &config);
}
