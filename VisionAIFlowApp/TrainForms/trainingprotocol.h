#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

class QObject;
class QProcess;
class QTimer;

namespace TrainingProtocol
{
QList<QByteArray> takeLines(QByteArray &pending, const QByteArray &data);
void configurePythonProcess(QProcess *process);
bool parseEvent(const QByteArray &line, QJsonObject *event, QString *errorMessage);
bool validateTrainingEvent(const QJsonObject &event,
                           const QString &taskName,
                           const QString &runId,
                           const QString &task,
                           bool *ready,
                           QString *errorMessage);
bool requestCancellation(QProcess *process, QTimer *startTimer, QTimer *cancelTimer, QString *errorMessage);
void terminateWithGrace(QProcess *process, QObject *context);
} // namespace TrainingProtocol
