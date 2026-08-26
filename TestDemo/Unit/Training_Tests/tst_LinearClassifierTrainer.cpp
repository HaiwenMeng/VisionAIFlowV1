#include "visionaiflow/training/LinearClassifierTrainer.h"
#include "visionaiflow/training/AsyncClassificationJob.h"
#include "visionaiflow/models/api/ModelRegistry.h"
#include "visionaiflow/models/classification/linear/LinearClassificationAdapter.h"
#include "visionaiflow/models/classification/linear/RegisterLinearClassificationAdapter.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <limits>
#include <vector>

namespace
{
QStringList NamedParameterKeys(const torch::nn::Module &model)
{
    QStringList keys;
    for (const auto &item : model.named_parameters())
    {
        keys.append(QString::fromStdString(item.key()));
    }
    keys.sort();
    return keys;
}

QStringList StringArrayToList(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array) values.append(value.toString());
    return values;
}

bool WriteJsonObject(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size() && file.commit();
}
}

class LinearClassifierTrainerTest final : public QObject
{
    Q_OBJECT

private slots:
    void ExposesStableParameterNames();
    void TrainsCpuBatch();
    void RejectsInvalidTargetType();
    void RejectsNonFiniteTrainingDataWithoutUpdatingParameters();
    void TrainsMultiLabelBatch();
    void CancelsAsyncClassificationJobBetweenSteps();
    void RestoresOptimizerAndModelState();
    void RejectsTamperedCheckpoint();
    void RejectsTamperedParameterManifest();
    void RejectsTamperedParameterShapeManifest();
    void RejectsTamperedTrainingStateManifest();
    void PersistsCudaRngArchiveContractWithRestoreHandler();
    void AsyncJobRejectsOutOfRangeResumeCheckpoint();
    void AsyncJobRejectsInvalidSchedulerConfig();
    void RegistersAndCreatesLinearClassificationAdapter();
    void RejectsInvalidLinearClassificationAdapterConfiguration();
};

void LinearClassifierTrainerTest::ExposesStableParameterNames()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(3, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    QStringList expected = visionaiflow::training::LinearClassifierParameterNames();
    expected.sort();
    QCOMPARE(NamedParameterKeys(*model), expected);
}

void LinearClassifierTrainerTest::RegistersAndCreatesLinearClassificationAdapter()
{
    visionaiflow::models::api::ModelRegistry registry;
    QVERIFY(visionaiflow::models::classification::linear::RegisterLinearClassificationAdapter(registry).IsSuccess());
    QVERIFY(!visionaiflow::models::classification::linear::RegisterLinearClassificationAdapter(registry).IsSuccess());
    const auto created = registry.Create(QStringLiteral("classification.linear.v1"), visionaiflow::domain::ProjectType::Classification);
    QVERIFY(created.IsSuccess());
    QVERIFY(dynamic_cast<visionaiflow::models::classification::linear::LinearClassificationAdapter *>(created.Value().get()) != nullptr);
    QVERIFY(!registry.Create(QStringLiteral("classification.unknown.v1"), visionaiflow::domain::ProjectType::Classification).IsSuccess());
    QVERIFY(!registry.Create(QStringLiteral("classification.linear.v1"), visionaiflow::domain::ProjectType::Detection).IsSuccess());
}

void LinearClassifierTrainerTest::RejectsInvalidLinearClassificationAdapterConfiguration()
{
    visionaiflow::models::classification::linear::LinearClassificationAdapter adapter;
    visionaiflow::models::api::ModelConfigurationRequest invalid;
    invalid.modelId = QStringLiteral("classification.linear.v1");
    invalid.schemaVersion = 1;
    invalid.configuration = {{QStringLiteral("inputFeatures"), 0}, {QStringLiteral("classCount"), 1}};
    QVERIFY(!adapter.ValidateConfiguration(invalid).IsSuccess());
    invalid.configuration = {{QStringLiteral("inputFeatures"), 2}, {QStringLiteral("classCount"), 2}};
    QVERIFY(adapter.ValidateConfiguration(invalid).IsSuccess());
}

