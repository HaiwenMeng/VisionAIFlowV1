#pragma once

#include <QHash>
#include <QProcess>
#include <QWidget>

namespace Ui
{
class LineInferForm;
}

class LineInferForm final : public QWidget
{
    Q_OBJECT

public:
    explicit LineInferForm(QWidget *parent = nullptr);
    ~LineInferForm() override;
    void initialize(const QString &taskName);
    bool isIdle() const;

private:
    void addImages();
    void runInference();
    void readOutput();
    void handleLine(const QByteArray &line);
    void showImage();
    void reportError(const QString &errorMessage);
    void setRunning(bool running);

    Ui::LineInferForm *ui;
    QProcess *m_process;
    QString m_taskName;
    QString m_runId;
    QString m_runDir;
    QByteArray m_pending;
    QHash<int, QString> m_results;
    bool m_ready = false;
    bool m_finished = false;
    bool m_failed = false;
};
