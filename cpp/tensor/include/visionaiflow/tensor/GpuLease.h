#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

namespace visionaiflow::tensor
{
class GpuLease final
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
