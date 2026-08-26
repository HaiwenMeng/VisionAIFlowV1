#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

class QThread;

namespace visionaiflow::app
{
struct YoloTrainingRequest final
{
    QString projectRoot;
    int epochs{0};
    int batchSize{0};
    double learningRate{0.0};
    bool horizontalFlip{true};
    QString resumeCheckpointPath;
};

class TrainingController final : public QObject
{
    Q_OBJECT

public:
    explicit TrainingController(QObject *parent = nullptr);
    ~TrainingController() override;
    void Start(const YoloTrainingRequest &request);
    void Cancel();
    [[nodiscard]] bool IsRunning() const noexcept;

signals:
    void Progress(int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double iou);
    void Completed(const QString &modelPath, const QString &bestCheckpointPath);
    void Failed(const QString &errorMessage);
    void Cancelled();
    void StateChanged(bool running);

private:
    QThread *m_thread{nullptr};
    std::shared_ptr<std::atomic_bool> m_cancel;
};
}
