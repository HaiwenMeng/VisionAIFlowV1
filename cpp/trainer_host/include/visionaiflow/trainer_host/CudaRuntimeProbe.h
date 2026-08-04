#pragma once

#include "visionaiflow/foundation/Result.h"

#include <cstddef>
#include <string>

namespace visionaiflow::trainer_host
{
struct CudaDeviceInfo final
{
    int index{0};
    int runtimeVersion{0};
    int driverVersion{0};
    int computeCapabilityMajor{0};
    int computeCapabilityMinor{0};
    size_t totalMemoryBytes{0};
    size_t freeMemoryBytes{0};
    std::string name;
    std::string uuid;
};

foundation::Result<int> VerifyCudaRuntime();
foundation::Result<CudaDeviceInfo> QueryCudaDeviceInfo(int deviceIndex);
}
