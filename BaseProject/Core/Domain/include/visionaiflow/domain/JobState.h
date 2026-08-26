#pragma once

#include "visionaiflow/foundation/Result.h"

namespace visionaiflow::domain
{
#if defined(VISIONAIFLOW_DOMAIN_LIBRARY)
#define VISIONAIFLOW_DOMAIN_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_DOMAIN_EXPORT __declspec(dllimport)
#endif

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

[[nodiscard]] VISIONAIFLOW_DOMAIN_EXPORT bool IsTerminal(JobState state) noexcept;
[[nodiscard]] VISIONAIFLOW_DOMAIN_EXPORT const char *ToString(JobState state) noexcept;
VISIONAIFLOW_DOMAIN_EXPORT foundation::Result<void> ValidateTransition(JobState from, JobState to);
}
