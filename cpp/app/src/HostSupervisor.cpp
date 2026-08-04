#include "visionaiflow/app/HostSupervisor.h"

#include "visionaiflow/ipc/Protocol.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>

#include <stdexcept>

namespace visionaiflow::app
{
HostSupervisor::HostSupervisor(QObject *parent) : QObject(parent)
{
    m_serverName = QStringLiteral("VisionAIFlowV1-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto started = m_server.Start(m_serverName);
    if (!started.IsSuccess())
    {
        throw std::runtime_error(started.Failure().message);
    }
    connect(&m_server, &ipc::LocalServer::MessageReceived, this, &HostSupervisor::OnMessage);
    connect(&m_server, &ipc::LocalServer::TransportError, this, [this](QLocalSocket *socket, const QString &code, const QString &message) {
        emit HostError(m_socketRoles.value(socket, QStringLiteral("unknown")), code, message);
    });
}

foundation::Result<void> HostSupervisor::StartHost(const QString &role, const QString &programPath)
{
    if (role.isEmpty() || programPath.isEmpty())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Host role and executable path must not be empty"));
    }
    if (m_processes.contains(role) && m_processes.value(role)->state() != QProcess::NotRunning)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState, "Requested host is already running"));
    }

    auto *process = new QProcess(this);
    process->setProgram(programPath);
    process->setArguments({QStringLiteral("--ipc-server"), m_serverName});
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::errorOccurred, this, &HostSupervisor::OnProcessError);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &HostSupervisor::OnProcessFinished);
    m_processes.insert(role, process);
    m_completedRoles.remove(role);
    emit HostStateChanged(role, QStringLiteral("starting"));
    process->start();
    return foundation::Result<void>::Success();
}

