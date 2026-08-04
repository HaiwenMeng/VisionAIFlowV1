#include "visionaiflow/ipc/LocalClient.h"

namespace visionaiflow::ipc
{
LocalClient::LocalClient(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QLocalSocket::connected, this, &LocalClient::Connected);
    connect(&m_socket, &QLocalSocket::disconnected, this, &LocalClient::Disconnected);
    connect(&m_socket, &QLocalSocket::disconnected, this, [this]() { m_requestIds.Clear(); });
    connect(&m_socket, &QLocalSocket::readyRead, this, &LocalClient::OnReadyRead);
    connect(&m_socket, &QLocalSocket::errorOccurred, this, &LocalClient::OnSocketError);
}

void LocalClient::ConnectToServer(const QString &serverName)
{
    m_decoder.Reset();
    m_requestIds.Clear();
    m_socket.abort();
    m_socket.connectToServer(serverName);
}

foundation::Result<void> LocalClient::Send(const QCborMap &message)
{
    if (m_socket.state() != QLocalSocket::ConnectedState)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, "Cannot send IPC message before local socket connection is established"));
    }
    const auto frame = EncodeFrame(message);
    if (!frame.IsSuccess())
    {
        return foundation::Result<void>::Failure(frame.Failure());
    }
    if (m_socket.write(frame.Value()) != frame.Value().size())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::IoFailure, m_socket.errorString().toStdString()));
    }
    return foundation::Result<void>::Success();
}

void LocalClient::Disconnect()
{
    m_requestIds.Clear();
    m_socket.disconnectFromServer();
}

QLocalSocket::LocalSocketState LocalClient::State() const noexcept
{
    return m_socket.state();
}

void LocalClient::OnReadyRead()
{
    const auto messages = m_decoder.Append(m_socket.readAll());
    if (!messages.IsSuccess())
    {
        emit TransportError(QString::fromLatin1(foundation::ToString(messages.Failure().code)), QString::fromStdString(messages.Failure().message));
        m_socket.disconnectFromServer();
        return;
    }
    for (const QCborMap &message : messages.Value())
    {
        const auto remembered = m_requestIds.Remember(message);
        if (!remembered.IsSuccess())
        {
            const auto sent = Send(CreateErrorResponse(message, remembered.Failure(), false));
            if (!sent.IsSuccess())
            {
                emit TransportError(QString::fromLatin1(foundation::ToString(sent.Failure().code)), QString::fromStdString(sent.Failure().message));
            }
            emit TransportError(QString::fromLatin1(foundation::ToString(remembered.Failure().code)), QString::fromStdString(remembered.Failure().message));
            m_socket.disconnectFromServer();
            return;
        }
        emit MessageReceived(message);
    }
}

void LocalClient::OnSocketError(const QLocalSocket::LocalSocketError)
{
    emit TransportError(QStringLiteral("LocalSocketError"), m_socket.errorString());
}
}
