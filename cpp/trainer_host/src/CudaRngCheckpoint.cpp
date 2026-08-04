#include "visionaiflow/trainer_host/CudaRngCheckpoint.h"

#include <ATen/cuda/CUDAGeneratorImpl.h>
#include <cuda_runtime_api.h>

#include <string>

namespace visionaiflow::trainer_host
{
namespace
{
foundation::Result<int> AvailableCudaDeviceCount()
{
    int deviceCount = 0;
    const cudaError_t status = cudaGetDeviceCount(&deviceCount);
    if (status != cudaSuccess) return foundation::Result<int>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("cudaGetDeviceCount failed while handling CUDA RNG state: ") + cudaGetErrorString(status)));
    if (deviceCount <= 0) return foundation::Result<int>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "No CUDA device is available for CUDA RNG checkpointing"));
    return foundation::Result<int>::Success(deviceCount);
}

foundation::Result<void> ValidateCudaRngStateTensor(const torch::Tensor &state, const int deviceIndex)
{
    if (!state.defined() || state.scalar_type() != torch::kUInt8 || !state.device().is_cpu() || state.numel() <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, std::string("CUDA RNG state tensor is invalid for device ") + std::to_string(deviceIndex)));
    return foundation::Result<void>::Success();
}
}

foundation::Result<std::vector<torch::Tensor>> CaptureCudaRngStates()
{
    try
    {
        const auto deviceCount = AvailableCudaDeviceCount();
        if (!deviceCount.IsSuccess()) return foundation::Result<std::vector<torch::Tensor>>::Failure(deviceCount.Failure());
        std::vector<torch::Tensor> states;
        states.reserve(static_cast<size_t>(deviceCount.Value()));
        for (int deviceIndex = 0; deviceIndex < deviceCount.Value(); ++deviceIndex)
        {
            const at::Generator &generator = at::cuda::detail::getDefaultCUDAGenerator(static_cast<at::DeviceIndex>(deviceIndex));
            torch::Tensor state = generator.get_state().to(torch::kCPU).contiguous();
            const auto validState = ValidateCudaRngStateTensor(state, deviceIndex);
            if (!validState.IsSuccess()) return foundation::Result<std::vector<torch::Tensor>>::Failure(validState.Failure());
            states.push_back(state);
        }
        return foundation::Result<std::vector<torch::Tensor>>::Success(states);
    }
    catch (const c10::Error &error) { return foundation::Result<std::vector<torch::Tensor>>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch CUDA RNG capture failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<std::vector<torch::Tensor>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("CUDA RNG capture failed: ") + error.what())); }
}

foundation::Result<void> RestoreCudaRngStates(const std::vector<torch::Tensor> &states)
{
    try
    {
        const auto deviceCount = AvailableCudaDeviceCount();
        if (!deviceCount.IsSuccess()) return foundation::Result<void>::Failure(deviceCount.Failure());
        if (states.size() != static_cast<size_t>(deviceCount.Value())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Checkpoint CUDA RNG device count does not match the available CUDA device count"));
        for (int deviceIndex = 0; deviceIndex < deviceCount.Value(); ++deviceIndex)
        {
            const auto validState = ValidateCudaRngStateTensor(states[static_cast<size_t>(deviceIndex)], deviceIndex);
            if (!validState.IsSuccess()) return validState;
            const at::Generator &generatorRef = at::cuda::detail::getDefaultCUDAGenerator(static_cast<at::DeviceIndex>(deviceIndex));
            at::Generator generator = generatorRef;
            generator.set_state(states[static_cast<size_t>(deviceIndex)]);
        }
        return foundation::Result<void>::Success();
    }
    catch (const c10::Error &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch CUDA RNG restore failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("CUDA RNG restore failed: ") + error.what())); }
}
}
