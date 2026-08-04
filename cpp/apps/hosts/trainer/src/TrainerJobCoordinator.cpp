#include "visionaiflow/trainer_host/TrainerJobCoordinator.h"
#include "visionaiflow/trainer_host/CudaRngCheckpoint.h"
#include "visionaiflow/trainer_host/CudaRuntimeProbe.h"

#include <QCborArray>

#include <vector>

namespace visionaiflow::trainer_host
{
TrainerJobCoordinator::TrainerJobCoordinator(QObject *parent) : QObject(parent) {}

foundation::Result<void> TrainerJobCoordinator::Handle(const QCborMap &request, const qt_foundation::HostAsyncResponder &responder)
{
    const QString type = request.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("cancel"))
    {
        if (!m_job || !m_job->IsRunning() || request.value(QStringLiteral("jobId")).toString() != m_jobId) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "No matching running training job can be cancelled"));
        const auto cancelled = m_job->RequestCancel();
        if (!cancelled.IsSuccess()) return cancelled;
        QCborMap state;
        state.insert(QStringLiteral("state"), QStringLiteral("cancelling"));
        return responder(QStringLiteral("progress"), state);
    }
    if (type != QStringLiteral("execute") || request.value(QStringLiteral("operation")).toString() != QStringLiteral("trainSingleLabel")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Trainer operation is unsupported"));
    return StartSingleLabelJob(request, responder);
}

