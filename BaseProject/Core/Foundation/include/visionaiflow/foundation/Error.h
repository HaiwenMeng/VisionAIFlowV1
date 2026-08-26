#pragma once

#include <map>
#include <string>

#if defined(VISIONAIFLOW_FOUNDATION_LIBRARY)
#define VISIONAIFLOW_FOUNDATION_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_FOUNDATION_EXPORT __declspec(dllimport)
#endif

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

struct VISIONAIFLOW_FOUNDATION_EXPORT Error
{
    ErrorCode code;
    std::string message;
    std::map<std::string, std::string> context;

    static Error Create(ErrorCode code, std::string message);
};

VISIONAIFLOW_FOUNDATION_EXPORT const char *ToString(ErrorCode code) noexcept;
}
