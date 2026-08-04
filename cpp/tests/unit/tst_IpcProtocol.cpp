#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/ipc/Protocol.h"

#include <QtTest>

namespace
{
QCborMap CreateEnvelope()
{
    QCborMap message;
    message.insert(QStringLiteral("protocolVersion"), visionaiflow::ipc::ProtocolVersion);
    message.insert(QStringLiteral("requestId"), QStringLiteral("request-1"));
    message.insert(QStringLiteral("jobId"), QStringLiteral("job-1"));
    message.insert(QStringLiteral("type"), QStringLiteral("hello"));
    message.insert(QStringLiteral("timestampUtc"), QStringLiteral("2026-07-21T00:00:00.000Z"));
    return message;
}
}

class IpcProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void ResultRequiresNonEmptyErrorMessage();
    void FrameDecoderHandlesPartialFrame();
    void FrameDecoderHandlesCoalescedFrames();
    void FrameDecoderRejectsOversizedFrame();
    void EnvelopeRejectsIncompatibleVersion();
    void RequestIdTrackerRejectsDuplicateRequestId();
    void ErrorResponseCarriesRequiredErrorFields();
};

void IpcProtocolTest::ResultRequiresNonEmptyErrorMessage()
{
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        visionaiflow::foundation::Result<void>::Failure({visionaiflow::foundation::ErrorCode::InvalidArgument, {}, {}}));
}

void IpcProtocolTest::FrameDecoderHandlesPartialFrame()
{
    const auto encoded = visionaiflow::ipc::EncodeFrame(CreateEnvelope());
    QVERIFY(encoded.IsSuccess());
    visionaiflow::ipc::FrameDecoder decoder;
    const QByteArray firstHalf = encoded.Value().left(encoded.Value().size() / 2);
    const QByteArray secondHalf = encoded.Value().mid(firstHalf.size());
    const auto beforeCompletion = decoder.Append(firstHalf);
    QVERIFY(beforeCompletion.IsSuccess());
    QCOMPARE(beforeCompletion.Value().size(), 0);
    const auto complete = decoder.Append(secondHalf);
    QVERIFY(complete.IsSuccess());
    QCOMPARE(complete.Value().size(), 1);
    QCOMPARE(complete.Value().first().value(QStringLiteral("type")).toString(), QStringLiteral("hello"));
}

void IpcProtocolTest::FrameDecoderHandlesCoalescedFrames()
{
    QCborMap first = CreateEnvelope();
    QCborMap second = CreateEnvelope();
    second.insert(QStringLiteral("requestId"), QStringLiteral("request-2"));
    second.insert(QStringLiteral("type"), QStringLiteral("execute"));
    const auto firstFrame = visionaiflow::ipc::EncodeFrame(first);
    const auto secondFrame = visionaiflow::ipc::EncodeFrame(second);
    QVERIFY(firstFrame.IsSuccess());
    QVERIFY(secondFrame.IsSuccess());
    visionaiflow::ipc::FrameDecoder decoder;
    const auto decoded = decoder.Append(firstFrame.Value() + secondFrame.Value());
    QVERIFY(decoded.IsSuccess());
    QCOMPARE(decoded.Value().size(), 2);
    QCOMPARE(decoded.Value().at(0).value(QStringLiteral("requestId")).toString(), QStringLiteral("request-1"));
    QCOMPARE(decoded.Value().at(1).value(QStringLiteral("requestId")).toString(), QStringLiteral("request-2"));
    QCOMPARE(decoded.Value().at(1).value(QStringLiteral("type")).toString(), QStringLiteral("execute"));
}

void IpcProtocolTest::FrameDecoderRejectsOversizedFrame()
{
    const quint32 oversized = visionaiflow::ipc::MaximumControlMessageBytes + 1U;
    QByteArray header(4, Qt::Uninitialized);
    header[0] = static_cast<char>(oversized & 0xFFU);
    header[1] = static_cast<char>((oversized >> 8U) & 0xFFU);
    header[2] = static_cast<char>((oversized >> 16U) & 0xFFU);
    header[3] = static_cast<char>((oversized >> 24U) & 0xFFU);
    visionaiflow::ipc::FrameDecoder decoder;
    const auto decoded = decoder.Append(header);
    QVERIFY(!decoded.IsSuccess());
    QVERIFY(decoded.Failure().code == visionaiflow::foundation::ErrorCode::MessageTooLarge);
}

void IpcProtocolTest::EnvelopeRejectsIncompatibleVersion()
{
    QCborMap message = CreateEnvelope();
    message.insert(QStringLiteral("protocolVersion"), visionaiflow::ipc::ProtocolVersion + 1);
    const auto validation = visionaiflow::ipc::ValidateEnvelope(message);
    QVERIFY(!validation.IsSuccess());
    QVERIFY(validation.Failure().code == visionaiflow::foundation::ErrorCode::ProtocolVersionMismatch);
}

void IpcProtocolTest::RequestIdTrackerRejectsDuplicateRequestId()
{
    visionaiflow::ipc::RequestIdTracker tracker;
    QCborMap first = CreateEnvelope();
    first.insert(QStringLiteral("requestId"), QStringLiteral("duplicate-request"));
    first.insert(QStringLiteral("type"), QStringLiteral("execute"));
    QCborMap second = first;
    const auto firstResult = tracker.Remember(first);
    QVERIFY(firstResult.IsSuccess());
    const auto duplicateResult = tracker.Remember(second);
    QVERIFY(!duplicateResult.IsSuccess());
    QVERIFY(duplicateResult.Failure().code == visionaiflow::foundation::ErrorCode::DuplicateRequest);
    QVERIFY(!duplicateResult.Failure().message.empty());
    tracker.Clear();
    const auto afterClear = tracker.Remember(second);
    QVERIFY(afterClear.IsSuccess());
}

void IpcProtocolTest::ErrorResponseCarriesRequiredErrorFields()
{
    const QCborMap request = CreateEnvelope();
    const auto error = visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::DuplicateRequest, "Duplicate requestId received on one IPC connection");
    const QCborMap response = visionaiflow::ipc::CreateErrorResponse(request, error, false);
    QCOMPARE(response.value(QStringLiteral("type")).toString(), QStringLiteral("error"));
    QCOMPARE(response.value(QStringLiteral("requestId")).toString(), request.value(QStringLiteral("requestId")).toString());
    QCOMPARE(response.value(QStringLiteral("errorCode")).toString(), QStringLiteral("DuplicateRequest"));
    QVERIFY(!response.value(QStringLiteral("errorMessage")).toString().isEmpty());
    QCOMPARE(response.value(QStringLiteral("recoverable")).toBool(true), false);
}

QTEST_GUILESS_MAIN(IpcProtocolTest)

#include "tst_IpcProtocol.moc"
