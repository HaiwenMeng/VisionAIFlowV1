#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/ipc/Protocol.h"

#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>

namespace visionaiflow::ipc
{
class LocalServer final : public QObject
{
    Q_OBJECT

public:
    explicit LocalServer(QObject *parent = nullptr);
    foundation::Result<void> Start(const QString &serverName);
    void Stop();
    foundation::Result<void> Send(QLocalSocket *socket, const QCborMap &message);

signals:
    void ClientConnected(QLocalSocket *socket);
    void ClientDisconnected(QLocalSocket *socket);
    void MessageReceived(QLocalSocket *socket, const QCborMap &message);
    void TransportError(QLocalSocket *socket, const QString &errorCode, const QString &errorMessage);

private slots:
    void OnNewConnection();
    void OnReadyRead();
    void OnDisconnected();

private:
    void DisconnectForError(QLocalSocket *socket, const foundation::Error &error);

    QLocalServer m_server;
    QHash<QLocalSocket *, FrameDecoder> m_decoders;
    QHash<QLocalSocket *, RequestIdTracker> m_requestIds;
};
}
