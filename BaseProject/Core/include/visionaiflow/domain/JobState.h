#pragma once

#include "visionaiflow/foundation/Result.h"

namespace visionaiflow::domain
{
enum class JobState
{
    Created,
    Validating,
    Queued,
    Starting,
    Running,
    Pausing,
    Paused,
    Cancelling,
    Cancelled,
    Completing,
    Completed,
    Failed
};

[[nodiscard]] VISIONAIFLOW_CORE_EXPORT bool IsTerminal(JobState state) noexcept;
[[nodiscard]] VISIONAIFLOW_CORE_EXPORT const char *ToString(JobState state) noexcept;
VISIONAIFLOW_CORE_EXPORT foundation::Result<void> ValidateTransition(JobState from, JobState to);
} // namespace visionaiflow::domain