foundation::Result<void> TrainerJobCoordinator::StartSingleLabelJob(const QCborMap &request, const qt_foundation::HostAsyncResponder &responder)
{
    if (m_job && m_job->IsRunning()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "A training job is already running"));
    ReleaseGpuLeaseAfterTerminalEvent();
    const QCborValue inputFeaturesValue = request.value(QStringLiteral("inputFeatures"));
    const QCborValue classCountValue = request.value(QStringLiteral("classCount"));
    const QCborValue totalStepsValue = request.value(QStringLiteral("totalSteps"));
    const QCborValue learningRateValue = request.value(QStringLiteral("learningRate"));
    if (!inputFeaturesValue.isInteger() || !classCountValue.isInteger() || !totalStepsValue.isInteger() || (!learningRateValue.isDouble() && !learningRateValue.isInteger())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training request configuration fields are invalid"));
    const int64_t inputFeatures = inputFeaturesValue.toInteger();
    const int64_t classCount = classCountValue.toInteger();
    const int totalSteps = static_cast<int>(totalStepsValue.toInteger());
    const double learningRate = learningRateValue.toDouble();
    const QCborArray featureRows = request.value(QStringLiteral("features")).toArray();
    const QCborArray targetValues = request.value(QStringLiteral("targets")).toArray();
    if (inputFeatures <= 0 || classCount < 2 || totalSteps <= 0 || learningRate <= 0.0 || featureRows.isEmpty() || targetValues.size() != featureRows.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training request data dimensions are invalid"));
    std::vector<float> features;
    features.reserve(static_cast<size_t>(featureRows.size()) * static_cast<size_t>(inputFeatures));
    std::vector<int64_t> targets;
    targets.reserve(static_cast<size_t>(targetValues.size()));
    for (int rowIndex = 0; rowIndex < featureRows.size(); ++rowIndex)
    {
        const QCborArray row = featureRows.at(rowIndex).toArray();
        if (row.size() != inputFeatures || !targetValues.at(rowIndex).isInteger()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training feature rows or targets do not match the declared contract"));
        const int64_t target = targetValues.at(rowIndex).toInteger();
        if (target < 0 || target >= classCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training target is outside the declared class range"));
        targets.push_back(target);
        for (const QCborValue &value : row)
        {
            if (!value.isDouble() && !value.isInteger()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training features must be numeric"));
            features.push_back(static_cast<float>(value.toDouble()));
        }
    }
    try
    {
        const torch::Tensor featureTensor = torch::from_blob(features.data(), {featureRows.size(), inputFeatures}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
        const torch::Tensor targetTensor = torch::from_blob(targets.data(), {targetValues.size()}, torch::TensorOptions().dtype(torch::kInt64)).clone();
        const auto device = QueryCudaDeviceInfo(0);
        if (!device.IsSuccess()) return foundation::Result<void>::Failure(device.Failure());
        m_gpuLease = std::make_unique<tensor::GpuLease>();
        const auto acquired = m_gpuLease->Acquire(QString::fromStdString(device.Value().uuid));
        if (!acquired.IsSuccess())
        {
            m_gpuLease.reset();
            return acquired;
        }
        m_job = std::make_unique<training::AsyncClassificationJob>(this);
        m_jobId = request.value(QStringLiteral("jobId")).toString();
        connect(m_job.get(), &training::AsyncClassificationJob::Progress, this, [responder](const int completedSteps, const int total, const training::TrainingMetrics &metrics) {
            QCborMap payload;
            payload.insert(QStringLiteral("state"), QStringLiteral("running"));
            payload.insert(QStringLiteral("completedSteps"), completedSteps);
            payload.insert(QStringLiteral("totalSteps"), total);
            payload.insert(QStringLiteral("loss"), metrics.loss);
            payload.insert(QStringLiteral("accuracy"), metrics.accuracy);
            responder(QStringLiteral("progress"), payload);
        });
        connect(m_job.get(), &training::AsyncClassificationJob::Completed, this, [this, responder](const training::TrainingMetrics &metrics) { ReleaseGpuLeaseAfterTerminalEvent(); QCborMap payload; payload.insert(QStringLiteral("state"), QStringLiteral("completed")); payload.insert(QStringLiteral("loss"), metrics.loss); payload.insert(QStringLiteral("accuracy"), metrics.accuracy); responder(QStringLiteral("completed"), payload); });
        connect(m_job.get(), &training::AsyncClassificationJob::Cancelled, this, [this, responder]() { ReleaseGpuLeaseAfterTerminalEvent(); QCborMap payload; payload.insert(QStringLiteral("state"), QStringLiteral("cancelled")); responder(QStringLiteral("cancelled"), payload); });
        connect(m_job.get(), &training::AsyncClassificationJob::Failed, this, [this, responder](const QString &code, const QString &message) { ReleaseGpuLeaseAfterTerminalEvent(); QCborMap payload; payload.insert(QStringLiteral("errorCode"), code); payload.insert(QStringLiteral("errorMessage"), message); responder(QStringLiteral("failed"), payload); });
        training::AsyncClassificationJobConfig config;
        config.inputFeatures = inputFeatures;
        config.classCount = classCount;
        config.totalSteps = totalSteps;
        config.learningRate = learningRate;
        config.device = torch::Device(torch::kCUDA, 0);
        config.checkpointPath = request.value(QStringLiteral("checkpointPath")).toString();
        config.resumeCheckpointPath = request.value(QStringLiteral("resumeCheckpointPath")).toString();
        const QCborValue schedulerStepSizeValue = request.value(QStringLiteral("schedulerStepSize"));
        const QCborValue schedulerGammaValue = request.value(QStringLiteral("schedulerGamma"));
        if (schedulerStepSizeValue.isInteger()) config.schedulerStepSize = schedulerStepSizeValue.toInteger();
        if (schedulerGammaValue.isDouble() || schedulerGammaValue.isInteger()) config.schedulerGamma = schedulerGammaValue.toDouble();
        const QCborValue samplerSeedValue = request.value(QStringLiteral("samplerSeed"));
        if (samplerSeedValue.isInteger())
        {
            const qint64 samplerSeed = samplerSeedValue.toInteger();
            if (samplerSeed < 0)
            {
                ReleaseGpuLeaseAfterTerminalEvent();
                return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Training samplerSeed must not be negative"));
            }
            config.samplerSeed = static_cast<uint64_t>(samplerSeed);
        }
        config.captureCudaRngStates = []() { return CaptureCudaRngStates(); };
        config.restoreCudaRngStates = [](const std::vector<torch::Tensor> &states) { return RestoreCudaRngStates(states); };
        const auto started = m_job->Start(config, featureTensor, targetTensor);
        if (!started.IsSuccess()) ReleaseGpuLeaseAfterTerminalEvent();
        return started;
    }
    catch (const c10::Error &error) { ReleaseGpuLeaseAfterTerminalEvent(); return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("CUDA training job creation failed: ") + error.what())); }
}

void TrainerJobCoordinator::ReleaseGpuLeaseAfterTerminalEvent()
{
    if (!m_gpuLease) return;
    const auto released = m_gpuLease->Release();
    if (!released.IsSuccess()) qCritical("Trainer GPU lease release failed: %s", released.Failure().message.c_str());
    m_gpuLease.reset();
}
}
