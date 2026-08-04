#include "visionaiflow/qt_foundation/HostRuntime.h"

#include "visionaiflow/ipc/LocalClient.h"
#include "visionaiflow/ipc/Protocol.h"
#include "visionaiflow/qt_foundation/StructuredLogger.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <cstdio>

namespace visionaiflow::qt_foundation
{
namespace
{
QCborMap CreateMessage(const QString &type, const QString &jobId = {})
{
    QCborMap message;
    message.insert(QStringLiteral("protocolVersion"), ipc::ProtocolVersion);
    message.insert(QStringLiteral("requestId"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    message.insert(QStringLiteral("jobId"), jobId);
    message.insert(QStringLiteral("type"), type);
    message.insert(QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return message;
}
}

int RunHostApplication(QCoreApplication &application, const QString &hostRole, const QString &runtimeVersion, HostOperationHandler operationHandler, HostAsyncOperationHandler asyncOperationHandler)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("VisionAIFlow host process"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption serverOption(QStringLiteral("ipc-server"), QStringLiteral("Local IPC server name"), QStringLiteral("name"));
    parser.addOption(serverOption);
    parser.process(application);
    if (parser.isSet(QStringLiteral("help")))
    {
        QTextStream(stdout) << parser.helpText();
        return 0;
    }
    if (parser.isSet(QStringLiteral("version")))
    {
        QTextStream(stdout) << application.applicationVersion() << '\n';
        return 0;
    }
    if (!parser.isSet(serverOption))
    {
        QTextStream(stderr) << "Missing required --ipc-server option\n";
        return 2;
    }

    const auto initializeResult = StructuredLogger::Initialize(QDir(application.applicationDirPath()).filePath(QStringLiteral("logs")), hostRole);
    if (!initializeResult.IsSuccess())
    {
        QTextStream(stderr) << QString::fromStdString(initializeResult.Failure().message) << '\n';
        return 3;
    }
    StructuredLogger::Info(QStringLiteral("runtime"), QStringLiteral("Host runtime initialized"));

    ipc::LocalClient client;
    QTimer heartbeat;
    QTimer connectionTimeout;
    heartbeat.setInterval(1000);
    heartbeat.setSingleShot(false);
    connectionTimeout.setInterval(5000);
    connectionTimeout.setSingleShot(true);
    bool handshakeComplete = false;

    QObject::connect(&client, &ipc::LocalClient::Connected, &application, [&]() {
        connectionTimeout.stop();
        QCborMap hello = CreateMessage(QStringLiteral("hello"));
        hello.insert(QStringLiteral("hostRole"), hostRole);
        hello.insert(QStringLiteral("runtimeVersion"), runtimeVersion);
        const auto sent = client.Send(hello);
        if (!sent.IsSuccess())
        {
            StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
            application.exit(4);
        }
    });
    QObject::connect(&client, &ipc::LocalClient::MessageReceived, &application, [&](const QCborMap &message) {
        const QString type = message.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("helloAck"))
        {
            handshakeComplete = true;
            heartbeat.start();
            StructuredLogger::Info(QStringLiteral("ipc"), QStringLiteral("Handshake completed"));
            return;
        }
        if (type == QStringLiteral("shutdown"))
        {
            QCborMap completed = CreateMessage(QStringLiteral("completed"), message.value(QStringLiteral("jobId")).toString());
            completed.insert(QStringLiteral("requestId"), message.value(QStringLiteral("requestId")).toString());
            completed.insert(QStringLiteral("completedType"), QStringLiteral("shutdown"));
            const auto sent = client.Send(completed);
            if (!sent.IsSuccess())
            {
                StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
                application.exit(5);
                return;
            }
            heartbeat.stop();
            client.Disconnect();
            application.quit();
            return;
        }
        const bool isAsyncExecute = type == QStringLiteral("execute") && message.value(QStringLiteral("async")).toBool();
        if ((isAsyncExecute || type == QStringLiteral("cancel")) && asyncOperationHandler)
        {
            const QString requestId = message.value(QStringLiteral("requestId")).toString();
            const QString jobId = message.value(QStringLiteral("jobId")).toString();
            const QString operation = message.value(QStringLiteral("operation")).toString();
            const HostAsyncResponder responder = [&client, requestId, jobId, operation](const QString &responseType, const QCborMap &payload) -> foundation::Result<void> {
                if (responseType != QStringLiteral("progress") && responseType != QStringLiteral("completed") && responseType != QStringLiteral("cancelled") && responseType != QStringLiteral("failed")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Host async response type is unsupported"));
                for (const QString &reservedKey : {QStringLiteral("protocolVersion"), QStringLiteral("requestId"), QStringLiteral("jobId"), QStringLiteral("type"), QStringLiteral("timestampUtc"), QStringLiteral("operation")})
                {
                    if (payload.contains(reservedKey)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Host async response payload contains a reserved envelope key"));
                }
                QCborMap response = CreateMessage(responseType, jobId);
                response.insert(QStringLiteral("requestId"), requestId);
                for (auto iterator = payload.constBegin(); iterator != payload.constEnd(); ++iterator) response.insert(iterator.key(), iterator.value());
                if (!operation.isEmpty()) response.insert(QStringLiteral("operation"), operation);
                return client.Send(response);
            };
            const auto handled = asyncOperationHandler(message, responder);
            if (!handled.IsSuccess())
            {
                StructuredLogger::Error(QStringLiteral("operation"), handled.Failure());
                const auto sent = client.Send(ipc::CreateErrorResponse(message, handled.Failure(), true));
                if (!sent.IsSuccess()) application.exit(13);
            }
            return;
        }
        if (type == QStringLiteral("execute"))
        {
            if (!operationHandler)
            {
                const auto error = foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Host does not provide an operation handler");
                const auto sent = client.Send(ipc::CreateErrorResponse(message, error, true));
                if (!sent.IsSuccess())
                {
                    StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
                    application.exit(10);
                }
                return;
            }
            foundation::Result<QCborMap> operationResult = foundation::Result<QCborMap>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, "Host operation did not run"));
            try
            {
                operationResult = operationHandler(message);
            }
            catch (const std::exception &exception)
            {
                operationResult = foundation::Result<QCborMap>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, std::string("Host operation threw an exception: ") + exception.what()));
            }
            catch (...)
            {
                operationResult = foundation::Result<QCborMap>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, "Host operation threw an unknown exception"));
            }
            if (!operationResult.IsSuccess())
            {
                StructuredLogger::Error(QStringLiteral("operation"), operationResult.Failure());
                const auto sent = client.Send(ipc::CreateErrorResponse(message, operationResult.Failure(), true));
                if (!sent.IsSuccess())
                {
                    StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
                    application.exit(11);
                }
                return;
            }
            QCborMap completed = CreateMessage(QStringLiteral("completed"), message.value(QStringLiteral("jobId")).toString());
            completed.insert(QStringLiteral("requestId"), message.value(QStringLiteral("requestId")).toString());
            completed.insert(QStringLiteral("completedType"), QStringLiteral("execute"));
            for (auto iterator = operationResult.Value().constBegin(); iterator != operationResult.Value().constEnd(); ++iterator)
            {
                completed.insert(iterator.key(), iterator.value());
            }
            const auto sent = client.Send(completed);
            if (!sent.IsSuccess())
            {
                StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
                application.exit(12);
            }
            return;
        }
        const auto error = foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Host received an unsupported IPC message type");
        const auto response = ipc::CreateErrorResponse(message, error, true);
        const auto sent = client.Send(response);
        if (!sent.IsSuccess())
        {
            StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
            application.exit(6);
        }
    });
    QObject::connect(&client, &ipc::LocalClient::TransportError, &application, [&](const QString &code, const QString &message) {
        StructuredLogger::Error(QStringLiteral("ipc"), foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, (code + QStringLiteral(": ") + message).toStdString()));
        application.exit(handshakeComplete ? 7 : 8);
    });
    QObject::connect(&heartbeat, &QTimer::timeout, &application, [&]() {
        const auto sent = client.Send(CreateMessage(QStringLiteral("heartbeat")));
        if (!sent.IsSuccess())
        {
            StructuredLogger::Error(QStringLiteral("ipc"), sent.Failure());
            application.exit(9);
        }
    });

    QObject::connect(&connectionTimeout, &QTimer::timeout, &application, [&]() {
        if (!handshakeComplete)
        {
            StructuredLogger::Error(QStringLiteral("ipc"), foundation::Error::Create(foundation::ErrorCode::Timeout, "Host timed out while connecting to the local IPC server"));
            application.exit(13);
        }
    });

    StructuredLogger::Info(QStringLiteral("ipc"), QStringLiteral("Connecting to local IPC server: ") + parser.value(serverOption));
    client.ConnectToServer(parser.value(serverOption));
    connectionTimeout.start();
    return application.exec();
}
}
