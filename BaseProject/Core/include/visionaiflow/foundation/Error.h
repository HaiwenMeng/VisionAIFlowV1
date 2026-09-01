#pragma once

#include <map>
#include <string>

#if defined(VISIONAIFLOW_CORE_LIBRARY)
#define VISIONAIFLOW_CORE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_CORE_EXPORT __declspec(dllimport)
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

struct VISIONAIFLOW_CORE_EXPORT Error
{
    ErrorCode code;
    std::string message;
    std::map<std::string, std::string> context;

    static Error Create(ErrorCode code, std::string message);
};

VISIONAIFLOW_CORE_EXPORT const char *ToString(ErrorCode code) noexcept;
} // namespace visionaiflow::foundation
