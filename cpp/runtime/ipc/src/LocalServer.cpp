#include "visionaiflow/ipc/LocalServer.h"

namespace visionaiflow::ipc
{
LocalServer::LocalServer(QObject *parent) : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, &LocalServer::OnNewConnection);
}

foundation::Result<void> LocalServer::Start(const QString &serverName)
{
    if (serverName.isEmpty())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Local server name must not be empty"));
    }
    if (m_server.isListening())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState, "Local server is already listening"));
    }
    if (!m_server.listen(serverName))
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, m_server.errorString().toStdString()));
    }
    return foundation::Result<void>::Success();
}

void LocalServer::Stop()
{
    const auto clients = m_decoders.keys();
    for (QLocalSocket *client : clients)
    {
        client->disconnect(this);
        client->disconnectFromServer();
        client->deleteLater();
    }
    m_decoders.clear();
    m_requestIds.clear();
    m_server.close();
}

foundation::Result<void> LocalServer::Send(QLocalSocket *socket, const QCborMap &message)
{
    if (socket == nullptr || socket->state() != QLocalSocket::ConnectedState)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::ConnectionFailure, "Cannot send IPC message to a disconnected local socket"));
    }
    const auto frame = EncodeFrame(message);
    if (!frame.IsSuccess())
    {
        return foundation::Result<void>::Failure(frame.Failure());
    }
    if (socket->write(frame.Value()) != frame.Value().size())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::IoFailure, socket->errorString().toStdString()));
    }
    return foundation::Result<void>::Success();
}

void LocalServer::OnNewConnection()
{
    while (m_server.hasPendingConnections())
    {
        QLocalSocket *socket = m_server.nextPendingConnection();
        if (socket == nullptr)
        {
            continue;
        }
        socket->setParent(this);
        m_decoders.insert(socket, FrameDecoder{});
        m_requestIds.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, &LocalServer::OnReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &LocalServer::OnDisconnected);
        emit ClientConnected(socket);
    }
}

void LocalServer::OnReadyRead()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (socket == nullptr || !m_decoders.contains(socket))
    {
        return;
    }
    const auto messages = m_decoders[socket].Append(socket->readAll());
    if (!messages.IsSuccess())
    {
        DisconnectForError(socket, messages.Failure());
        return;
    }
    for (const QCborMap &message : messages.Value())
    {
        const auto remembered = m_requestIds[socket].Remember(message);
        if (!remembered.IsSuccess())
        {
            const auto response = CreateErrorResponse(message, remembered.Failure(), false);
            const auto sent = Send(socket, response);
            if (!sent.IsSuccess())
            {
                DisconnectForError(socket, sent.Failure());
            }
            continue;
        }
        emit MessageReceived(socket, message);
    }
}

void LocalServer::OnDisconnected()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (socket == nullptr)
    {
        return;
    }
    m_decoders.remove(socket);
    m_requestIds.remove(socket);
    emit ClientDisconnected(socket);
    socket->deleteLater();
}

void LocalServer::DisconnectForError(QLocalSocket *socket, const foundation::Error &error)
{
    emit TransportError(socket, QString::fromLatin1(foundation::ToString(error.code)), QString::fromStdString(error.message));
    socket->disconnectFromServer();
}
}
