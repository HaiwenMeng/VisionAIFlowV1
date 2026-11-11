#pragma once

#include <QProcess>
#include <QWidget>
#include <QTimer>
#include <QStringConverter>

namespace Ui
{
class YoloeTrainForm;
}

class YoloeTrainForm final : public QWidget
{
    Q_OBJECT

public:
    explicit YoloeTrainForm(QWidget *parent = nullptr);
    ~YoloeTrainForm() override;

    void setTaskName(const QString &taskName);
    void initialize();
    bool isIdle() const;

private slots:
    void on_PB_Start_clicked();
    void on_PB_Stop_clicked();
    void on_PB_ExportOnnx_clicked();
    void on_PB_OpenOutput_clicked();
    void readProcessOutput();
    void readProcessError();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);

private:
    enum class RunningOperation
    {
        None,
        Training,
        ExportOnnx
    };

    bool validateRequest(QString *errorMessage) const;
    bool validateExportRequest(QString *errorMessage) const;
    void appendLog(const QString &message);
    void handleEvent(const QByteArray &line);
    void setRunning(bool running);
    void updateTaskProgress(const QString &progress);
    void resetCharts();
    int effectiveImageSize() const;
    QString outputRoot() const;

    Ui::YoloeTrainForm *ui;
    QProcess *m_process;
    QString m_taskName;
    QByteArray m_pendingOutput;
    QStringDecoder m_errorDecoder{QStringDecoder::Utf8};
    RunningOperation m_runningOperation = RunningOperation::None;
    bool m_completeEventReceived = false;
    bool m_cancelled = false;
    bool m_protocolError = false;
    bool m_workerReady = false;
    bool m_cancelEventReceived = false;
    QString m_runId;
    int m_completedSegEpochs = 0;
    QTimer m_startTimer;
    QTimer m_cancelTimer;
};
