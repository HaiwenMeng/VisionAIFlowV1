#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

class QThread;

struct DetectTrainingRequest
{
    QString taskName;
    int epochs{0};
    int batchSize{0};
    double learningRate{0.0};
    bool horizontalFlip{true};
    QString resumeCheckpointPath;
};

class DetectTrainingController final : public QObject
{
    Q_OBJECT

public:
    explicit DetectTrainingController(QObject *parent = nullptr);
    ~DetectTrainingController() override;

    void Start(const DetectTrainingRequest &request);
    void Cancel();
    bool IsRunning() const noexcept;

signals:
    void Progress(int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double meanIou);
    void Completed(const QString &runDirectory, const QString &modelPath, const QString &bestCheckpointPath);
    void Failed(const QString &errorMessage);
    void Cancelled();
    void StateChanged(bool running);

private:
    QThread *m_thread{nullptr};
    std::shared_ptr<std::atomic_bool> m_cancel;
};