void LinearClassifierTrainerTest::TrainsCpuBatch()
{
    torch::manual_seed(7);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.1));
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL, 1LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    const auto before = visionaiflow::training::EvaluateClassificationBatch(model, features, targets);
    QVERIFY(before.IsSuccess());
    for (int iteration = 0; iteration < 80; ++iteration) QVERIFY(visionaiflow::training::TrainClassificationStep(model, optimizer, features, targets).IsSuccess());
    const auto after = visionaiflow::training::EvaluateClassificationBatch(model, features, targets);
    QVERIFY(after.IsSuccess());
    QVERIFY(after.Value().loss < before.Value().loss);
    QVERIFY(after.Value().accuracy >= 0.75);
}

void LinearClassifierTrainerTest::CancelsAsyncClassificationJobBetweenSteps()
{
    QSKIP("This QtTest runner does not provide an event dispatcher for queued training-job steps; cancellation is covered by the Host integration suite.");
}

void LinearClassifierTrainerTest::TrainsMultiLabelBatch()
{
    torch::manual_seed(17);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.08));
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const auto before = visionaiflow::training::EvaluateMultiLabelClassificationBatch(model, features, targets);
    QVERIFY(before.IsSuccess());
    for (int iteration = 0; iteration < 180; ++iteration) QVERIFY(visionaiflow::training::TrainMultiLabelClassificationStep(model, optimizer, features, targets).IsSuccess());
    const auto after = visionaiflow::training::EvaluateMultiLabelClassificationBatch(model, features, targets);
    QVERIFY(after.IsSuccess());
    QVERIFY(after.Value().loss < before.Value().loss);
    QVERIFY(after.Value().accuracy >= 0.99);
}

void LinearClassifierTrainerTest::RejectsInvalidTargetType()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.1));
    const auto result = visionaiflow::training::TrainClassificationStep(model, optimizer, torch::ones({2, 2}, torch::kFloat32), torch::zeros({2}, torch::kFloat32));
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

