#include "visionaiflow/training/TrainingCheckpointState.h"

#include <ATen/CPUGeneratorImpl.h>

#include <QJsonValue>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace visionaiflow::training
{
namespace
{
QString PrecisionModeToString(const PrecisionMode mode)
{
    return mode == PrecisionMode::AmpFp16 ? QStringLiteral("amp_fp16") : QStringLiteral("fp32");
}

foundation::Result<PrecisionMode> PrecisionModeFromString(const QString &value)
{
    if (value == QStringLiteral("fp32")) return foundation::Result<PrecisionMode>::Success(PrecisionMode::Fp32);
    if (value == QStringLiteral("amp_fp16")) return foundation::Result<PrecisionMode>::Success(PrecisionMode::AmpFp16);
    return foundation::Result<PrecisionMode>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint AMP precision mode is invalid"));
}

QString SchedulerKindToString(const LearningRateSchedulerKind kind)
{
    return kind == LearningRateSchedulerKind::Step ? QStringLiteral("step") : QStringLiteral("none");
}

foundation::Result<LearningRateSchedulerKind> SchedulerKindFromString(const QString &value)
{
    if (value == QStringLiteral("none")) return foundation::Result<LearningRateSchedulerKind>::Success(LearningRateSchedulerKind::None);
    if (value == QStringLiteral("step")) return foundation::Result<LearningRateSchedulerKind>::Success(LearningRateSchedulerKind::Step);
    return foundation::Result<LearningRateSchedulerKind>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint scheduler kind is invalid"));
}

foundation::Result<int64_t> ReadInt64Tensor(torch::serialize::InputArchive &archive, const char *key)
{
    torch::Tensor value;
    archive.read(key, value);
    if (!value.defined() || value.numel() != 1 || value.scalar_type() != torch::kInt64) return foundation::Result<int64_t>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("Checkpoint training state field is not an int64 tensor: ") + key));
    return foundation::Result<int64_t>::Success(value.to(torch::kCPU).item<int64_t>());
}

foundation::Result<double> ReadFloat64Tensor(torch::serialize::InputArchive &archive, const char *key)
{
    torch::Tensor value;
    archive.read(key, value);
    if (!value.defined() || value.numel() != 1 || value.scalar_type() != torch::kFloat64) return foundation::Result<double>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("Checkpoint training state field is not a float64 tensor: ") + key));
    return foundation::Result<double>::Success(value.to(torch::kCPU).item<double>());
}

torch::Tensor Int64Tensor(const int64_t value)
{
    return torch::tensor({value}, torch::TensorOptions().dtype(torch::kInt64));
}

torch::Tensor Float64Tensor(const double value)
{
    return torch::tensor({value}, torch::TensorOptions().dtype(torch::kFloat64));
}

foundation::Result<uint64_t> SamplerSeedFromJson(const QJsonObject &object)
{
    const QJsonValue value = object.value(QStringLiteral("samplerSeed"));
    if (value.isString())
    {
        bool ok = false;
        const quint64 seed = value.toString().toULongLong(&ok);
        if (ok) return foundation::Result<uint64_t>::Success(static_cast<uint64_t>(seed));
    }
    return foundation::Result<uint64_t>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state samplerSeed must be a decimal string"));
}

foundation::Result<void> RestoreCpuRngState(torch::serialize::InputArchive &stateArchive)
{
    torch::Tensor cpuRngState;
    stateArchive.read("cpuRngState", cpuRngState);
    if (!cpuRngState.defined() || cpuRngState.scalar_type() != torch::kUInt8 || !cpuRngState.device().is_cpu()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint CPU RNG state is invalid"));
    at::Generator generator = at::detail::getDefaultCPUGenerator();
    generator.set_state(cpuRngState);
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateCudaRngTensor(const torch::Tensor &state, const int64_t deviceIndex)
{
    if (!state.defined() || state.scalar_type() != torch::kUInt8 || !state.device().is_cpu() || state.numel() <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string("Checkpoint CUDA RNG state tensor is invalid for device ") + std::to_string(deviceIndex)));
    return foundation::Result<void>::Success();
}
}

