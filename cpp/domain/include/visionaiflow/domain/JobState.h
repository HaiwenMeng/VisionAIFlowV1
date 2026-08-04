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

[[nodiscard]] bool IsTerminal(JobState state) noexcept;
[[nodiscard]] const char *ToString(JobState state) noexcept;
foundation::Result<void> ValidateTransition(JobState from, JobState to);
}