foundation::Result<void> HostSupervisor::ShutdownHost(const QString &role)
{
    if (!m_processes.contains(role))
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Cannot stop an unknown host role"));
    }
    QProcess *process = m_processes.value(role);
    if (process->state() == QProcess::NotRunning)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState, "Cannot stop a host process that is not running"));
    }
    const auto socket = m_socketRoles.key(role, nullptr);
    if (socket == nullptr)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, "Host has not completed IPC handshake and cannot receive structured shutdown"));
    }
    const auto sent = m_server.Send(socket, CreateServerMessage(QStringLiteral("shutdown"), QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!sent.IsSuccess())
    {
        return sent;
    }
    emit HostStateChanged(role, QStringLiteral("stopping"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> HostSupervisor::ExecuteHost(const QString &role, const QString &jobId, const QString &operation, const QCborMap &payload, const bool asynchronous)
{
    if (role.isEmpty() || jobId.isEmpty() || operation.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Host operation requires non-empty role, jobId, and operation"));
    if (!m_processes.contains(role) || m_processes.value(role)->state() == QProcess::NotRunning) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Host operation requires a running host process"));
    const auto socket = m_socketRoles.key(role, nullptr);
    if (socket == nullptr) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, "Host operation requires a completed IPC handshake"));
    for (const QString &reservedKey : {QStringLiteral("protocolVersion"), QStringLiteral("requestId"), QStringLiteral("jobId"), QStringLiteral("type"), QStringLiteral("timestampUtc"), QStringLiteral("operation"), QStringLiteral("async")})
    {
        if (payload.contains(reservedKey)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Host operation payload contains a reserved envelope key"));
    }
    QCborMap request = CreateServerMessage(QStringLiteral("execute"), QUuid::createUuid().toString(QUuid::WithoutBraces), jobId);
    request.insert(QStringLiteral("operation"), operation);
    request.insert(QStringLiteral("async"), asynchronous);
    for (auto iterator = payload.constBegin(); iterator != payload.constEnd(); ++iterator) request.insert(iterator.key(), iterator.value());
    return m_server.Send(socket, request);
}

bool HostSupervisor::HasRunningHosts() const noexcept
{
    for (const QProcess *process : m_processes)
    {
        if (process != nullptr && process->state() != QProcess::NotRunning)
        {
            return true;
        }
    }
    return false;
}

void HostSupervisor::OnMessage(QLocalSocket *socket, const QCborMap &message)
{
    const QString type = message.value(QStringLiteral("type")).toString();
    const QString role = message.value(QStringLiteral("hostRole")).toString();
    if (type == QStringLiteral("hello"))
    {
        if (!m_processes.contains(role) || message.value(QStringLiteral("runtimeVersion")).toString().isEmpty())
        {
            const auto error = foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Host hello has an unknown role or empty runtimeVersion");
            const auto sent = m_server.Send(socket, ipc::CreateErrorResponse(message, error, false));
            if (!sent.IsSuccess())
            {
                emit HostError(role, QString::fromLatin1(foundation::ToString(sent.Failure().code)), QString::fromStdString(sent.Failure().message));
            }
            return;
        }
        m_socketRoles.insert(socket, role);
        const auto sent = m_server.Send(socket, CreateServerMessage(QStringLiteral("helloAck"), message.value(QStringLiteral("requestId")).toString()));
        if (!sent.IsSuccess())
        {
            emit HostError(role, QString::fromLatin1(foundation::ToString(sent.Failure().code)), QString::fromStdString(sent.Failure().message));
            return;
        }
        emit HostStateChanged(role, QStringLiteral("running"));
        return;
    }
    const QString knownRole = m_socketRoles.value(socket);
    if (knownRole.isEmpty())
    {
        const auto error = foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Host sent a non-hello message before completing handshake");
        m_server.Send(socket, ipc::CreateErrorResponse(message, error, false));
        return;
    }
    if (type == QStringLiteral("heartbeat"))
    {
        return;
    }
    const QString operation = message.value(QStringLiteral("operation")).toString();
    const QString jobId = message.value(QStringLiteral("jobId")).toString();
    if (type == QStringLiteral("progress"))
    {
        emit HostOperationProgress(knownRole, jobId, operation, message);
        return;
    }
    if (type == QStringLiteral("cancelled"))
    {
        emit HostOperationCancelled(knownRole, jobId, operation, message);
        return;
    }
    if (type == QStringLiteral("failed"))
    {
        const QString errorCode = message.value(QStringLiteral("errorCode")).toString();
        const QString errorMessage = message.value(QStringLiteral("errorMessage")).toString();
        if (errorCode.isEmpty() || errorMessage.isEmpty())
        {
            const auto error = foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Host async failure is missing errorCode or errorMessage");
            m_server.Send(socket, ipc::CreateErrorResponse(message, error, false));
            return;
        }
        emit HostOperationFailed(knownRole, jobId, operation, errorCode, errorMessage, message);
        return;
    }
    if (type == QStringLiteral("completed") && message.value(QStringLiteral("completedType")).toString() == QStringLiteral("shutdown"))
    {
        m_completedRoles.insert(knownRole);
        emit HostStateChanged(knownRole, QStringLiteral("completed"));
        return;
    }
    if (type == QStringLiteral("completed") && message.value(QStringLiteral("completedType")).toString() == QStringLiteral("execute"))
    {
        if (operation.isEmpty())
        {
            const auto error = foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Host execute completion is missing its operation name");
            m_server.Send(socket, ipc::CreateErrorResponse(message, error, false));
            return;
        }
        emit HostOperationCompleted(knownRole, jobId, operation, message);
        return;
    }
    if (type == QStringLiteral("completed"))
    {
        emit HostOperationCompleted(knownRole, jobId, operation, message);
        return;
    }
    const auto error = foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "UI supervisor received an unsupported host IPC message type");
    m_server.Send(socket, ipc::CreateErrorResponse(message, error, true));
}

void HostSupervisor::OnProcessError(const QProcess::ProcessError)
{
    auto *process = qobject_cast<QProcess *>(sender());
    const QString role = RoleForProcess(process);
    emit HostError(role, QStringLiteral("ProcessError"), process == nullptr ? QStringLiteral("Unknown process error") : process->errorString());
}

void HostSupervisor::OnProcessFinished(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    auto *process = qobject_cast<QProcess *>(sender());
    const QString role = RoleForProcess(process);
    if (role.isEmpty())
    {
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0 || !m_completedRoles.contains(role))
    {
        emit HostError(role, QStringLiteral("ProcessFailure"), QStringLiteral("Host exited without confirmed completion or returned a non-zero exit code"));
    }
    else
    {
        emit HostStateChanged(role, QStringLiteral("stopped"));
    }
    m_socketRoles.remove(m_socketRoles.key(role, nullptr));
}

QCborMap HostSupervisor::CreateServerMessage(const QString &type, const QString &requestId, const QString &jobId) const
{
    QCborMap message;
    message.insert(QStringLiteral("protocolVersion"), ipc::ProtocolVersion);
    message.insert(QStringLiteral("requestId"), requestId);
    message.insert(QStringLiteral("jobId"), jobId);
    message.insert(QStringLiteral("type"), type);
    message.insert(QStringLiteral("timestampUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return message;
}

QString HostSupervisor::RoleForProcess(const QProcess *process) const
{
    return m_processes.key(const_cast<QProcess *>(process));
}
}