foundation::Result<void> ValidateTrainingCheckpointState(const TrainingCheckpointState &state)
{
    if (state.epoch < 0 || state.step < 0 || state.samplerEpoch < 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint training state counters must be non-negative"));
    if (state.samplerSeed > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint samplerSeed exceeds the supported archive range"));
    if (!std::isfinite(state.schedulerState.baseLearningRate) || !std::isfinite(state.schedulerState.currentLearningRate) || state.schedulerState.baseLearningRate < 0.0 || state.schedulerState.currentLearningRate < 0.0 || state.schedulerState.lastStep < 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint scheduler state is invalid"));
    if (state.schedulerState.kind == LearningRateSchedulerKind::None)
    {
        if (state.schedulerState.stepSize != 0 || state.schedulerState.gamma != 1.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint disabled scheduler state must use neutral parameters"));
    }
    else
    {
        if (state.schedulerState.stepSize <= 0 || !std::isfinite(state.schedulerState.gamma) || state.schedulerState.gamma <= 0.0 || state.schedulerState.baseLearningRate <= 0.0 || state.schedulerState.currentLearningRate <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint StepLR scheduler state is invalid"));
    }
    if (!std::isfinite(state.ampState.scale) || state.ampState.scale <= 0.0 || state.ampState.consecutiveFiniteSteps < 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint AMP state is invalid"));
    if (state.ampState.mode == PrecisionMode::Fp32 && state.ampState.scale != 1.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint FP32 AMP state must use scale one"));
    if (!state.captureCudaRng)
    {
        if (state.cudaRngDeviceCount != 0 || !state.cudaRngStates.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint disabled CUDA RNG state must use an empty contract"));
    }
    else
    {
        if (state.cudaRngDeviceCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint CUDA RNG device count must be positive when captured"));
        if (!state.cudaRngStates.empty() && static_cast<int64_t>(state.cudaRngStates.size()) != state.cudaRngDeviceCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint CUDA RNG tensor count does not match the device count"));
        for (size_t index = 0; index < state.cudaRngStates.size(); ++index)
        {
            const auto validTensor = ValidateCudaRngTensor(state.cudaRngStates[index], static_cast<int64_t>(index));
            if (!validTensor.IsSuccess()) return validTensor;
        }
    }
    return foundation::Result<void>::Success();
}

foundation::Result<QJsonObject> TrainingCheckpointStateToJson(const TrainingCheckpointState &state)
{
    const auto validation = ValidateTrainingCheckpointState(state);
    if (!validation.IsSuccess()) return foundation::Result<QJsonObject>::Failure(validation.Failure());
    const QJsonObject ampObject{
        {QStringLiteral("mode"), PrecisionModeToString(state.ampState.mode)},
        {QStringLiteral("scale"), state.ampState.scale},
        {QStringLiteral("consecutiveFiniteSteps"), state.ampState.consecutiveFiniteSteps}};
    const QJsonObject schedulerObject{
        {QStringLiteral("kind"), SchedulerKindToString(state.schedulerState.kind)},
        {QStringLiteral("baseLearningRate"), state.schedulerState.baseLearningRate},
        {QStringLiteral("currentLearningRate"), state.schedulerState.currentLearningRate},
        {QStringLiteral("stepSize"), state.schedulerState.stepSize},
        {QStringLiteral("gamma"), state.schedulerState.gamma},
        {QStringLiteral("lastStep"), state.schedulerState.lastStep}};
    const QJsonObject rngObject{
        {QStringLiteral("cpuCaptured"), state.captureCpuRng},
        {QStringLiteral("cpuArchiveKey"), QStringLiteral("trainingState/cpuRngState")},
        {QStringLiteral("cudaCaptured"), state.captureCudaRng},
        {QStringLiteral("cudaDeviceCount"), static_cast<double>(state.cudaRngDeviceCount)},
        {QStringLiteral("cudaArchivePrefix"), QStringLiteral("trainingState/cudaRngState")}};
    return foundation::Result<QJsonObject>::Success(QJsonObject{
        {QStringLiteral("epoch"), static_cast<double>(state.epoch)},
        {QStringLiteral("step"), static_cast<double>(state.step)},
        {QStringLiteral("samplerSeed"), QString::number(state.samplerSeed)},
        {QStringLiteral("samplerEpoch"), static_cast<double>(state.samplerEpoch)},
        {QStringLiteral("scheduler"), schedulerObject},
        {QStringLiteral("amp"), ampObject},
        {QStringLiteral("rng"), rngObject}});
}

foundation::Result<TrainingCheckpointState> TrainingCheckpointStateFromJson(const QJsonObject &object)
{
    TrainingCheckpointState state;
    const double epoch = object.value(QStringLiteral("epoch")).toDouble(-1.0);
    const double step = object.value(QStringLiteral("step")).toDouble(-1.0);
    const double samplerEpoch = object.value(QStringLiteral("samplerEpoch")).toDouble(-1.0);
    if (!std::isfinite(epoch) || !std::isfinite(step) || !std::isfinite(samplerEpoch) || std::floor(epoch) != epoch || std::floor(step) != step || std::floor(samplerEpoch) != samplerEpoch) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state counters must be finite integers"));
    state.epoch = static_cast<int64_t>(epoch);
    state.step = static_cast<int64_t>(step);
    state.samplerEpoch = static_cast<int64_t>(samplerEpoch);
    const auto samplerSeed = SamplerSeedFromJson(object);
    if (!samplerSeed.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(samplerSeed.Failure());
    state.samplerSeed = samplerSeed.Value();
    const QJsonObject schedulerObject = object.value(QStringLiteral("scheduler")).toObject();
    const auto schedulerKind = SchedulerKindFromString(schedulerObject.value(QStringLiteral("kind")).toString());
    if (!schedulerKind.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerKind.Failure());
    state.schedulerState.kind = schedulerKind.Value();
    state.schedulerState.baseLearningRate = schedulerObject.value(QStringLiteral("baseLearningRate")).toDouble(-1.0);
    state.schedulerState.currentLearningRate = schedulerObject.value(QStringLiteral("currentLearningRate")).toDouble(-1.0);
    state.schedulerState.stepSize = static_cast<int64_t>(schedulerObject.value(QStringLiteral("stepSize")).toDouble(-1.0));
    state.schedulerState.gamma = schedulerObject.value(QStringLiteral("gamma")).toDouble(-1.0);
    state.schedulerState.lastStep = static_cast<int64_t>(schedulerObject.value(QStringLiteral("lastStep")).toDouble(-1.0));
    const QJsonObject ampObject = object.value(QStringLiteral("amp")).toObject();
    const auto mode = PrecisionModeFromString(ampObject.value(QStringLiteral("mode")).toString());
    if (!mode.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(mode.Failure());
    state.ampState.mode = mode.Value();
    state.ampState.scale = ampObject.value(QStringLiteral("scale")).toDouble(-1.0);
    state.ampState.consecutiveFiniteSteps = static_cast<int64_t>(ampObject.value(QStringLiteral("consecutiveFiniteSteps")).toDouble(-1.0));
    const QJsonObject rngObject = object.value(QStringLiteral("rng")).toObject();
    if (!rngObject.value(QStringLiteral("cpuCaptured")).isBool()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state rng.cpuCaptured must be a boolean"));
    state.captureCpuRng = rngObject.value(QStringLiteral("cpuCaptured")).toBool();
    if (!rngObject.value(QStringLiteral("cudaCaptured")).isBool()) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state rng.cudaCaptured must be a boolean"));
    state.captureCudaRng = rngObject.value(QStringLiteral("cudaCaptured")).toBool();
    const double cudaDeviceCount = rngObject.value(QStringLiteral("cudaDeviceCount")).toDouble(-1.0);
    if (!std::isfinite(cudaDeviceCount) || std::floor(cudaDeviceCount) != cudaDeviceCount) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state rng.cudaDeviceCount must be a finite integer"));
    state.cudaRngDeviceCount = static_cast<int64_t>(cudaDeviceCount);
    const auto validation = ValidateTrainingCheckpointState(state);
    if (!validation.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(validation.Failure());
    return foundation::Result<TrainingCheckpointState>::Success(state);
}

foundation::Result<void> ValidateTrainingCheckpointStateMatch(const TrainingCheckpointState &manifestState, const TrainingCheckpointState &archiveState)
{
    if (manifestState.epoch != archiveState.epoch || manifestState.step != archiveState.step || manifestState.samplerSeed != archiveState.samplerSeed || manifestState.samplerEpoch != archiveState.samplerEpoch) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state archive does not match manifest counters"));
    if (manifestState.schedulerState.kind != archiveState.schedulerState.kind || manifestState.schedulerState.baseLearningRate != archiveState.schedulerState.baseLearningRate || manifestState.schedulerState.currentLearningRate != archiveState.schedulerState.currentLearningRate || manifestState.schedulerState.stepSize != archiveState.schedulerState.stepSize || manifestState.schedulerState.gamma != archiveState.schedulerState.gamma || manifestState.schedulerState.lastStep != archiveState.schedulerState.lastStep) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state archive does not match manifest scheduler state"));
    if (manifestState.ampState.mode != archiveState.ampState.mode || manifestState.ampState.scale != archiveState.ampState.scale || manifestState.ampState.consecutiveFiniteSteps != archiveState.ampState.consecutiveFiniteSteps) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state archive does not match manifest AMP state"));
    if (manifestState.captureCpuRng != archiveState.captureCpuRng || manifestState.captureCudaRng != archiveState.captureCudaRng || manifestState.cudaRngDeviceCount != archiveState.cudaRngDeviceCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint training state archive does not match manifest RNG contract"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> WriteTrainingCheckpointStateArchive(torch::serialize::OutputArchive &root, const TrainingCheckpointState &state)
{
    const auto validation = ValidateTrainingCheckpointState(state);
    if (!validation.IsSuccess()) return validation;
    try
    {
        torch::serialize::OutputArchive stateArchive;
        stateArchive.write("epoch", Int64Tensor(state.epoch));
        stateArchive.write("step", Int64Tensor(state.step));
        stateArchive.write("samplerSeed", Int64Tensor(static_cast<int64_t>(state.samplerSeed)));
        stateArchive.write("samplerEpoch", Int64Tensor(state.samplerEpoch));
        stateArchive.write("schedulerKind", Int64Tensor(state.schedulerState.kind == LearningRateSchedulerKind::Step ? 1 : 0));
        stateArchive.write("schedulerBaseLearningRate", Float64Tensor(state.schedulerState.baseLearningRate));
        stateArchive.write("schedulerCurrentLearningRate", Float64Tensor(state.schedulerState.currentLearningRate));
        stateArchive.write("schedulerStepSize", Int64Tensor(state.schedulerState.stepSize));
        stateArchive.write("schedulerGamma", Float64Tensor(state.schedulerState.gamma));
        stateArchive.write("schedulerLastStep", Int64Tensor(state.schedulerState.lastStep));
        stateArchive.write("ampMode", Int64Tensor(state.ampState.mode == PrecisionMode::AmpFp16 ? 1 : 0));
        stateArchive.write("ampScale", Float64Tensor(state.ampState.scale));
        stateArchive.write("ampConsecutiveFiniteSteps", Int64Tensor(state.ampState.consecutiveFiniteSteps));
        stateArchive.write("cpuRngCaptured", Int64Tensor(state.captureCpuRng ? 1 : 0));
        if (state.captureCpuRng) stateArchive.write("cpuRngState", at::detail::getDefaultCPUGenerator().get_state());
        stateArchive.write("cudaRngCaptured", Int64Tensor(state.captureCudaRng ? 1 : 0));
        stateArchive.write("cudaRngDeviceCount", Int64Tensor(state.cudaRngDeviceCount));
        if (state.captureCudaRng)
        {
            if (static_cast<int64_t>(state.cudaRngStates.size()) != state.cudaRngDeviceCount) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Checkpoint CUDA RNG tensors are required before saving"));
            for (int64_t deviceIndex = 0; deviceIndex < state.cudaRngDeviceCount; ++deviceIndex)
            {
                const auto validTensor = ValidateCudaRngTensor(state.cudaRngStates[static_cast<size_t>(deviceIndex)], deviceIndex);
                if (!validTensor.IsSuccess()) return validTensor;
                stateArchive.write(std::string("cudaRngState") + std::to_string(deviceIndex), state.cudaRngStates[static_cast<size_t>(deviceIndex)]);
            }
        }
        root.write("trainingState", stateArchive);
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch checkpoint state save failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("Checkpoint state save failed: ") + error.what())); }
}

foundation::Result<TrainingCheckpointState> ReadTrainingCheckpointStateArchive(torch::serialize::InputArchive &root, const bool restoreCpuRng)
{
    try
    {
        torch::serialize::InputArchive stateArchive;
        root.read("trainingState", stateArchive);
        TrainingCheckpointState state;
        const auto epoch = ReadInt64Tensor(stateArchive, "epoch");
        const auto step = ReadInt64Tensor(stateArchive, "step");
        const auto samplerSeed = ReadInt64Tensor(stateArchive, "samplerSeed");
        const auto samplerEpoch = ReadInt64Tensor(stateArchive, "samplerEpoch");
        const auto schedulerKind = ReadInt64Tensor(stateArchive, "schedulerKind");
        const auto schedulerBaseLearningRate = ReadFloat64Tensor(stateArchive, "schedulerBaseLearningRate");
        const auto schedulerCurrentLearningRate = ReadFloat64Tensor(stateArchive, "schedulerCurrentLearningRate");
        const auto schedulerStepSize = ReadInt64Tensor(stateArchive, "schedulerStepSize");
        const auto schedulerGamma = ReadFloat64Tensor(stateArchive, "schedulerGamma");
        const auto schedulerLastStep = ReadInt64Tensor(stateArchive, "schedulerLastStep");
        const auto ampMode = ReadInt64Tensor(stateArchive, "ampMode");
        const auto ampScale = ReadFloat64Tensor(stateArchive, "ampScale");
        const auto ampSteps = ReadInt64Tensor(stateArchive, "ampConsecutiveFiniteSteps");
        const auto cpuRngCaptured = ReadInt64Tensor(stateArchive, "cpuRngCaptured");
        const auto cudaRngCaptured = ReadInt64Tensor(stateArchive, "cudaRngCaptured");
        const auto cudaRngDeviceCount = ReadInt64Tensor(stateArchive, "cudaRngDeviceCount");
        if (!epoch.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(epoch.Failure());
        if (!step.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(step.Failure());
        if (!samplerSeed.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(samplerSeed.Failure());
        if (!samplerEpoch.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(samplerEpoch.Failure());
        if (!schedulerKind.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerKind.Failure());
        if (!schedulerBaseLearningRate.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerBaseLearningRate.Failure());
        if (!schedulerCurrentLearningRate.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerCurrentLearningRate.Failure());
        if (!schedulerStepSize.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerStepSize.Failure());
        if (!schedulerGamma.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerGamma.Failure());
        if (!schedulerLastStep.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(schedulerLastStep.Failure());
        if (!ampMode.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(ampMode.Failure());
        if (!ampScale.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(ampScale.Failure());
        if (!ampSteps.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(ampSteps.Failure());
        if (!cpuRngCaptured.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(cpuRngCaptured.Failure());
        if (!cudaRngCaptured.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(cudaRngCaptured.Failure());
        if (!cudaRngDeviceCount.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(cudaRngDeviceCount.Failure());
        if (ampMode.Value() != 0 && ampMode.Value() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint AMP mode archive value is invalid"));
        if (schedulerKind.Value() != 0 && schedulerKind.Value() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint scheduler kind archive value is invalid"));
        if (cpuRngCaptured.Value() != 0 && cpuRngCaptured.Value() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint CPU RNG captured archive value is invalid"));
        if (cudaRngCaptured.Value() != 0 && cudaRngCaptured.Value() != 1) return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint CUDA RNG captured archive value is invalid"));
        state.epoch = epoch.Value();
        state.step = step.Value();
        state.samplerSeed = static_cast<uint64_t>(samplerSeed.Value());
        state.samplerEpoch = samplerEpoch.Value();
        state.schedulerState.kind = schedulerKind.Value() == 1 ? LearningRateSchedulerKind::Step : LearningRateSchedulerKind::None;
        state.schedulerState.baseLearningRate = schedulerBaseLearningRate.Value();
        state.schedulerState.currentLearningRate = schedulerCurrentLearningRate.Value();
        state.schedulerState.stepSize = schedulerStepSize.Value();
        state.schedulerState.gamma = schedulerGamma.Value();
        state.schedulerState.lastStep = schedulerLastStep.Value();
        state.ampState.mode = ampMode.Value() == 1 ? PrecisionMode::AmpFp16 : PrecisionMode::Fp32;
        state.ampState.scale = ampScale.Value();
        state.ampState.consecutiveFiniteSteps = ampSteps.Value();
        state.captureCpuRng = cpuRngCaptured.Value() == 1;
        state.captureCudaRng = cudaRngCaptured.Value() == 1;
        state.cudaRngDeviceCount = cudaRngDeviceCount.Value();
        if (state.captureCudaRng)
        {
            for (int64_t deviceIndex = 0; deviceIndex < state.cudaRngDeviceCount; ++deviceIndex)
            {
                torch::Tensor cudaRngState;
                stateArchive.read(std::string("cudaRngState") + std::to_string(deviceIndex), cudaRngState);
                const auto validTensor = ValidateCudaRngTensor(cudaRngState, deviceIndex);
                if (!validTensor.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(validTensor.Failure());
                state.cudaRngStates.push_back(cudaRngState);
            }
        }
        const auto validation = ValidateTrainingCheckpointState(state);
        if (!validation.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(validation.Failure());
        if (restoreCpuRng && state.captureCpuRng)
        {
            const auto restored = RestoreCpuRngState(stateArchive);
            if (!restored.IsSuccess()) return foundation::Result<TrainingCheckpointState>::Failure(restored.Failure());
        }
        return foundation::Result<TrainingCheckpointState>::Success(state);
    }
    catch (const c10::Error &error) { return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("LibTorch checkpoint state load failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<TrainingCheckpointState>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("Checkpoint state load failed: ") + error.what())); }
}
}
