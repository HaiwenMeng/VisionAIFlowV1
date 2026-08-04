#include "visionaiflow/training/TensorDataLoader.h"

#include <QtTest>

class TensorDataLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void ProducesDeterministicShuffledBatches();
    void RejectsInvalidBatchSize();
};

void TensorDataLoaderTest::ProducesDeterministicShuffledBatches()
{
    const torch::Tensor features = torch::arange(12, torch::TensorOptions().dtype(torch::kFloat32)).reshape({6, 2});
    const torch::Tensor targets = torch::arange(6, torch::TensorOptions().dtype(torch::kInt64));
    const visionaiflow::training::DataLoaderOptions options{2, 123, true, false};
    const auto firstLoader = visionaiflow::training::TensorDataLoader::Create(features, targets, options);
    const auto secondLoader = visionaiflow::training::TensorDataLoader::Create(features, targets, options);
    QVERIFY(firstLoader.IsSuccess());
    QVERIFY(secondLoader.IsSuccess());
    auto first = firstLoader.Value();
    auto second = secondLoader.Value();
    const auto firstEpoch = first.NextEpoch();
    const auto secondEpoch = second.NextEpoch();
    QVERIFY(firstEpoch.IsSuccess());
    QVERIFY(secondEpoch.IsSuccess());
    QCOMPARE(static_cast<qsizetype>(firstEpoch.Value().size()), 3);
    for (size_t index = 0; index < firstEpoch.Value().size(); ++index)
    {
        QVERIFY(torch::equal(firstEpoch.Value()[index].features, secondEpoch.Value()[index].features));
        QVERIFY(torch::equal(firstEpoch.Value()[index].targets, secondEpoch.Value()[index].targets));
    }
}

void TensorDataLoaderTest::RejectsInvalidBatchSize()
{
    const auto result = visionaiflow::training::TensorDataLoader::Create(torch::ones({2, 2}), torch::zeros({2}, torch::kInt64), {0, 0, false, false});
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

QTEST_APPLESS_MAIN(TensorDataLoaderTest)

#include "tst_TensorDataLoader.moc"
