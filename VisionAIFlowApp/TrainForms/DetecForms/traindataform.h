#pragma once

#include <QProcess>
#include <QTimer>
#include <QWidget>
#include <QJsonObject>
#include <QStringConverter>

namespace Ui
{
class TrainDataForm;
}

class TrainDataForm final : public QWidget
{
    Q_OBJECT

public:
    explicit TrainDataForm(QWidget *parent = nullptr);
    ~TrainDataForm() override;
    void toSetProcessName(const QString &setName);
    void toInitShow();
    bool isrunstate() const;

private:
    enum class Operation
    {
        None,
        Training,
        Export
    };
    void startTraining();
    void stopTraining();
    void exportOnnx();
    void readOutput();
    void readError();
    void processLine(const QByteArray &line);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void setRunning(bool running);
    void configureCharts();
    void logMessage(const QString &message);
    void updateProgress(const QString &progress);
    QString outputRoot() const;
    bool writeJson(const QString &path, const QJsonObject &value);

    Ui::TrainDataForm *ui;
    QProcess *m_process;
    QTimer m_startTimer;
    QTimer m_cancelTimer;
    QString m_taskName;
    QString m_task;
    QString m_runId;
    QString m_runDirectory;
    QByteArray m_pending;
    QStringDecoder m_errorDecoder{QStringDecoder::Utf8};
    Operation m_operation = Operation::None;
    bool m_ready = false;
    bool m_finished = false;
    bool m_cancelled = false;
    bool m_protocolError = false;
    bool m_cancelEvent = false;
};
