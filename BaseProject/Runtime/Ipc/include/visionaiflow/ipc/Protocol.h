#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QCborMap>
#include <QByteArray>
#include <QSet>
#include <QVector>

#if defined(VISIONAIFLOW_IPC_LIBRARY)
#define VISIONAIFLOW_IPC_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_IPC_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::ipc
{
constexpr int ProtocolVersion = 1;
constexpr quint32 MaximumControlMessageBytes = 16U * 1024U * 1024U;

class VISIONAIFLOW_IPC_EXPORT FrameDecoder final
{
public:
    foundation::Result<QVector<QCborMap>> Append(const QByteArray &bytes);
    void Reset() noexcept;

private:
    QByteArray m_buffer;
};

class VISIONAIFLOW_IPC_EXPORT RequestIdTracker final
{
public:
    foundation::Result<void> Remember(const QCborMap &message);
    void Clear() noexcept;

private:
    QSet<QString> m_seenRequestIds;
};

VISIONAIFLOW_IPC_EXPORT foundation::Result<QByteArray> EncodeFrame(const QCborMap &message);
VISIONAIFLOW_IPC_EXPORT foundation::Result<void> ValidateEnvelope(const QCborMap &message);
VISIONAIFLOW_IPC_EXPORT QCborMap CreateErrorResponse(const QCborMap &request, const foundation::Error &error, bool recoverable);
}
