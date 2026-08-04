#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/tensor/GpuLease.h"
#include "visionaiflow/training/AsyncClassificationJob.h"
#include "visionaiflow/qt_foundation/HostRuntime.h"

#include <QObject>

#include <memory>

namespace visionaiflow::trainer_host
{
class TrainerJobCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit TrainerJobCoordinator(QObject *parent = nullptr);
    foundation::Result<void> Handle(const QCborMap &request, const qt_foundation::HostAsyncResponder &responder);

private:
    foundation::Result<void> StartSingleLabelJob(const QCborMap &request, const qt_foundation::HostAsyncResponder &responder);
    void ReleaseGpuLeaseAfterTerminalEvent();

    std::unique_ptr<training::AsyncClassificationJob> m_job;
    std::unique_ptr<tensor::GpuLease> m_gpuLease;
    QString m_jobId;
};
}
