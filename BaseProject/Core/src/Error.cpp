#include "visionaiflow/foundation/Error.h"

#include <stdexcept>

namespace visionaiflow::foundation
{
Error Error::Create(const ErrorCode code, std::string message)
{
    if (message.empty())
    {
        throw std::invalid_argument("Error message must not be empty");
    }
    return Error{code, std::move(message), {}};
}

const char *ToString(const ErrorCode code) noexcept
{
    switch (code)
    {
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    case ErrorCode::InvalidState:
        return "InvalidState";
    case ErrorCode::ProtocolViolation:
        return "ProtocolViolation";
    case ErrorCode::ProtocolVersionMismatch:
        return "ProtocolVersionMismatch";
    case ErrorCode::MessageTooLarge:
        return "MessageTooLarge";
    case ErrorCode::DuplicateRequest:
        return "DuplicateRequest";
    case ErrorCode::ConnectionFailure:
        return "ConnectionFailure";
    case ErrorCode::Timeout:
        return "Timeout";
    case ErrorCode::IoFailure:
        return "IoFailure";
    case ErrorCode::DependencyMissing:
        return "DependencyMissing";
    case ErrorCode::DependencyVersionMismatch:
        return "DependencyVersionMismatch";
    case ErrorCode::ProcessFailure:
        return "ProcessFailure";
    case ErrorCode::UnsupportedOperation:
        return "UnsupportedOperation";
    case ErrorCode::InternalFailure:
        return "InternalFailure";
    }
    return "Unknown";
}
} // namespace visionaiflow::foundation
