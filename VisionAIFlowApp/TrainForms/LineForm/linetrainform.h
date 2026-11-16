#pragma once

#include <QProcess>
#include <QTimer>
#include <QWidget>

namespace Ui
{
class LineTrainForm;
}

class LineTrainForm final : public QWidget
{
    Q_OBJECT

public:
    explicit LineTrainForm(QWidget *parent = nullptr);
    ~LineTrainForm() override;
    void initialize(const QString &taskName);
    bool isIdle() const;

private:
    void startTraining();
    void stopTraining();
    void readOutput();
    void handleLine(const QByteArray &line);
    void finish(int exitCode, QProcess::ExitStatus status);
    void reportError(const QString &message);
    void setRunning(bool running);
    void setTaskProgress(const QString &progress);

    Ui::LineTrainForm *ui;
    QProcess *m_process;
    QTimer m_startTimer;
    QTimer m_cancelTimer;
    QString m_taskName;
    QString m_runId;
    QString m_runDir;
    QByteArray m_pending;
    bool m_ready = false;
    bool m_finished = false;
    bool m_failed = false;
    bool m_cancelled = false;
};
