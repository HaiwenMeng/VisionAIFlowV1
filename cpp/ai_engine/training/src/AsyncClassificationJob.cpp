#include "visionaiflow/training/AsyncClassificationJob.h"

#include <cmath>
#include <exception>

namespace visionaiflow::training
{
AsyncClassificationJob::AsyncClassificationJob(QObject *parent) : QObject(parent)
{
}

foundation::Result<void> AsyncClassificationJob::Start(const AsyncClassificationJobConfig &config, const torch::Tensor &features, const torch::Tensor &targets)
{
    if (m_running) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot start a classification job that is already running"));
    if (config.inputFeatures <= 0 || config.classCount < 2 || config.totalSteps <= 0 || config.learningRate <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Async classification job configuration is invalid"));
    if (config.schedulerStepSize < 0 || !std::isfinite(config.schedulerGamma) || config.schedulerGamma <= 0.0 || (config.schedulerStepSize == 0 && config.schedulerGamma != 1.0)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Async classification scheduler configuration is invalid"));
    if (!features.defined() || !targets.defined()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Async classification job tensors must be defined"));
    try
    {
        const auto model = CreateLinearClassifier(config.inputFeatures, config.classCount);
        if (!model.IsSuccess()) return foundation::Result<void>::Failure(model.Failure());
        m_model = model.Value();
        m_model->to(config.device);
        m_features = features.to(config.device);
        m_targets = targets.to(config.device);
        m_optimizer = std::make_unique<torch::optim::SGD>(m_model->parameters(), torch::optim::SGDOptions(config.learningRate));
        m_checkpointPath = config.checkpointPath;
        m_samplerSeed = config.samplerSeed;
        m_captureCudaRng = config.device.is_cuda() && !m_checkpointPath.isEmpty();
        m_captureCudaRngStates = config.captureCudaRngStates;
        m_restoreCudaRngStates = config.restoreCudaRngStates;
        if (m_captureCudaRng && !m_captureCudaRngStates) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "CUDA checkpointing requires a CUDA RNG capture handler"));
        m_schedulerState = {LearningRateSchedulerKind::None, config.learningRate, config.learningRate, 0, 1.0, 0};
        if (config.schedulerStepSize > 0) m_schedulerState = {LearningRateSchedulerKind::Step, config.learningRate, config.learningRate, config.schedulerStepSize, config.schedulerGamma, 0};
        m_completedSteps = 0;
        m_totalSteps = config.totalSteps;
        if (!config.resumeCheckpointPath.isEmpty())
        {
            TrainingCheckpointState restoredState;
            TrainingCheckpointLoadOptions loadOptions;
            loadOptions.restoreCpuRng = true;
            loadOptions.restoreCudaRngStates = m_restoreCudaRngStates;
            const auto loaded = LoadTrainingCheckpoint(config.resumeCheckpointPath, m_model, *m_optimizer, config.device, restoredState, loadOptions);
            if (!loaded.IsSuccess()) return loaded;
            if (restoredState.step < 0 || restoredState.step >= config.totalSteps) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Resume checkpoint step is outside the requested training range"));
            m_completedSteps = static_cast<int>(restoredState.step);
            m_samplerSeed = restoredState.samplerSeed;
            m_schedulerState = restoredState.schedulerState;
            const auto schedulerApplied = ApplySchedulerLearningRate();
            if (!schedulerApplied.IsSuccess()) return schedulerApplied;
        }
        m_cancelRequested = false;
        m_running = true;
        QMetaObject::invokeMethod(this, &AsyncClassificationJob::ExecuteOneStep, Qt::QueuedConnection);
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("Async classification job initialization failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Async classification job initialization failed: ") + error.what())); }
}

foundation::Result<void> AsyncClassificationJob::RequestCancel()
{
    if (!m_running) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot cancel a classification job that is not running"));
    m_cancelRequested = true;
    return foundation::Result<void>::Success();
}

bool AsyncClassificationJob::IsRunning() const noexcept { return m_running; }

