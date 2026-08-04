#include "visionaiflow/tensor/GpuLease.h"

#include <QCryptographicHash>

#include <windows.h>

namespace visionaiflow::tensor
{
namespace
{
QString MutexName(const QString &gpuUuid)
{
    const QByteArray digest = QCryptographicHash::hash(gpuUuid.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("Local\\VisionAIFlowV1-GpuLease-") + QString::fromLatin1(digest);
}
}

GpuLease::~GpuLease()
{
    const auto released = Release();
    if (!released.IsSuccess()) qCritical("GPU lease release failed during destruction: %s", released.Failure().message.c_str());
}

foundation::Result<void> GpuLease::Acquire(const QString &gpuUuid)
{
    if (IsHeld()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "GPU lease is already held by this instance"));
    if (gpuUuid.trimmed().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "GPU UUID must not be empty"));
    const QString name = MutexName(gpuUuid);
    HANDLE handle = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(name.utf16()));
    if (handle == nullptr) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to create GPU lease mutex"));
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(handle);
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "GPU is leased by another VisionAIFlow process"));
    }
    m_handle = handle;
    m_name = name;
    return foundation::Result<void>::Success();
}

foundation::Result<void> GpuLease::Release()
{
    if (!IsHeld()) return foundation::Result<void>::Success();
    HANDLE handle = static_cast<HANDLE>(m_handle);
    m_handle = nullptr;
    m_name.clear();
    if (!CloseHandle(handle)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to close GPU lease mutex"));
    return foundation::Result<void>::Success();
}

bool GpuLease::IsHeld() const noexcept { return m_handle != nullptr; }
}
