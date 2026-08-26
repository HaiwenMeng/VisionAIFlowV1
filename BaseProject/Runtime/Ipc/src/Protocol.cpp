#include "visionaiflow/ipc/Protocol.h"

#include <QCborParserError>
#include <QDateTime>

namespace visionaiflow::ipc
{
namespace
{
foundation::Error ProtocolError(const QString &message)
{
    return foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, message.toStdString());
}
}

foundation::Result<QByteArray> EncodeFrame(const QCborMap &message)
{
    const auto validation = ValidateEnvelope(message);
    if (!validation.IsSuccess())
    {
        return foundation::Result<QByteArray>::Failure(validation.Failure());
    }

    const QByteArray payload = QCborValue(message).toCbor();
    if (payload.size() > static_cast<qsizetype>(MaximumControlMessageBytes))
    {
        return foundation::Result<QByteArray>::Failure(
            foundation::Error::Create(foundation::ErrorCode::MessageTooLarge, "CBOR payload exceeds 16 MiB control message limit"));
    }

    const quint32 length = static_cast<quint32>(payload.size());
    QByteArray frame(4, Qt::Uninitialized);
    frame[0] = static_cast<char>(length & 0xFFU);
    frame[1] = static_cast<char>((length >> 8U) & 0xFFU);
    frame[2] = static_cast<char>((length >> 16U) & 0xFFU);
    frame[3] = static_cast<char>((length >> 24U) & 0xFFU);
    frame.append(payload);
    return foundation::Result<QByteArray>::Success(std::move(frame));
}

foundation::Result<QVector<QCborMap>> FrameDecoder::Append(const QByteArray &bytes)
{
    m_buffer.append(bytes);
    QVector<QCborMap> messages;
    while (m_buffer.size() >= 4)
    {
        const auto *data = reinterpret_cast<const unsigned char *>(m_buffer.constData());
        const quint32 length = static_cast<quint32>(data[0]) |
            (static_cast<quint32>(data[1]) << 8U) |
            (static_cast<quint32>(data[2]) << 16U) |
            (static_cast<quint32>(data[3]) << 24U);
        if (length > MaximumControlMessageBytes)
        {
            Reset();
            return foundation::Result<QVector<QCborMap>>::Failure(
                foundation::Error::Create(foundation::ErrorCode::MessageTooLarge, "IPC frame length exceeds 16 MiB"));
        }
        const qsizetype completeLength = 4 + static_cast<qsizetype>(length);
        if (m_buffer.size() < completeLength)
        {
            break;
        }

        const QByteArray payload = m_buffer.mid(4, static_cast<qsizetype>(length));
        m_buffer.remove(0, completeLength);
        QCborParserError parseError{};
        const QCborValue value = QCborValue::fromCbor(payload, &parseError);
        if (parseError.error != QCborError::NoError || !value.isMap())
        {
            Reset();
            return foundation::Result<QVector<QCborMap>>::Failure(
                foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "IPC frame payload is not a valid CBOR map"));
        }
        const QCborMap message = value.toMap();
        const auto validation = ValidateEnvelope(message);
        if (!validation.IsSuccess())
        {
            Reset();
            return foundation::Result<QVector<QCborMap>>::Failure(validation.Failure());
        }
        messages.append(message);
    }
    return foundation::Result<QVector<QCborMap>>::Success(std::move(messages));
}

void FrameDecoder::Reset() noexcept
{
    m_buffer.clear();
}

foundation::Result<void> RequestIdTracker::Remember(const QCborMap &message)
{
    const QString requestId = message.value(QStringLiteral("requestId")).toString();
    if (m_seenRequestIds.contains(requestId))
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::DuplicateRequest, "Duplicate requestId received on one IPC connection"));
    }
    m_seenRequestIds.insert(requestId);
    return foundation::Result<void>::Success();
}

void RequestIdTracker::Clear() noexcept
{
    m_seenRequestIds.clear();
}

foundation::Result<void> ValidateEnvelope(const QCborMap &message)
{
    const auto version = message.value(QStringLiteral("protocolVersion"));
    const auto requestId = message.value(QStringLiteral("requestId"));
    const auto jobId = message.value(QStringLiteral("jobId"));
    const auto type = message.value(QStringLiteral("type"));
    const auto timestampUtc = message.value(QStringLiteral("timestampUtc"));
    if (!version.isInteger() || version.toInteger() != ProtocolVersion)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ProtocolVersionMismatch, "IPC protocolVersion is missing or unsupported"));
    }
    if (!requestId.isString() || requestId.toString().isEmpty() || !jobId.isString() || !type.isString() || type.toString().isEmpty() || !timestampUtc.isString() || timestampUtc.toString().isEmpty())
    {
        return foundation::Result<void>::Failure(ProtocolError(QStringLiteral("IPC envelope requires non-empty requestId, type and timestampUtc strings plus a jobId string")));
    }
    return foundation::Result<void>::Success();
}

QCborMap CreateErrorResponse(const QCborMap &request, const foundation::Error &error, const bool recoverable)
{
    QCborMap response;
    response.insert(QStringLiteral("protocolVersion"), ProtocolVersion);
    response.insert(QStringLiteral("requestId"), request.value(QStringLiteral("requestId")).toString());
    response.insert(QStringLiteral("jobId"), request.value(QStringLiteral("jobId")).toString());
    response.insert(QStringLiteral("type"), QStringLiteral("error"));
    response.insert(QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    response.insert(QStringLiteral("errorCode"), QString::fromLatin1(foundation::ToString(error.code)));
    response.insert(QStringLiteral("errorMessage"), QString::fromStdString(error.message));
    response.insert(QStringLiteral("recoverable"), recoverable);
    return response;
}
}
