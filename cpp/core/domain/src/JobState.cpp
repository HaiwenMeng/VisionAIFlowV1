#include "visionaiflow/domain/JobState.h"

namespace visionaiflow::domain
{
bool IsTerminal(const JobState state) noexcept
{
    return state == JobState::Cancelled || state == JobState::Completed || state == JobState::Failed;
}

const char *ToString(const JobState state) noexcept
{
    switch (state)
    {
    case JobState::Created: return "created";
    case JobState::Validating: return "validating";
    case JobState::Queued: return "queued";
    case JobState::Starting: return "starting";
    case JobState::Running: return "running";
    case JobState::Pausing: return "pausing";
    case JobState::Paused: return "paused";
    case JobState::Cancelling: return "cancelling";
    case JobState::Cancelled: return "cancelled";
    case JobState::Completing: return "completing";
    case JobState::Completed: return "completed";
    case JobState::Failed: return "failed";
    }
    return "unknown";
}

foundation::Result<void> ValidateTransition(const JobState from, const JobState to)
{
    const bool allowed =
        (from == JobState::Created && to == JobState::Validating) ||
        (from == JobState::Validating && (to == JobState::Queued || to == JobState::Failed)) ||
        (from == JobState::Queued && (to == JobState::Starting || to == JobState::Cancelling)) ||
        (from == JobState::Starting && (to == JobState::Running || to == JobState::Failed || to == JobState::Cancelling)) ||
        (from == JobState::Running && (to == JobState::Pausing || to == JobState::Cancelling || to == JobState::Completing || to == JobState::Failed)) ||
        (from == JobState::Pausing && (to == JobState::Paused || to == JobState::Failed)) ||
        (from == JobState::Paused && (to == JobState::Running || to == JobState::Cancelling || to == JobState::Failed)) ||
        (from == JobState::Cancelling && (to == JobState::Cancelled || to == JobState::Failed)) ||
        (from == JobState::Completing && (to == JobState::Completed || to == JobState::Failed));
    if (!allowed)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(
            foundation::ErrorCode::InvalidState,
            std::string("Invalid job state transition from ") + ToString(from) + " to " + ToString(to)));
    }
    return foundation::Result<void>::Success();
}
}
