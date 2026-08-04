#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/ipc/LocalServer.h"

#include <QHash>
#include <QProcess>
#include <QSet>

namespace visionaiflow::app
{
class HostSupervisor final : public QObject
{
    Q_OBJECT

public:
    explicit HostSupervisor(QObject *parent = nullptr);
    foundation::Result<void> StartHost(const QString &role, const QString &programPath);
    foundation::Result<void> ShutdownHost(const QString &role);
    foundation::Result<void> ExecuteHost(const QString &role, const QString &jobId, const QString &operation, const QCborMap &payload = {}, bool asynchronous = false);
    [[nodiscard]] bool HasRunningHosts() const noexcept;

signals:
    void HostStateChanged(const QString &role, const QString &state);
    void HostError(const QString &role, const QString &errorCode, const QString &errorMessage);
    void HostOperationProgress(const QString &role, const QString &jobId, const QString &operation, const QCborMap &payload);
    void HostOperationCompleted(const QString &role, const QString &jobId, const QString &operation, const QCborMap &payload);
    void HostOperationCancelled(const QString &role, const QString &jobId, const QString &operation, const QCborMap &payload);
    void HostOperationFailed(const QString &role, const QString &jobId, const QString &operation, const QString &errorCode, const QString &errorMessage, const QCborMap &payload);

private slots:
    void OnMessage(QLocalSocket *socket, const QCborMap &message);
    void OnProcessError(QProcess::ProcessError error);
    void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QCborMap CreateServerMessage(const QString &type, const QString &requestId, const QString &jobId = {}) const;
    QString RoleForProcess(const QProcess *process) const;

    ipc::LocalServer m_server;
    QString m_serverName;
    QHash<QString, QProcess *> m_processes;
    QHash<QLocalSocket *, QString> m_socketRoles;
    QSet<QString> m_completedRoles;
};
}