void AsyncClassificationJob::ExecuteOneStep()
{
    if (!m_running) return;
    if (m_cancelRequested)
    {
        const auto checkpointed = PersistCheckpoint();
        if (!checkpointed.IsSuccess())
        {
            Fail(checkpointed.Failure());
            return;
        }
        m_running = false;
        emit Cancelled();
        return;
    }
    const auto result = TrainClassificationStep(m_model, *m_optimizer, m_features, m_targets);
    if (!result.IsSuccess())
    {
        Fail(result.Failure());
        return;
    }
    ++m_completedSteps;
    const auto schedulerAdvanced = AdvanceScheduler();
    if (!schedulerAdvanced.IsSuccess())
    {
        Fail(schedulerAdvanced.Failure());
        return;
    }
    const auto checkpointed = PersistCheckpoint();
    if (!checkpointed.IsSuccess())
    {
        Fail(checkpointed.Failure());
        return;
    }
    emit Progress(m_completedSteps, m_totalSteps, result.Value());
    if (m_completedSteps >= m_totalSteps)
    {
        m_running = false;
        emit Completed(result.Value());
        return;
    }
    QMetaObject::invokeMethod(this, &AsyncClassificationJob::ExecuteOneStep, Qt::QueuedConnection);
}

void AsyncClassificationJob::Fail(const foundation::Error &error)
{
    m_running = false;
    emit Failed(QString::fromLatin1(foundation::ToString(error.code)), QString::fromStdString(error.message));
}

foundation::Result<void> AsyncClassificationJob::PersistCheckpoint()
{
    if (m_checkpointPath.isEmpty()) return foundation::Result<void>::Success();
    if (!m_model || !m_optimizer) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot persist checkpoint before the training job is initialized"));
    TrainingCheckpointState state;
    state.epoch = 0;
    state.step = m_completedSteps;
    state.samplerSeed = m_samplerSeed;
    state.samplerEpoch = 0;
    state.schedulerState = m_schedulerState;
    state.ampState.mode = PrecisionMode::Fp32;
    state.ampState.scale = 1.0;
    state.ampState.consecutiveFiniteSteps = m_completedSteps;
    state.captureCpuRng = true;
    state.captureCudaRng = m_captureCudaRng;
    if (m_captureCudaRng)
    {
        if (!m_captureCudaRngStates) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot persist CUDA checkpoint without a CUDA RNG capture handler"));
        const auto cudaStates = m_captureCudaRngStates();
        if (!cudaStates.IsSuccess()) return foundation::Result<void>::Failure(cudaStates.Failure());
        state.cudaRngDeviceCount = static_cast<int64_t>(cudaStates.Value().size());
        state.cudaRngStates = cudaStates.Value();
    }
    return SaveTrainingCheckpoint(m_checkpointPath, m_model, *m_optimizer, state);
}

foundation::Result<void> AsyncClassificationJob::ApplySchedulerLearningRate()
{
    if (!m_optimizer) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot apply scheduler before optimizer initialization"));
    if (!std::isfinite(m_schedulerState.currentLearningRate) || m_schedulerState.currentLearningRate <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Scheduler current learning rate is invalid"));
    try
    {
        for (torch::optim::OptimizerParamGroup &group : m_optimizer->param_groups())
        {
            static_cast<torch::optim::SGDOptions &>(group.options()).lr(m_schedulerState.currentLearningRate);
        }
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch scheduler learning-rate apply failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Scheduler learning-rate apply failed: ") + error.what())); }
}

foundation::Result<void> AsyncClassificationJob::AdvanceScheduler()
{
    if (m_schedulerState.kind == LearningRateSchedulerKind::None)
    {
        m_schedulerState.lastStep = m_completedSteps;
        return ApplySchedulerLearningRate();
    }
    if (m_schedulerState.stepSize <= 0 || !std::isfinite(m_schedulerState.gamma) || m_schedulerState.gamma <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "StepLR scheduler state is invalid"));
    ++m_schedulerState.lastStep;
    if (m_schedulerState.lastStep % m_schedulerState.stepSize == 0) m_schedulerState.currentLearningRate *= m_schedulerState.gamma;
    return ApplySchedulerLearningRate();
}
}