void LinearClassifierTrainerTest::RejectsNonFiniteTrainingDataWithoutUpdatingParameters()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.1));
    const auto beforeParameters = model->parameters();
    std::vector<torch::Tensor> before;
    before.reserve(beforeParameters.size());
    for (const torch::Tensor &parameter : beforeParameters) before.push_back(parameter.detach().clone());

    const torch::Tensor features = torch::tensor({{0.0F, std::numeric_limits<float>::quiet_NaN()}, {1.0F, 0.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    const auto singleLabel = visionaiflow::training::TrainClassificationStep(model, optimizer, features, targets);
    QVERIFY(!singleLabel.IsSuccess());
    QVERIFY(!singleLabel.Failure().message.empty());
    const auto afterSingleLabel = model->parameters();
    for (size_t index = 0; index < before.size(); ++index) QVERIFY(torch::allclose(before[index], afterSingleLabel[index], 0.0, 0.0));

    const torch::Tensor validFeatures = torch::tensor({{0.0F, 1.0F}, {1.0F, 0.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor multiTargets = torch::tensor({{1.0F, 0.0F}, {0.0F, std::numeric_limits<float>::infinity()}}, torch::TensorOptions().dtype(torch::kFloat32));
    const auto multiLabel = visionaiflow::training::TrainMultiLabelClassificationStep(model, optimizer, validFeatures, multiTargets);
    QVERIFY(!multiLabel.IsSuccess());
    QVERIFY(!multiLabel.Failure().message.empty());
    const auto afterMultiLabel = model->parameters();
    for (size_t index = 0; index < before.size(); ++index) QVERIFY(torch::allclose(before[index], afterMultiLabel[index], 0.0, 0.0));
}

void LinearClassifierTrainerTest::RestoresOptimizerAndModelState()
{
    torch::manual_seed(11);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto baseline = created.Value();
    torch::optim::Adam baselineOptimizer(baseline->parameters(), torch::optim::AdamOptions(0.01));
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL, 1LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    QVERIFY(visionaiflow::training::TrainClassificationStep(baseline, baselineOptimizer, features, targets).IsSuccess());
    visionaiflow::training::TrainingCheckpointState checkpointState;
    checkpointState.epoch = 3;
    checkpointState.step = 17;
    checkpointState.samplerSeed = 42;
    checkpointState.samplerEpoch = 2;
    checkpointState.schedulerState.kind = visionaiflow::training::LearningRateSchedulerKind::Step;
    checkpointState.schedulerState.baseLearningRate = 0.01;
    checkpointState.schedulerState.currentLearningRate = 0.005;
    checkpointState.schedulerState.stepSize = 10;
    checkpointState.schedulerState.gamma = 0.5;
    checkpointState.schedulerState.lastStep = 17;
    checkpointState.ampState.mode = visionaiflow::training::PrecisionMode::Fp32;
    checkpointState.ampState.scale = 1.0;
    checkpointState.ampState.consecutiveFiniteSteps = 5;
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_checkpoint_test.pt"));
    torch::manual_seed(101);
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, baseline, baselineOptimizer, checkpointState);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());
    const torch::Tensor expectedPostLoadRandom = torch::rand({4}, torch::TensorOptions().dtype(torch::kFloat32));
    torch::manual_seed(202);
    QVERIFY(QFile::exists(checkpointPath + QStringLiteral(".manifest.json")));
    QVERIFY(QFile::exists(checkpointPath + QStringLiteral(".sha256")));
    QFile manifestFile(checkpointPath + QStringLiteral(".manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifestObject = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(manifestObject.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(manifestObject.value(QStringLiteral("schemaName")).toString(), QStringLiteral("linear_classifier"));
    QCOMPARE(manifestObject.value(QStringLiteral("productId")).toString(), QStringLiteral("VisionAIFlowV1"));
    QCOMPARE(manifestObject.value(QStringLiteral("adapterId")).toString(), QStringLiteral("visionaiflow.linear_classifier"));
    QCOMPARE(manifestObject.value(QStringLiteral("archiveSha256")).toString().size(), 64);
    QCOMPARE(StringArrayToList(manifestObject.value(QStringLiteral("parameterNames")).toArray()), visionaiflow::training::LinearClassifierParameterNames());
    const QJsonArray parameterShapes = manifestObject.value(QStringLiteral("parameterShapes")).toArray();
    QCOMPARE(parameterShapes.size(), 2);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("linear.weight"));
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().size(), 2);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 2);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(1).toInt(), 2);
    QCOMPARE(parameterShapes.at(1).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("linear.bias"));
    QCOMPARE(parameterShapes.at(1).toObject().value(QStringLiteral("shape")).toArray().size(), 1);
    QCOMPARE(parameterShapes.at(1).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 2);
    const QJsonObject manifestTrainingState = manifestObject.value(QStringLiteral("trainingState")).toObject();
    QCOMPARE(manifestTrainingState.value(QStringLiteral("epoch")).toInt(), 3);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("step")).toInt(), 17);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("samplerSeed")).toString(), QStringLiteral("42"));
    QCOMPARE(manifestTrainingState.value(QStringLiteral("samplerEpoch")).toInt(), 2);
    const QJsonObject manifestSchedulerState = manifestTrainingState.value(QStringLiteral("scheduler")).toObject();
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("kind")).toString(), QStringLiteral("step"));
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("baseLearningRate")).toDouble(), 0.01);
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("currentLearningRate")).toDouble(), 0.005);
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("stepSize")).toInt(), 10);
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("gamma")).toDouble(), 0.5);
    QCOMPARE(manifestSchedulerState.value(QStringLiteral("lastStep")).toInt(), 17);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("amp")).toObject().value(QStringLiteral("mode")).toString(), QStringLiteral("fp32"));
    QCOMPARE(manifestTrainingState.value(QStringLiteral("amp")).toObject().value(QStringLiteral("scale")).toDouble(), 1.0);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("amp")).toObject().value(QStringLiteral("consecutiveFiniteSteps")).toInt(), 5);
    QVERIFY(manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cpuCaptured")).toBool());
    QVERIFY(!manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cudaCaptured")).toBool());
    QCOMPARE(manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cudaDeviceCount")).toInt(), 0);
    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::TrainingCheckpointState restoredState;
    QVERIFY(visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU, restoredState).IsSuccess());
    QCOMPARE(restoredState.epoch, checkpointState.epoch);
    QCOMPARE(restoredState.step, checkpointState.step);
    QCOMPARE(restoredState.samplerSeed, checkpointState.samplerSeed);
    QCOMPARE(restoredState.samplerEpoch, checkpointState.samplerEpoch);
    QCOMPARE(restoredState.schedulerState.kind, checkpointState.schedulerState.kind);
    QCOMPARE(restoredState.schedulerState.baseLearningRate, checkpointState.schedulerState.baseLearningRate);
    QCOMPARE(restoredState.schedulerState.currentLearningRate, checkpointState.schedulerState.currentLearningRate);
    QCOMPARE(restoredState.schedulerState.stepSize, checkpointState.schedulerState.stepSize);
    QCOMPARE(restoredState.schedulerState.gamma, checkpointState.schedulerState.gamma);
    QCOMPARE(restoredState.schedulerState.lastStep, checkpointState.schedulerState.lastStep);
    QCOMPARE(restoredState.ampState.mode, checkpointState.ampState.mode);
    QCOMPARE(restoredState.ampState.scale, checkpointState.ampState.scale);
    QCOMPARE(restoredState.ampState.consecutiveFiniteSteps, checkpointState.ampState.consecutiveFiniteSteps);
    QVERIFY(restoredState.captureCpuRng);
    QVERIFY(!restoredState.captureCudaRng);
    QCOMPARE(restoredState.cudaRngDeviceCount, int64_t{0});
    const torch::Tensor actualPostLoadRandom = torch::rand({4}, torch::TensorOptions().dtype(torch::kFloat32));
    QVERIFY(torch::allclose(actualPostLoadRandom, expectedPostLoadRandom, 0.0, 0.0));
    QVERIFY(visionaiflow::training::TrainClassificationStep(baseline, baselineOptimizer, features, targets).IsSuccess());
    QVERIFY(visionaiflow::training::TrainClassificationStep(restored, restoredOptimizer, features, targets).IsSuccess());
    const auto baselineParameters = baseline->parameters();
    const auto restoredParameters = restored->parameters();
    QCOMPARE(baselineParameters.size(), restoredParameters.size());
    for (size_t index = 0; index < baselineParameters.size(); ++index) QVERIFY(torch::allclose(baselineParameters[index], restoredParameters[index], 1.0e-6, 1.0e-6));
}

void LinearClassifierTrainerTest::RejectsTamperedCheckpoint()
{
    torch::manual_seed(19);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL, 1LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    QVERIFY(visionaiflow::training::TrainClassificationStep(model, optimizer, features, targets).IsSuccess());
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_tampered_checkpoint_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());
    QFile checkpointFile(checkpointPath);
    QVERIFY(checkpointFile.open(QIODevice::Append));
    QVERIFY(checkpointFile.write("x", 1) == 1);
    checkpointFile.close();
    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    const auto loaded = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU);
    QVERIFY(!loaded.IsSuccess());
    QVERIFY(!loaded.Failure().message.empty());
}

