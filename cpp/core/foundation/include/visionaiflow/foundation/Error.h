#pragma once

#include <map>
#include <string>

namespace visionaiflow::foundation
{
enum class ErrorCode
{
    InvalidArgument,
    InvalidState,
    ProtocolViolation,
    ProtocolVersionMismatch,
    MessageTooLarge,
    DuplicateRequest,
    ConnectionFailure,
    Timeout,
    IoFailure,
    DependencyMissing,
    DependencyVersionMismatch,
    ProcessFailure,
    UnsupportedOperation,
    InternalFailure
};

struct Error
{
    ErrorCode code;
    std::string message;
    std::map<std::string, std::string> context;

    static Error Create(ErrorCode code, std::string message);
};

const char *ToString(ErrorCode code) noexcept;
}
