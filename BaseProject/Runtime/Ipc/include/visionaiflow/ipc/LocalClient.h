#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/ipc/Protocol.h"

#include <QLocalSocket>

namespace visionaiflow::ipc
{
class VISIONAIFLOW_IPC_EXPORT LocalClient final : public QObject
{
    Q_OBJECT

public:
    explicit LocalClient(QObject *parent = nullptr);
    void ConnectToServer(const QString &serverName);
    foundation::Result<void> Send(const QCborMap &message);
    void Disconnect();
    [[nodiscard]] QLocalSocket::LocalSocketState State() const noexcept;

signals:
    void Connected();
    void Disconnected();
    void MessageReceived(const QCborMap &message);
    void TransportError(const QString &errorCode, const QString &errorMessage);

private slots:
    void OnReadyRead();
    void OnSocketError(QLocalSocket::LocalSocketError error);

private:
    QLocalSocket m_socket;
    FrameDecoder m_decoder;
    RequestIdTracker m_requestIds;
};
}
