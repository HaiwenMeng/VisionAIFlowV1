#include "trainingprotocol.h"

#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

namespace TrainingProtocol
{
void configurePythonProcess(QProcess *process)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::SeparateChannels);
}

QList<QByteArray> takeLines(QByteArray &pending, const QByteArray &data)
{
    pending.append(data);
    QList<QByteArray> lines;
    for (qsizetype newline = pending.indexOf('\n'); newline >= 0; newline = pending.indexOf('\n'))
    {
        const QByteArray line = pending.left(newline).trimmed();
        pending.remove(0, newline + 1);
        if (!line.isEmpty())
        {
            lines.append(line);
        }
    }
    return lines;
}

bool parseEvent(const QByteArray &line, QJsonObject *event, QString *errorMessage)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        *errorMessage = QString(u8"无效 JSON 事件: %1").arg(QString::fromUtf8(line));
        return false;
    }
    *event = document.object();
    return true;
}

bool validateTrainingEvent(const QJsonObject &event,
                           const QString &taskName,
                           const QString &runId,
                           const QString &task,
                           bool *ready,
                           QString *errorMessage)
{
    const QString type = event.value(QStringLiteral("type")).toString();
    if (!*ready && type == QStringLiteral("error") &&
        event.value(QStringLiteral("protocol")).toString() == QStringLiteral("visionaiflow.training.v1") &&
        event.value(QStringLiteral("run_id")).isNull())
    {
        *errorMessage = QString(u8"训练进程启动失败: %1").arg(event.value(QStringLiteral("message")).toString());
        return false;
    }
    if (event.value(QStringLiteral("protocol")).toString() != QStringLiteral("visionaiflow.training.v1") ||
        event.value(QStringLiteral("job_id")).toString() != taskName ||
        event.value(QStringLiteral("run_id")).toString() != runId ||
        event.value(QStringLiteral("task")).toString() != task)
    {
        *errorMessage = QString(u8"协议版本、任务或运行标识不匹配");
        return false;
    }
    if (type == QStringLiteral("worker_ready"))
    {
        *ready = true;
    }
    else if (!*ready)
    {
        *errorMessage = QString(u8"训练进程未完成协议握手");
        return false;
    }
    return true;
}

bool requestCancellation(QProcess *process, QTimer *startTimer, QTimer *cancelTimer, QString *errorMessage)
{
    const QByteArray command("{\"cmd\":\"cancel\"}\n");
    if (process->write(command) != command.size())
    {
        *errorMessage = QString(u8"无法发送停止训练命令: %1").arg(process->errorString());
        return false;
    }
    startTimer->stop();
    cancelTimer->start(10000);
    return true;
}

void terminateWithGrace(QProcess *process, QObject *context)
{
    const qint64 processId = process->processId();
    process->terminate();
    QTimer::singleShot(5000,
                       context,
                       [process, processId]()
                       {
                           if (process->state() != QProcess::NotRunning && process->processId() == processId)
                           {
                               process->kill();
                           }
                       });
}
} // namespace TrainingProtocol
