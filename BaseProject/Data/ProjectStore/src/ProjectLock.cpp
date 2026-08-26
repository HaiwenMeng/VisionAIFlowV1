#include "visionaiflow/project_store/ProjectLock.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <windows.h>

namespace visionaiflow::project_store
{
namespace
{
QString LastErrorMessage(const QString &prefix)
{
    return prefix + QStringLiteral(" (Win32 error ") + QString::number(GetLastError()) + QLatin1Char(')');
}

quint64 FileTimeToUint64(const FILETIME &value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

foundation::Result<void> ValidateLockRecord(const QJsonObject &lock)
{
    const QStringList requiredKeys{QStringLiteral("pid"), QStringLiteral("processStartFileTime"), QStringLiteral("machine"), QStringLiteral("user"), QStringLiteral("productVersion")};
    for (const QString &key : requiredKeys)
    {
        if (!lock.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock is missing owner identity fields and must be inspected manually"));
    }
    for (auto iterator = lock.constBegin(); iterator != lock.constEnd(); ++iterator)
    {
        if (!requiredKeys.contains(iterator.key())) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock contains unsupported fields and must be inspected manually"));
    }
    if (!lock.value(QStringLiteral("pid")).isDouble() || lock.value(QStringLiteral("pid")).toInteger() <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock pid is invalid and must be inspected manually"));
    bool startTimeOk = false;
    const quint64 startTime = lock.value(QStringLiteral("processStartFileTime")).toString().toULongLong(&startTimeOk);
    if (!startTimeOk || startTime == 0U) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock process start time is invalid and must be inspected manually"));
    for (const QString &stringKey : {QStringLiteral("machine"), QStringLiteral("user"), QStringLiteral("productVersion")})
    {
        if (!lock.value(stringKey).isString() || lock.value(stringKey).toString().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock owner text fields are invalid and must be inspected manually"));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> RemoveStaleLockIfVerified(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Existing project lock cannot be read"));
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Existing project lock is malformed and must be inspected manually"));
    const QJsonObject lock = document.object();
    const auto lockValidation = ValidateLockRecord(lock);
    if (!lockValidation.IsSuccess()) return lockValidation;
    const DWORD pid = static_cast<DWORD>(lock.value(QStringLiteral("pid")).toInteger());
    const quint64 expectedStartTime = lock.value(QStringLiteral("processStartFileTime")).toString().toULongLong();
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    bool stale = process == nullptr;
    if (process != nullptr)
    {
        FILETIME creation{}, exitTime{}, kernel{}, user{};
        stale = !GetProcessTimes(process, &creation, &exitTime, &kernel, &user) || FileTimeToUint64(creation) != expectedStartTime;
        CloseHandle(process);
    }
    if (!stale) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Project is locked by a currently verified process"));
    if (!DeleteFileW(reinterpret_cast<LPCWSTR>(path.utf16()))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Verified stale project lock could not be removed")).toStdString()));
    return foundation::Result<void>::Success();
}
}

ProjectLock::~ProjectLock()
{
    const auto released = Release();
    if (!released.IsSuccess()) qCritical("Project lock release failed during destruction: %s", released.Failure().message.c_str());
}

foundation::Result<void> ProjectLock::Acquire(const QString &projectRoot, const QString &productVersion)
{
    if (IsHeld()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Project lock is already held by this instance"));
    if (projectRoot.isEmpty() || productVersion.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root and product version must not be empty"));
    m_path = QDir(projectRoot).filePath(QStringLiteral("project.lock"));
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE)
        {
            FILETIME creation{}, exitTime{}, kernel{}, user{};
            if (!GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user))
            {
                CloseHandle(handle);
                DeleteFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()));
                return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to inspect current process creation time")).toStdString()));
            }
            wchar_t computer[256]{}; DWORD computerSize = 256;
            wchar_t userName[256]{}; DWORD userSize = 256;
            if (!GetComputerNameW(computer, &computerSize) || !GetUserNameW(userName, &userSize))
            {
                CloseHandle(handle);
                DeleteFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()));
                return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to obtain lock owner identity")).toStdString()));
            }
            const QJsonObject record{{QStringLiteral("pid"), static_cast<qint64>(GetCurrentProcessId())}, {QStringLiteral("processStartFileTime"), QString::number(FileTimeToUint64(creation))}, {QStringLiteral("machine"), QString::fromWCharArray(computer)}, {QStringLiteral("user"), QString::fromWCharArray(userName)}, {QStringLiteral("productVersion"), productVersion}};
            const QByteArray bytes = QJsonDocument(record).toJson(QJsonDocument::Compact);
            DWORD written = 0;
            if (!WriteFile(handle, bytes.constData(), static_cast<DWORD>(bytes.size()), &written, nullptr) || written != static_cast<DWORD>(bytes.size()))
            {
                CloseHandle(handle);
                DeleteFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()));
                return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to write project lock")).toStdString()));
            }
            m_handle = handle;
            return foundation::Result<void>::Success();
        }
        if (GetLastError() != ERROR_FILE_EXISTS) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to create project lock")).toStdString()));
        const auto stale = RemoveStaleLockIfVerified(m_path);
        if (!stale.IsSuccess()) return stale;
    }
    return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Project lock creation failed after verified stale-lock recovery"));
}

foundation::Result<void> ProjectLock::Release()
{
    if (!IsHeld()) return foundation::Result<void>::Success();
    const HANDLE handle = static_cast<HANDLE>(m_handle);
    m_handle = nullptr;
    if (!CloseHandle(handle)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to close project lock handle")).toStdString()));
    if (!DeleteFileW(reinterpret_cast<LPCWSTR>(m_path.utf16()))) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, LastErrorMessage(QStringLiteral("Failed to remove project lock")).toStdString()));
    m_path.clear();
    return foundation::Result<void>::Success();
}

bool ProjectLock::IsHeld() const noexcept
{
    return m_handle != nullptr;
}
}
