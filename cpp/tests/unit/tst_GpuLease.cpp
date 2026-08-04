#include "visionaiflow/tensor/GpuLease.h"

#include <QtTest>
#include <QUuid>

class GpuLeaseTest final : public QObject
{
    Q_OBJECT

private slots:
    void ExcludesConcurrentLeaseAndReleases();
};

void GpuLeaseTest::ExcludesConcurrentLeaseAndReleases()
{
    const QString gpuUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    visionaiflow::tensor::GpuLease first;
    visionaiflow::tensor::GpuLease second;
    QVERIFY(first.Acquire(gpuUuid).IsSuccess());
    const auto duplicate = second.Acquire(gpuUuid);
    QVERIFY(!duplicate.IsSuccess());
    QVERIFY(!duplicate.Failure().message.empty());
    QVERIFY(first.Release().IsSuccess());
    QVERIFY(second.Acquire(gpuUuid).IsSuccess());
    QVERIFY(second.Release().IsSuccess());
}

QTEST_APPLESS_MAIN(GpuLeaseTest)

#include "tst_GpuLease.moc"