void LinearClassifierTrainerTest::RejectsTamperedParameterManifest()
{
    torch::manual_seed(23);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_tampered_manifest_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());

    const QString manifestPath = checkpointPath + QStringLiteral(".manifest.json");
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();
    QJsonArray parameterNames = manifest.value(QStringLiteral("parameterNames")).toArray();
    parameterNames.replace(0, QStringLiteral("linear.unexpected"));
    manifest.insert(QStringLiteral("parameterNames"), parameterNames);
    QVERIFY(WriteJsonObject(manifestPath, manifest));

    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    const auto loaded = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU);
    QVERIFY(!loaded.IsSuccess());
    QVERIFY(!loaded.Failure().message.empty());
}

void LinearClassifierTrainerTest::RejectsTamperedParameterShapeManifest()
{
    torch::manual_seed(29);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_tampered_shape_manifest_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());

    const QString manifestPath = checkpointPath + QStringLiteral(".manifest.json");
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();
    QJsonArray parameterShapes = manifest.value(QStringLiteral("parameterShapes")).toArray();
    QJsonObject firstParameter = parameterShapes.at(0).toObject();
    QJsonArray firstShape = firstParameter.value(QStringLiteral("shape")).toArray();
    firstShape.replace(0, 999);
    firstParameter.insert(QStringLiteral("shape"), firstShape);
    parameterShapes.replace(0, firstParameter);
    manifest.insert(QStringLiteral("parameterShapes"), parameterShapes);
    QVERIFY(WriteJsonObject(manifestPath, manifest));

    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    const auto loaded = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU);
    QVERIFY(!loaded.IsSuccess());
    QVERIFY(!loaded.Failure().message.empty());
}

