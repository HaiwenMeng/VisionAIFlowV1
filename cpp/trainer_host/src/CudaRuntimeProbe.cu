#include "visionaiflow/trainer_host/CudaRuntimeProbe.h"

#include <cuda_runtime_api.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace visionaiflow::trainer_host
{
foundation::Result<int> VerifyCudaRuntime()
{
    int runtimeVersion = 0;
    const cudaError_t runtimeResult = cudaRuntimeGetVersion(&runtimeVersion);
    if (runtimeResult != cudaSuccess)
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(
            foundation::ErrorCode::DependencyMissing,
            std::string("cudaRuntimeGetVersion failed: ") + cudaGetErrorString(runtimeResult)));
    }

    int deviceCount = 0;
    const cudaError_t deviceResult = cudaGetDeviceCount(&deviceCount);
    if (deviceResult != cudaSuccess)
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(
            foundation::ErrorCode::DependencyMissing,
            std::string("cudaGetDeviceCount failed: ") + cudaGetErrorString(deviceResult)));
    }
    if (deviceCount < 1)
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(
            foundation::ErrorCode::DependencyMissing,
            "No CUDA device is available for the trainer host"));
    }
    const auto device = QueryCudaDeviceInfo(0);
    if (!device.IsSuccess()) return foundation::Result<int>::Failure(device.Failure());
    if (device.Value().computeCapabilityMajor < 7 || (device.Value().computeCapabilityMajor == 7 && device.Value().computeCapabilityMinor < 5))
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(
            foundation::ErrorCode::UnsupportedOperation,
            "CUDA device compute capability is below the required SM 7.5 baseline"));
    }
    return foundation::Result<int>::Success(runtimeVersion);
}

foundation::Result<CudaDeviceInfo> QueryCudaDeviceInfo(const int deviceIndex)
{
    if (deviceIndex < 0) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "CUDA device index must not be negative"));
    int deviceCount = 0;
    const cudaError_t countResult = cudaGetDeviceCount(&deviceCount);
    if (countResult != cudaSuccess) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("cudaGetDeviceCount failed: ") + cudaGetErrorString(countResult)));
    if (deviceIndex >= deviceCount) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "CUDA device index is outside the available device range"));
    const cudaError_t setResult = cudaSetDevice(deviceIndex);
    if (setResult != cudaSuccess) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("cudaSetDevice failed: ") + cudaGetErrorString(setResult)));
    cudaDeviceProp properties{};
    const cudaError_t propertyResult = cudaGetDeviceProperties(&properties, deviceIndex);
    if (propertyResult != cudaSuccess) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("cudaGetDeviceProperties failed: ") + cudaGetErrorString(propertyResult)));
    int runtimeVersion = 0;
    int driverVersion = 0;
    size_t freeMemory = 0;
    size_t totalMemory = 0;
    if (cudaRuntimeGetVersion(&runtimeVersion) != cudaSuccess || cudaDriverGetVersion(&driverVersion) != cudaSuccess || cudaMemGetInfo(&freeMemory, &totalMemory) != cudaSuccess) return foundation::Result<CudaDeviceInfo>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, "Unable to query CUDA runtime, driver, or memory information"));
    std::ostringstream uuidText;
    uuidText << std::hex << std::setfill('0');
    for (unsigned char byte : properties.uuid.bytes) uuidText << std::setw(2) << static_cast<unsigned int>(byte);
    return foundation::Result<CudaDeviceInfo>::Success({deviceIndex, runtimeVersion, driverVersion, properties.major, properties.minor, properties.totalGlobalMem, freeMemory, properties.name, uuidText.str()});
}
}
