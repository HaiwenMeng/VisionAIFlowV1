#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QThread;

namespace visionaiflow::app
{
struct InferenceRequest final
{
    QString modelPath;
    QString imagePath;
    int classCount{0};
};

struct InferenceDetection final
{
    int classIndex{-1};
    float score{0.0F};
    float x1{0.0F};
    float y1{0.0F};
    float x2{0.0F};
    float y2{0.0F};
};

class InferenceController final : public QObject
{
    Q_OBJECT

public:
    explicit InferenceController(QObject *parent = nullptr);
    ~InferenceController() override;
    void Start(const InferenceRequest &request);
    [[nodiscard]] bool IsRunning() const noexcept;

signals:
    void Completed(const QVector<visionaiflow::app::InferenceDetection> &detections);
    void Failed(const QString &errorMessage);
    void StateChanged(bool running);

private:
    QThread *m_thread{nullptr};
};
}

Q_DECLARE_METATYPE(visionaiflow::app::InferenceDetection)
Q_DECLARE_METATYPE(QVector<visionaiflow::app::InferenceDetection>)