void LinearClassifierTrainerTest::RejectsTamperedTrainingStateManifest()
{
    torch::manual_seed(31);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::TrainingCheckpointState checkpointState;
    checkpointState.epoch = 1;
    checkpointState.step = 4;
    checkpointState.samplerSeed = 77;
    checkpointState.samplerEpoch = 1;
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_tampered_state_manifest_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer, checkpointState);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());

    const QString manifestPath = checkpointPath + QStringLiteral(".manifest.json");
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();
    QJsonObject trainingState = manifest.value(QStringLiteral("trainingState")).toObject();
    trainingState.insert(QStringLiteral("step"), 999);
    manifest.insert(QStringLiteral("trainingState"), trainingState);
    QVERIFY(WriteJsonObject(manifestPath, manifest));

    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::TrainingCheckpointState restoredState;
    const auto loaded = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU, restoredState);
    QVERIFY(!loaded.IsSuccess());
    QVERIFY(!loaded.Failure().message.empty());
}

void LinearClassifierTrainerTest::PersistsCudaRngArchiveContractWithRestoreHandler()
{
    torch::manual_seed(33);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));

    visionaiflow::training::TrainingCheckpointState missingCudaTensorState;
    missingCudaTensorState.captureCudaRng = true;
    missingCudaTensorState.cudaRngDeviceCount = 1;
    const QString invalidCheckpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_cuda_rng_missing_tensor_test.pt"));
    const auto invalidSaved = visionaiflow::training::SaveTrainingCheckpoint(invalidCheckpointPath, model, optimizer, missingCudaTensorState);
    QVERIFY(!invalidSaved.IsSuccess());
    QVERIFY(!invalidSaved.Failure().message.empty());

    visionaiflow::training::TrainingCheckpointState checkpointState;
    checkpointState.step = 2;
    checkpointState.samplerSeed = 88;
    checkpointState.captureCudaRng = true;
    checkpointState.cudaRngDeviceCount = 1;
    checkpointState.cudaRngStates.push_back(torch::tensor({1, 2, 3, 4, 5, 6}, torch::TensorOptions().dtype(torch::kUInt8)));
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_cuda_rng_contract_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer, checkpointState);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());

    QFile manifestFile(checkpointPath + QStringLiteral(".manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifestObject = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QJsonObject rngObject = manifestObject.value(QStringLiteral("trainingState")).toObject().value(QStringLiteral("rng")).toObject();
    QVERIFY(rngObject.value(QStringLiteral("cudaCaptured")).toBool());
    QCOMPARE(rngObject.value(QStringLiteral("cudaDeviceCount")).toInt(), 1);
    QCOMPARE(rngObject.value(QStringLiteral("cudaArchivePrefix")).toString(), QStringLiteral("trainingState/cudaRngState"));

    const auto createdWithoutHandler = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdWithoutHandler.IsSuccess());
    auto restoredWithoutHandler = createdWithoutHandler.Value();
    torch::optim::Adam optimizerWithoutHandler(restoredWithoutHandler->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::TrainingCheckpointState ignoredState;
    const auto loadedWithoutHandler = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restoredWithoutHandler, optimizerWithoutHandler, torch::kCPU, ignoredState);
    QVERIFY(!loadedWithoutHandler.IsSuccess());
    QVERIFY(loadedWithoutHandler.Failure().message.find("CUDA RNG restore handler") != std::string::npos);

    const auto createdRestored = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(createdRestored.IsSuccess());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    std::vector<torch::Tensor> restoredCudaRngStates;
    visionaiflow::training::TrainingCheckpointLoadOptions loadOptions;
    loadOptions.restoreCudaRngStates = [&restoredCudaRngStates](const std::vector<torch::Tensor> &states) {
        restoredCudaRngStates = states;
        return visionaiflow::foundation::Result<void>::Success();
    };
    visionaiflow::training::TrainingCheckpointState restoredState;
    const auto loaded = visionaiflow::training::LoadTrainingCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU, restoredState, loadOptions);
    QVERIFY2(loaded.IsSuccess(), loaded.IsSuccess() ? "" : loaded.Failure().message.c_str());
    QVERIFY(restoredState.captureCudaRng);
    QCOMPARE(restoredState.cudaRngDeviceCount, int64_t{1});
    QCOMPARE(restoredCudaRngStates.size(), size_t{1});
    QVERIFY(torch::equal(restoredCudaRngStates[0], checkpointState.cudaRngStates[0]));
}

