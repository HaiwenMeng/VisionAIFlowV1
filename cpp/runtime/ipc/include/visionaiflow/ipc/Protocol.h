#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QCborMap>
#include <QByteArray>
#include <QSet>
#include <QVector>

namespace visionaiflow::ipc
{
constexpr int ProtocolVersion = 1;
constexpr quint32 MaximumControlMessageBytes = 16U * 1024U * 1024U;

class FrameDecoder final
{
public:
    foundation::Result<QVector<QCborMap>> Append(const QByteArray &bytes);
    void Reset() noexcept;

private:
    QByteArray m_buffer;
};

class RequestIdTracker final
{
public:
    foundation::Result<void> Remember(const QCborMap &message);
    void Clear() noexcept;

private:
    QSet<QString> m_seenRequestIds;
};

foundation::Result<QByteArray> EncodeFrame(const QCborMap &message);
foundation::Result<void> ValidateEnvelope(const QCborMap &message);
QCborMap CreateErrorResponse(const QCborMap &request, const foundation::Error &error, bool recoverable);
}
