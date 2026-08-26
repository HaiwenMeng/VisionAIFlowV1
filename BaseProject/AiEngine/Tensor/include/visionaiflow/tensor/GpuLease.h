#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

#if defined(VISIONAIFLOW_TENSOR_LIBRARY)
#define VISIONAIFLOW_TENSOR_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_TENSOR_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::tensor
{
class VISIONAIFLOW_TENSOR_EXPORT GpuLease final
{
public:
    GpuLease() = default;
    ~GpuLease();
    GpuLease(const GpuLease &) = delete;
    GpuLease &operator=(const GpuLease &) = delete;

    foundation::Result<void> Acquire(const QString &gpuUuid);
    foundation::Result<void> Release();
    bool IsHeld() const noexcept;

private:
    void *m_handle{nullptr};
    QString m_name;
};
}