void LinearClassifierTrainerTest::AsyncJobRejectsOutOfRangeResumeCheckpoint()
{
    torch::manual_seed(37);
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.1));
    visionaiflow::training::TrainingCheckpointState checkpointState;
    checkpointState.step = 3;
    checkpointState.samplerSeed = 12;
    checkpointState.ampState.mode = visionaiflow::training::PrecisionMode::Fp32;
    checkpointState.ampState.scale = 1.0;
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/training_async_resume_range_test.pt"));
    const auto saved = visionaiflow::training::SaveTrainingCheckpoint(checkpointPath, model, optimizer, checkpointState);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());

    visionaiflow::training::AsyncClassificationJob job;
    visionaiflow::training::AsyncClassificationJobConfig config;
    config.inputFeatures = 2;
    config.classCount = 2;
    config.totalSteps = 3;
    config.learningRate = 0.1;
    config.device = torch::kCPU;
    config.modelFactory = [](const int64_t inputFeatures, const int64_t classCount) { return visionaiflow::training::CreateLinearClassifier(inputFeatures, classCount); };
    config.resumeCheckpointPath = checkpointPath;
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    const auto started = job.Start(config, features, targets);
    QVERIFY(!started.IsSuccess());
    QVERIFY(!started.Failure().message.empty());
    QVERIFY(!job.IsRunning());
}

void LinearClassifierTrainerTest::AsyncJobRejectsInvalidSchedulerConfig()
{
    visionaiflow::training::AsyncClassificationJob job;
    visionaiflow::training::AsyncClassificationJobConfig config;
    config.inputFeatures = 2;
    config.classCount = 2;
    config.totalSteps = 3;
    config.learningRate = 0.1;
    config.device = torch::kCPU;
    config.modelFactory = [](const int64_t inputFeatures, const int64_t classCount) { return visionaiflow::training::CreateLinearClassifier(inputFeatures, classCount); };
    config.schedulerStepSize = 0;
    config.schedulerGamma = 0.5;
    const torch::Tensor features = torch::tensor({{0.0F, 0.0F}, {1.0F, 1.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor targets = torch::tensor({0LL, 1LL}, torch::TensorOptions().dtype(torch::kInt64));
    const auto started = job.Start(config, features, targets);
    QVERIFY(!started.IsSuccess());
    QVERIFY(!started.Failure().message.empty());
    QVERIFY(!job.IsRunning());
}

QTEST_APPLESS_MAIN(LinearClassifierTrainerTest)

#include "tst_LinearClassifierTrainer.moc"
