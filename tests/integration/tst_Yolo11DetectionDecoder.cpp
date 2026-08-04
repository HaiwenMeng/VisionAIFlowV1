#include "visionaiflow/export/OnnxExporter.h"
#include "visionaiflow/export/ModelPackage.h"
#include "visionaiflow/models/yolo11/Yolo11DetectionDecoder.h"
#include "visionaiflow/training/Yolo11DetectionTraining.h"

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
struct ParsedDetection final
{
    int classIndex{-1};
    float score{0.0F};
    float x1{0.0F};
    float y1{0.0F};
    float x2{0.0F};
    float y2{0.0F};
};

struct HostRunResult final
{
    bool ok{false};
    std::vector<float> rawHead;
    std::vector<ParsedDetection> detections;
    QString stdoutText;
    QString errorMessage;
};

QString NativeExecutablePath(const QString &fileName)
{
    QString executableName = fileName;
#ifdef Q_OS_WIN
    if (!executableName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) executableName.append(QStringLiteral(".exe"));
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(executableName);
}

std::vector<ParsedDetection> ParseDetections(const QByteArray &output)
{
    const QRegularExpression expression(QStringLiteral("detections:\\s*([^\\r\\n]*)"));
    const QRegularExpressionMatch match = expression.match(QString::fromLocal8Bit(output));
    if (!match.hasMatch()) return {};
    std::vector<ParsedDetection> detections;
    const QStringList parts = match.captured(1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    detections.reserve(static_cast<size_t>(parts.size()));
    for (const QString &part : parts)
    {
        const QStringList fields = part.split(QLatin1Char(','));
        if (fields.size() != 6) return {};
        bool ok = false;
        ParsedDetection detection;
        detection.classIndex = fields.at(0).toInt(&ok);
        if (!ok) return {};
        detection.score = fields.at(1).toFloat(&ok);
        if (!ok) return {};
        detection.x1 = fields.at(2).toFloat(&ok);
        if (!ok) return {};
        detection.y1 = fields.at(3).toFloat(&ok);
        if (!ok) return {};
        detection.x2 = fields.at(4).toFloat(&ok);
        if (!ok) return {};
        detection.y2 = fields.at(5).toFloat(&ok);
        if (!ok) return {};
        detections.push_back(detection);
    }
    return detections;
}

std::vector<float> ParseRawHead(const QByteArray &output)
{
    const QRegularExpression expression(QStringLiteral("raw:\\s*([^\\r\\n]*)"));
    const QRegularExpressionMatch match = expression.match(QString::fromLocal8Bit(output));
    if (!match.hasMatch()) return {};
    std::vector<float> values;
    const QStringList parts = match.captured(1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    values.reserve(static_cast<size_t>(parts.size()));
    for (const QString &part : parts)
    {
        bool ok = false;
        const float value = part.toFloat(&ok);
        if (!ok) return {};
        values.push_back(value);
    }
    return values;
}

HostRunResult RunHostYolo11Inference(const QString &program, const QString &onnxPath, const QStringList &pathPrefixes)
{
    if (!QFile::exists(program)) return {false, {}, {}, {}, QStringLiteral("Required YOLO11 host is missing: ") + program};
    const QString traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stdoutPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11-host-stdout-") + traceId + QStringLiteral(".txt"));
    const QString stderrPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11-host-stderr-") + traceId + QStringLiteral(".txt"));
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), pathPrefixes.join(QStringLiteral(";")) + QStringLiteral(";") + environment.value(QStringLiteral("PATH")));
    environment.insert(QStringLiteral("VISIONAIFLOW_ENGINE_CACHE_ROOT"), QDir::current().filePath(QStringLiteral("out/qmake/Release/engine-cache-tests/yolo11")));
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments(QStringList{QStringLiteral("--infer-yolo11-onnx"), onnxPath});
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(stdoutPath);
    process.setStandardErrorFile(stderrPath);
    process.start();
    if (!process.waitForStarted(10000)) return {false, {}, {}, {}, QStringLiteral("YOLO11 host could not start: ") + process.errorString()};
    if (!process.waitForFinished(90000)) return {false, {}, {}, {}, QStringLiteral("YOLO11 host timed out: ") + program};
    QFile outputFile(stdoutPath);
    QFile errorFile(stderrPath);
    const QByteArray output = outputFile.open(QIODevice::ReadOnly) ? outputFile.readAll() : QByteArray();
    const QByteArray error = errorFile.open(QIODevice::ReadOnly) ? errorFile.readAll() : QByteArray();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {false, {}, {}, {}, QStringLiteral("YOLO11 host failed: ") + program + QStringLiteral(" stdout=") + QString::fromLocal8Bit(output) + QStringLiteral(" stderr=") + QString::fromLocal8Bit(error)};
    const std::vector<float> rawHead = ParseRawHead(output);
    if (rawHead.empty()) return {false, {}, {}, {}, QStringLiteral("YOLO11 host did not print raw head: ") + QString::fromLocal8Bit(output)};
    const std::vector<ParsedDetection> detections = ParseDetections(output);
    if (detections.empty()) return {false, {}, {}, {}, QStringLiteral("YOLO11 host did not print detections: ") + QString::fromLocal8Bit(output)};
    return {true, rawHead, detections, QString::fromLocal8Bit(output), {}};
}

bool HasYolo11EngineCacheManifest()
{
    QDirIterator iterator(QDir::current().filePath(QStringLiteral("out/qmake/Release/engine-cache-tests/yolo11")), QStringList{QStringLiteral("manifest.json")}, QDir::Files, QDirIterator::Subdirectories);
    return iterator.hasNext();
}

void VerifyDetection(const ParsedDetection &actual, const int expectedClass, const float expectedScore, const float expectedX1, const float expectedY1, const float expectedX2, const float expectedY2)
{
    QCOMPARE(actual.classIndex, expectedClass);
    QVERIFY(std::fabs(actual.score - expectedScore) <= 1.0e-4F);
    QVERIFY(std::fabs(actual.x1 - expectedX1) <= 1.0e-4F);
    QVERIFY(std::fabs(actual.y1 - expectedY1) <= 1.0e-4F);
    QVERIFY(std::fabs(actual.x2 - expectedX2) <= 1.0e-4F);
    QVERIFY(std::fabs(actual.y2 - expectedY2) <= 1.0e-4F);
}

void VerifyRawHead(const std::vector<float> &actual, const std::vector<float> &expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) QVERIFY(std::fabs(actual[index] - expected[index]) <= 1.0e-4F);
}

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

class Yolo11DetectionDecoderTest final : public QObject
{
    Q_OBJECT

private slots:
    void ParameterNamesMatchModuleState();
    void DecodesBoxesAndRunsPerClassNms();
    void ClassAgnosticNmsSuppressesAcrossClasses();
    void AllowsValidEmptyDetections();
    void RejectsInvalidRawContract();
    void RestoresLetterboxedCoordinates();
    void CreatesDetectionOverlayItems();
    void AssignsTargetsAndComputesDetectionLoss();
    void RejectsInvalidTrainingContracts();
    void HorizontallyFlipsDetectionSample();
    void EvaluatesDetectionMetrics();
    void TinyDetectorForwardAndTrainingStep();
    void GridDetectorForwardAndTrainingStep();
    void TinyDetectorCheckpointRestoresContinuousTraining();
    void GridDetectorCheckpointRestoresContinuousTraining();
    void CheckpointRejectsTamperedArchive();
    void CreatesYolo11DetectionModelPackage();
    void ExportedHeadRunsInOpenVinoAndTensorRt();
};

void Yolo11DetectionDecoderTest::ParameterNamesMatchModuleState()
{
    const auto tinyCreated = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(tinyCreated.IsSuccess(), tinyCreated.IsSuccess() ? "" : tinyCreated.Failure().message.c_str());
    QStringList tinyExpected = visionaiflow::training::Yolo11TinyDetectorParameterNames();
    tinyExpected.sort();
    QCOMPARE(NamedParameterKeys(*tinyCreated.Value()), tinyExpected);

    const auto gridCreated = visionaiflow::training::CreateYolo11GridDetector(3, 2);
    QVERIFY2(gridCreated.IsSuccess(), gridCreated.IsSuccess() ? "" : gridCreated.Failure().message.c_str());
    QStringList gridExpected = visionaiflow::training::Yolo11GridDetectorParameterNames();
    gridExpected.sort();
    QCOMPARE(NamedParameterKeys(*gridCreated.Value()), gridExpected);
}

void Yolo11DetectionDecoderTest::DecodesBoxesAndRunsPerClassNms()
{
    const std::vector<float> raw{
        50.0F, 50.0F, 40.0F, 40.0F, 0.90F, 0.10F,
        52.0F, 52.0F, 40.0F, 40.0F, 0.80F, 0.05F,
        52.0F, 52.0F, 40.0F, 40.0F, 0.05F, 0.85F,
        10.0F, 10.0F, 10.0F, 10.0F, 0.10F, 0.20F};
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    config.scoreThreshold = 0.25F;
    config.nmsIouThreshold = 0.50F;
    const auto decoded = visionaiflow::models::yolo11::DecodeYolo11Detections(raw, 4, 2, 100.0F, 100.0F, config);
    QVERIFY2(decoded.IsSuccess(), decoded.IsSuccess() ? "" : decoded.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(decoded.Value().size()), 2);
    QCOMPARE(decoded.Value().at(0).classIndex, 0);
    QCOMPARE(decoded.Value().at(0).score, 0.90F);
    QCOMPARE(decoded.Value().at(0).box.x1, 30.0F);
    QCOMPARE(decoded.Value().at(0).box.y1, 30.0F);
    QCOMPARE(decoded.Value().at(0).box.x2, 70.0F);
    QCOMPARE(decoded.Value().at(0).box.y2, 70.0F);
    QCOMPARE(decoded.Value().at(1).classIndex, 1);
    QCOMPARE(decoded.Value().at(1).score, 0.85F);
}

void Yolo11DetectionDecoderTest::ClassAgnosticNmsSuppressesAcrossClasses()
{
    const std::vector<float> raw{
        50.0F, 50.0F, 40.0F, 40.0F, 0.90F, 0.10F,
        52.0F, 52.0F, 40.0F, 40.0F, 0.05F, 0.85F};
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    config.scoreThreshold = 0.25F;
    config.nmsIouThreshold = 0.50F;
    config.classAgnosticNms = true;
    const auto decoded = visionaiflow::models::yolo11::DecodeYolo11Detections(raw, 2, 2, 100.0F, 100.0F, config);
    QVERIFY2(decoded.IsSuccess(), decoded.IsSuccess() ? "" : decoded.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(decoded.Value().size()), 1);
    QCOMPARE(decoded.Value().front().classIndex, 0);
}

void Yolo11DetectionDecoderTest::AllowsValidEmptyDetections()
{
    const std::vector<float> raw{
        50.0F, 50.0F, 40.0F, 40.0F, 0.10F, 0.20F,
        80.0F, 80.0F, 10.0F, 10.0F, 0.05F, 0.15F};
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    config.scoreThreshold = 0.25F;
    const auto decoded = visionaiflow::models::yolo11::DecodeYolo11Detections(raw, 2, 2, 100.0F, 100.0F, config);
    QVERIFY2(decoded.IsSuccess(), decoded.IsSuccess() ? "" : decoded.Failure().message.c_str());
    QVERIFY(decoded.Value().empty());
}

void Yolo11DetectionDecoderTest::RejectsInvalidRawContract()
{
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    const auto badSize = visionaiflow::models::yolo11::DecodeYolo11Detections({1.0F, 2.0F}, 1, 2, 100.0F, 100.0F, config);
    QVERIFY(!badSize.IsSuccess());
    QVERIFY(!badSize.Failure().message.empty());
    const auto badImage = visionaiflow::models::yolo11::DecodeYolo11Detections({}, 0, 1, 0.0F, 100.0F, config);
    QVERIFY(!badImage.IsSuccess());
    QVERIFY(!badImage.Failure().message.empty());
    const std::vector<float> nanRow{50.0F, 50.0F, 40.0F, 40.0F, std::numeric_limits<float>::quiet_NaN()};
    const auto badValue = visionaiflow::models::yolo11::DecodeYolo11Detections(nanRow, 1, 1, 100.0F, 100.0F, config);
    QVERIFY(!badValue.IsSuccess());
    QVERIFY(!badValue.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::RestoresLetterboxedCoordinates()
{
    const auto geometry = visionaiflow::models::yolo11::CreateYolo11LetterboxGeometry(100.0F, 50.0F, 200.0F, 200.0F, true);
    QVERIFY2(geometry.IsSuccess(), geometry.IsSuccess() ? "" : geometry.Failure().message.c_str());
    QCOMPARE(geometry.Value().scale, 2.0F);
    QCOMPARE(geometry.Value().padX, 0.0F);
    QCOMPARE(geometry.Value().padY, 50.0F);
    const std::vector<float> raw{100.0F, 100.0F, 40.0F, 20.0F, 0.90F};
    visionaiflow::models::yolo11::Yolo11DetectionDecodeConfig config;
    config.scoreThreshold = 0.25F;
    const auto decoded = visionaiflow::models::yolo11::DecodeYolo11DetectionsFromLetterbox(raw, 1, 1, geometry.Value(), config);
    QVERIFY2(decoded.IsSuccess(), decoded.IsSuccess() ? "" : decoded.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(decoded.Value().size()), 1);
    QCOMPARE(decoded.Value().front().box.x1, 40.0F);
    QCOMPARE(decoded.Value().front().box.y1, 20.0F);
    QCOMPARE(decoded.Value().front().box.x2, 60.0F);
    QCOMPARE(decoded.Value().front().box.y2, 30.0F);
    QCOMPARE(decoded.Value().front().classIndex, 0);
    QCOMPARE(decoded.Value().front().score, 0.90F);
    const auto invalidGeometry = visionaiflow::models::yolo11::CreateYolo11LetterboxGeometry(0.0F, 50.0F, 200.0F, 200.0F, true);
    QVERIFY(!invalidGeometry.IsSuccess());
    QVERIFY(!invalidGeometry.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::CreatesDetectionOverlayItems()
{
    const std::vector<visionaiflow::models::common::Detection> detections{{{10.0F, 20.0F, 30.0F, 40.0F}, 1, 0.875F}};
    const auto overlay = visionaiflow::models::common::CreateDetectionOverlayItems(detections, {"scratch", "crack"}, 100.0F, 80.0F);
    QVERIFY2(overlay.IsSuccess(), overlay.IsSuccess() ? "" : overlay.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(overlay.Value().size()), 1);
    QCOMPARE(overlay.Value().front().classIndex, 1);
    QCOMPARE(overlay.Value().front().box.x1, 10.0F);
    QCOMPARE(overlay.Value().front().box.y1, 20.0F);
    QCOMPARE(overlay.Value().front().caption, std::string("crack 0.875"));
    const auto outOfBounds = visionaiflow::models::common::CreateDetectionOverlayItems({{{10.0F, 20.0F, 130.0F, 40.0F}, 0, 0.5F}}, {"scratch"}, 100.0F, 80.0F);
    QVERIFY(!outOfBounds.IsSuccess());
    QVERIFY(!outOfBounds.Failure().message.empty());
    const auto badClass = visionaiflow::models::common::CreateDetectionOverlayItems(detections, {"scratch"}, 100.0F, 80.0F);
    QVERIFY(!badClass.IsSuccess());
    QVERIFY(!badClass.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::AssignsTargetsAndComputesDetectionLoss()
{
    const std::vector<visionaiflow::models::common::DetectionBox> candidates{{30.0F, 30.0F, 70.0F, 70.0F}, {32.0F, 32.0F, 72.0F, 72.0F}, {75.0F, 75.0F, 85.0F, 85.0F}, {0.0F, 0.0F, 10.0F, 10.0F}};
    const std::vector<visionaiflow::training::Yolo11GroundTruthDetection> targets{{{30.0F, 30.0F, 70.0F, 70.0F}, 0}, {{75.0F, 75.0F, 85.0F, 85.0F}, 1}};
    visionaiflow::training::Yolo11AssignmentConfig assignmentConfig;
    assignmentConfig.positiveIouThreshold = 0.80F;
    const auto assigned = visionaiflow::training::AssignYolo11DetectionTargets(candidates, targets, 2, assignmentConfig);
    QVERIFY2(assigned.IsSuccess(), assigned.IsSuccess() ? "" : assigned.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(assigned.Value().size()), 4);
    QVERIFY(assigned.Value().at(0).positive);
    QCOMPARE(assigned.Value().at(0).classIndex, 0);
    QVERIFY(assigned.Value().at(1).positive);
    QVERIFY(assigned.Value().at(2).positive);
    QCOMPARE(assigned.Value().at(2).classIndex, 1);
    QVERIFY(!assigned.Value().at(3).positive);
    const torch::Tensor raw = torch::tensor({{30.0F, 30.0F, 70.0F, 70.0F, 0.90F, 0.10F}, {32.0F, 32.0F, 72.0F, 72.0F, 0.80F, 0.10F}, {75.0F, 75.0F, 85.0F, 85.0F, 0.10F, 0.70F}, {0.0F, 0.0F, 10.0F, 10.0F, 0.0F, 0.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const auto loss = visionaiflow::training::ComputeYolo11DetectionLoss(raw, assigned.Value(), 2, {});
    QVERIFY2(loss.IsSuccess(), loss.IsSuccess() ? "" : loss.Failure().message.c_str());
    QCOMPARE(loss.Value().positiveRows, 3);
    QCOMPARE(loss.Value().assignedGroundTruthCount, 2);
    QVERIFY(loss.Value().boxLoss > 0.0);
    QVERIFY(loss.Value().boxLoss < 1.0);
    QVERIFY(std::fabs(loss.Value().classLoss - 0.02125) <= 1.0e-6);
    QVERIFY(loss.Value().meanPositiveIou > 0.90);
    QVERIFY(torch::isfinite(loss.Value().totalLoss).all().item<bool>());
}

void Yolo11DetectionDecoderTest::RejectsInvalidTrainingContracts()
{
    const std::vector<visionaiflow::models::common::DetectionBox> candidates{{0.0F, 0.0F, 10.0F, 10.0F}};
    const std::vector<visionaiflow::training::Yolo11GroundTruthDetection> badClass{{{0.0F, 0.0F, 10.0F, 10.0F}, 3}};
    const auto assigned = visionaiflow::training::AssignYolo11DetectionTargets(candidates, badClass, 2, {});
    QVERIFY(!assigned.IsSuccess());
    QVERIFY(!assigned.Failure().message.empty());
    const std::vector<visionaiflow::training::Yolo11AssignedTarget> oneAssignment{{true, 0, 0, {0.0F, 0.0F, 10.0F, 10.0F}}};
    const auto badShape = visionaiflow::training::ComputeYolo11DetectionLoss(torch::zeros({1, 5}, torch::kFloat32), oneAssignment, 2, {});
    QVERIFY(!badShape.IsSuccess());
    QVERIFY(!badShape.Failure().message.empty());
    const auto badType = visionaiflow::training::ComputeYolo11DetectionLoss(torch::zeros({1, 6}, torch::kFloat64), oneAssignment, 2, {});
    QVERIFY(!badType.IsSuccess());
    QVERIFY(!badType.Failure().message.empty());
    const torch::Tensor badValue = torch::tensor({{0.0F, 0.0F, 10.0F, 10.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F}}, torch::TensorOptions().dtype(torch::kFloat32));
    const auto nonFinite = visionaiflow::training::ComputeYolo11DetectionLoss(badValue, oneAssignment, 2, {});
    QVERIFY(!nonFinite.IsSuccess());
    QVERIFY(!nonFinite.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::HorizontallyFlipsDetectionSample()
{
    const torch::Tensor image = torch::arange(0, 12, torch::TensorOptions().dtype(torch::kFloat32)).view({1, 3, 4});
    const std::vector<visionaiflow::training::Yolo11GroundTruthDetection> targets{{{0.5F, 0.0F, 2.0F, 2.0F}, 0}, {{2.5F, 1.0F, 3.5F, 3.0F}, 1}};
    const auto flipped = visionaiflow::training::FlipYolo11DetectionSampleHorizontally(image, targets, 2, 4.0F);
    QVERIFY2(flipped.IsSuccess(), flipped.IsSuccess() ? "" : flipped.Failure().message.c_str());
    const torch::Tensor expectedImage = torch::tensor({{{3.0F, 2.0F, 1.0F, 0.0F}, {7.0F, 6.0F, 5.0F, 4.0F}, {11.0F, 10.0F, 9.0F, 8.0F}}}, torch::TensorOptions().dtype(torch::kFloat32));
    QVERIFY(torch::equal(flipped.Value().image, expectedImage));
    QCOMPARE(static_cast<qsizetype>(flipped.Value().targets.size()), 2);
    QCOMPARE(flipped.Value().targets.at(0).classIndex, 0);
    QCOMPARE(flipped.Value().targets.at(0).box.x1, 2.0F);
    QCOMPARE(flipped.Value().targets.at(0).box.x2, 3.5F);
    QCOMPARE(flipped.Value().targets.at(1).classIndex, 1);
    QCOMPARE(flipped.Value().targets.at(1).box.x1, 0.5F);
    QCOMPARE(flipped.Value().targets.at(1).box.x2, 1.5F);
    const auto bad = visionaiflow::training::FlipYolo11DetectionSampleHorizontally(image, {{{0.0F, 0.0F, 1.0F, 1.0F}, 3}}, 2, 4.0F);
    QVERIFY(!bad.IsSuccess());
    QVERIFY(!bad.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::EvaluatesDetectionMetrics()
{
    const std::vector<visionaiflow::models::common::Detection> predictions{{{0.0F, 0.0F, 10.0F, 10.0F}, 0, 0.90F}, {{0.0F, 0.0F, 10.0F, 10.0F}, 0, 0.80F}, {{20.0F, 20.0F, 30.0F, 30.0F}, 1, 0.70F}, {{90.0F, 90.0F, 95.0F, 95.0F}, 1, 0.10F}};
    const std::vector<visionaiflow::training::Yolo11GroundTruthDetection> groundTruth{{{0.0F, 0.0F, 10.0F, 10.0F}, 0}, {{20.0F, 20.0F, 30.0F, 30.0F}, 1}, {{40.0F, 40.0F, 50.0F, 50.0F}, 0}};
    visionaiflow::training::Yolo11DetectionMetricsConfig config;
    config.iouThreshold = 0.50F;
    config.scoreThreshold = 0.25F;
    const auto metrics = visionaiflow::training::EvaluateYolo11DetectionMetrics(predictions, groundTruth, 2, config);
    QVERIFY2(metrics.IsSuccess(), metrics.IsSuccess() ? "" : metrics.Failure().message.c_str());
    QCOMPARE(metrics.Value().truePositive, 2);
    QCOMPARE(metrics.Value().falsePositive, 1);
    QCOMPARE(metrics.Value().falseNegative, 1);
    QVERIFY(std::fabs(metrics.Value().precision - (2.0 / 3.0)) <= 1.0e-9);
    QVERIFY(std::fabs(metrics.Value().recall - (2.0 / 3.0)) <= 1.0e-9);
    QCOMPARE(metrics.Value().meanMatchedIou, 1.0);
    const auto bad = visionaiflow::training::EvaluateYolo11DetectionMetrics({{{0.0F, 0.0F, 10.0F, 10.0F}, 5, 0.9F}}, groundTruth, 2, config);
    QVERIFY(!bad.IsSuccess());
    QVERIFY(!bad.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::TinyDetectorForwardAndTrainingStep()
{
    torch::manual_seed(23);
    const auto created = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto model = created.Value();
    const torch::Tensor image = torch::ones({1, 3, 16, 16}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor firstRaw = model->forward(image);
    QCOMPARE(firstRaw.sizes(), c10::IntArrayRef({1, 2, 6}));
    const std::vector<visionaiflow::training::Yolo11AssignedTarget> assignments{{true, 0, 0, {0.30F, 0.30F, 0.70F, 0.70F}}, {true, 1, 1, {0.75F, 0.75F, 0.85F, 0.85F}}};
    const visionaiflow::training::Yolo11DetectionBatch batch{image, {assignments}};
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.05));
    const auto before = visionaiflow::training::EvaluateYolo11TinyDetectionBatch(model, batch, 2, {});
    QVERIFY2(before.IsSuccess(), before.IsSuccess() ? "" : before.Failure().message.c_str());
    const double beforeLoss = before.Value().totalLoss.item<double>();
    for (int step = 0; step < 80; ++step)
    {
        const auto loss = visionaiflow::training::TrainYolo11TinyDetectionStep(model, optimizer, batch, 2, {});
        QVERIFY2(loss.IsSuccess(), loss.IsSuccess() ? "" : loss.Failure().message.c_str());
    }
    const auto after = visionaiflow::training::EvaluateYolo11TinyDetectionBatch(model, batch, 2, {});
    QVERIFY2(after.IsSuccess(), after.IsSuccess() ? "" : after.Failure().message.c_str());
    QVERIFY(after.Value().totalLoss.item<double>() < beforeLoss);
    QVERIFY(after.Value().meanPositiveIou > 0.80);
}

void Yolo11DetectionDecoderTest::GridDetectorForwardAndTrainingStep()
{
    torch::manual_seed(41);
    const auto created = visionaiflow::training::CreateYolo11GridDetector(3, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto model = created.Value();
    const torch::Tensor image = torch::ones({1, 3, 32, 32}, torch::TensorOptions().dtype(torch::kFloat32));
    const torch::Tensor raw = model->forward(image);
    QCOMPARE(raw.sizes(), c10::IntArrayRef({1, 16, 6}));
    std::vector<visionaiflow::training::Yolo11AssignedTarget> assignments(16);
    assignments.at(0) = {true, 0, 0, {0.10F, 0.10F, 0.30F, 0.30F}};
    assignments.at(15) = {true, 1, 1, {0.70F, 0.70F, 0.90F, 0.90F}};
    const visionaiflow::training::Yolo11DetectionBatch batch{image, {assignments}};
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::Yolo11DetectionLossConfig lossConfig;
    lossConfig.boxWeight = 2.0;
    const auto before = visionaiflow::training::EvaluateYolo11GridDetectionBatch(model, batch, 2, lossConfig);
    QVERIFY2(before.IsSuccess(), before.IsSuccess() ? "" : before.Failure().message.c_str());
    const double beforeLoss = before.Value().totalLoss.item<double>();
    for (int step = 0; step < 40; ++step)
    {
        const auto loss = visionaiflow::training::TrainYolo11GridDetectionStep(model, optimizer, batch, 2, lossConfig);
        QVERIFY2(loss.IsSuccess(), loss.IsSuccess() ? "" : loss.Failure().message.c_str());
    }
    const auto after = visionaiflow::training::EvaluateYolo11GridDetectionBatch(model, batch, 2, lossConfig);
    QVERIFY2(after.IsSuccess(), after.IsSuccess() ? "" : after.Failure().message.c_str());
    QVERIFY(after.Value().totalLoss.item<double>() < beforeLoss);
    const auto badShape = visionaiflow::training::EvaluateYolo11GridDetectionBatch(model, {torch::ones({1, 3, 30, 32}, torch::TensorOptions().dtype(torch::kFloat32)), {assignments}}, 2, lossConfig);
    QVERIFY(!badShape.IsSuccess());
    QVERIFY(!badShape.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::TinyDetectorCheckpointRestoresContinuousTraining()
{
    torch::manual_seed(31);
    const auto created = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto baseline = created.Value();
    torch::optim::Adam baselineOptimizer(baseline->parameters(), torch::optim::AdamOptions(0.01));
    const torch::Tensor image = torch::ones({1, 3, 16, 16}, torch::TensorOptions().dtype(torch::kFloat32));
    const std::vector<visionaiflow::training::Yolo11AssignedTarget> assignments{{true, 0, 0, {0.30F, 0.30F, 0.70F, 0.70F}}, {true, 1, 1, {0.75F, 0.75F, 0.85F, 0.85F}}};
    const visionaiflow::training::Yolo11DetectionBatch batch{image, {assignments}};
    QVERIFY(visionaiflow::training::TrainYolo11TinyDetectionStep(baseline, baselineOptimizer, batch, 2, {}).IsSuccess());
    visionaiflow::training::TrainingCheckpointState checkpointState;
    checkpointState.epoch = 5;
    checkpointState.step = 23;
    checkpointState.samplerSeed = 88;
    checkpointState.samplerEpoch = 4;
    checkpointState.ampState.mode = visionaiflow::training::PrecisionMode::Fp32;
    checkpointState.ampState.scale = 1.0;
    checkpointState.ampState.consecutiveFiniteSteps = 7;
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_tiny_checkpoint_test.pt"));
    const auto saved = visionaiflow::training::SaveYolo11TinyDetectorCheckpoint(checkpointPath, baseline, baselineOptimizer, checkpointState);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());
    QVERIFY(QFile::exists(checkpointPath + QStringLiteral(".manifest.json")));
    QVERIFY(QFile::exists(checkpointPath + QStringLiteral(".sha256")));
    QFile manifestFile(checkpointPath + QStringLiteral(".manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifestObject = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(manifestObject.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(manifestObject.value(QStringLiteral("schemaName")).toString(), QStringLiteral("yolo11_tiny_detector"));
    QCOMPARE(manifestObject.value(QStringLiteral("productId")).toString(), QStringLiteral("VisionAIFlowV1"));
    QCOMPARE(manifestObject.value(QStringLiteral("adapterId")).toString(), QStringLiteral("visionaiflow.yolo11"));
    QCOMPARE(manifestObject.value(QStringLiteral("archiveSha256")).toString().size(), 64);
    QCOMPARE(StringArrayToList(manifestObject.value(QStringLiteral("parameterNames")).toArray()), visionaiflow::training::Yolo11TinyDetectorParameterNames());
    const QJsonArray parameterShapes = manifestObject.value(QStringLiteral("parameterShapes")).toArray();
    QCOMPARE(parameterShapes.size(), visionaiflow::training::Yolo11TinyDetectorParameterNames().size());
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("conv1.weight"));
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().size(), 4);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 16);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(1).toInt(), 3);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(2).toInt(), 3);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(3).toInt(), 3);
    QCOMPARE(parameterShapes.at(4).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("head.weight"));
    QCOMPARE(parameterShapes.at(4).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 12);
    QCOMPARE(parameterShapes.at(4).toObject().value(QStringLiteral("shape")).toArray().at(1).toInt(), 32);
    QCOMPARE(parameterShapes.at(5).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("head.bias"));
    QCOMPARE(parameterShapes.at(5).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 12);
    const QJsonObject manifestTrainingState = manifestObject.value(QStringLiteral("trainingState")).toObject();
    QCOMPARE(manifestTrainingState.value(QStringLiteral("epoch")).toInt(), 5);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("step")).toInt(), 23);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("samplerSeed")).toString(), QStringLiteral("88"));
    QCOMPARE(manifestTrainingState.value(QStringLiteral("samplerEpoch")).toInt(), 4);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("scheduler")).toObject().value(QStringLiteral("kind")).toString(), QStringLiteral("none"));
    QCOMPARE(manifestTrainingState.value(QStringLiteral("scheduler")).toObject().value(QStringLiteral("stepSize")).toInt(), 0);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("scheduler")).toObject().value(QStringLiteral("gamma")).toDouble(), 1.0);
    QCOMPARE(manifestTrainingState.value(QStringLiteral("amp")).toObject().value(QStringLiteral("mode")).toString(), QStringLiteral("fp32"));
    QCOMPARE(manifestTrainingState.value(QStringLiteral("amp")).toObject().value(QStringLiteral("consecutiveFiniteSteps")).toInt(), 7);
    QVERIFY(manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cpuCaptured")).toBool());
    QVERIFY(!manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cudaCaptured")).toBool());
    QCOMPARE(manifestTrainingState.value(QStringLiteral("rng")).toObject().value(QStringLiteral("cudaDeviceCount")).toInt(), 0);
    torch::manual_seed(123);
    const auto createdRestored = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(createdRestored.IsSuccess(), createdRestored.IsSuccess() ? "" : createdRestored.Failure().message.c_str());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    visionaiflow::training::TrainingCheckpointState restoredState;
    const auto loaded = visionaiflow::training::LoadYolo11TinyDetectorCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU, restoredState);
    QVERIFY2(loaded.IsSuccess(), loaded.IsSuccess() ? "" : loaded.Failure().message.c_str());
    QCOMPARE(restoredState.epoch, checkpointState.epoch);
    QCOMPARE(restoredState.step, checkpointState.step);
    QCOMPARE(restoredState.samplerSeed, checkpointState.samplerSeed);
    QCOMPARE(restoredState.samplerEpoch, checkpointState.samplerEpoch);
    QCOMPARE(restoredState.schedulerState.kind, checkpointState.schedulerState.kind);
    QCOMPARE(restoredState.schedulerState.stepSize, checkpointState.schedulerState.stepSize);
    QCOMPARE(restoredState.schedulerState.gamma, checkpointState.schedulerState.gamma);
    QCOMPARE(restoredState.ampState.consecutiveFiniteSteps, checkpointState.ampState.consecutiveFiniteSteps);
    QVERIFY(restoredState.captureCpuRng);
    QVERIFY(!restoredState.captureCudaRng);
    QCOMPARE(restoredState.cudaRngDeviceCount, int64_t{0});
    QVERIFY(visionaiflow::training::TrainYolo11TinyDetectionStep(baseline, baselineOptimizer, batch, 2, {}).IsSuccess());
    QVERIFY(visionaiflow::training::TrainYolo11TinyDetectionStep(restored, restoredOptimizer, batch, 2, {}).IsSuccess());
    const auto baselineParameters = baseline->parameters();
    const auto restoredParameters = restored->parameters();
    QCOMPARE(baselineParameters.size(), restoredParameters.size());
    for (size_t index = 0; index < baselineParameters.size(); ++index) QVERIFY(torch::allclose(baselineParameters[index], restoredParameters[index], 1.0e-6, 1.0e-6));
}

void Yolo11DetectionDecoderTest::GridDetectorCheckpointRestoresContinuousTraining()
{
    torch::manual_seed(47);
    const auto created = visionaiflow::training::CreateYolo11GridDetector(3, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto baseline = created.Value();
    torch::optim::Adam baselineOptimizer(baseline->parameters(), torch::optim::AdamOptions(0.01));
    const torch::Tensor image = torch::ones({1, 3, 32, 32}, torch::TensorOptions().dtype(torch::kFloat32));
    std::vector<visionaiflow::training::Yolo11AssignedTarget> assignments(16);
    assignments.at(0) = {true, 0, 0, {0.10F, 0.10F, 0.30F, 0.30F}};
    assignments.at(15) = {true, 1, 1, {0.70F, 0.70F, 0.90F, 0.90F}};
    const visionaiflow::training::Yolo11DetectionBatch batch{image, {assignments}};
    QVERIFY(visionaiflow::training::TrainYolo11GridDetectionStep(baseline, baselineOptimizer, batch, 2, {}).IsSuccess());
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_grid_checkpoint_test.pt"));
    const auto saved = visionaiflow::training::SaveYolo11GridDetectorCheckpoint(checkpointPath, baseline, baselineOptimizer);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());
    QFile manifestFile(checkpointPath + QStringLiteral(".manifest.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject manifestObject = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(manifestObject.value(QStringLiteral("schemaName")).toString(), QStringLiteral("yolo11_grid_detector"));
    QCOMPARE(StringArrayToList(manifestObject.value(QStringLiteral("parameterNames")).toArray()), visionaiflow::training::Yolo11GridDetectorParameterNames());
    const QJsonArray parameterShapes = manifestObject.value(QStringLiteral("parameterShapes")).toArray();
    QCOMPARE(parameterShapes.size(), visionaiflow::training::Yolo11GridDetectorParameterNames().size());
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("backbone.0.weight"));
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().size(), 4);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 16);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(1).toInt(), 3);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(2).toInt(), 3);
    QCOMPARE(parameterShapes.at(0).toObject().value(QStringLiteral("shape")).toArray().at(3).toInt(), 3);
    QCOMPARE(parameterShapes.at(parameterShapes.size() - 1).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("head.bias"));
    QCOMPARE(parameterShapes.at(parameterShapes.size() - 1).toObject().value(QStringLiteral("shape")).toArray().size(), 1);
    QCOMPARE(parameterShapes.at(parameterShapes.size() - 1).toObject().value(QStringLiteral("shape")).toArray().at(0).toInt(), 6);
    torch::manual_seed(99);
    const auto createdRestored = visionaiflow::training::CreateYolo11GridDetector(3, 2);
    QVERIFY2(createdRestored.IsSuccess(), createdRestored.IsSuccess() ? "" : createdRestored.Failure().message.c_str());
    auto restored = createdRestored.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    const auto loaded = visionaiflow::training::LoadYolo11GridDetectorCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU);
    QVERIFY2(loaded.IsSuccess(), loaded.IsSuccess() ? "" : loaded.Failure().message.c_str());
    QVERIFY(visionaiflow::training::TrainYolo11GridDetectionStep(baseline, baselineOptimizer, batch, 2, {}).IsSuccess());
    QVERIFY(visionaiflow::training::TrainYolo11GridDetectionStep(restored, restoredOptimizer, batch, 2, {}).IsSuccess());
    const auto baselineParameters = baseline->parameters();
    const auto restoredParameters = restored->parameters();
    QCOMPARE(baselineParameters.size(), restoredParameters.size());
    for (size_t index = 0; index < baselineParameters.size(); ++index) QVERIFY(torch::allclose(baselineParameters[index], restoredParameters[index], 1.0e-6, 1.0e-6));
    const auto baselineBuffers = baseline->buffers();
    const auto restoredBuffers = restored->buffers();
    QCOMPARE(baselineBuffers.size(), restoredBuffers.size());
    for (size_t index = 0; index < baselineBuffers.size(); ++index) QVERIFY(torch::allclose(baselineBuffers[index], restoredBuffers[index], 1.0e-6, 1.0e-6));
}

void Yolo11DetectionDecoderTest::CheckpointRejectsTamperedArchive()
{
    torch::manual_seed(59);
    const auto created = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto model = created.Value();
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(0.01));
    const torch::Tensor image = torch::ones({1, 3, 16, 16}, torch::TensorOptions().dtype(torch::kFloat32));
    const std::vector<visionaiflow::training::Yolo11AssignedTarget> assignments{{true, 0, 0, {0.30F, 0.30F, 0.70F, 0.70F}}, {true, 1, 1, {0.75F, 0.75F, 0.85F, 0.85F}}};
    const visionaiflow::training::Yolo11DetectionBatch batch{image, {assignments}};
    QVERIFY(visionaiflow::training::TrainYolo11TinyDetectionStep(model, optimizer, batch, 2, {}).IsSuccess());
    const QString checkpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_tampered_checkpoint_test.pt"));
    const auto saved = visionaiflow::training::SaveYolo11TinyDetectorCheckpoint(checkpointPath, model, optimizer);
    QVERIFY2(saved.IsSuccess(), saved.IsSuccess() ? "" : saved.Failure().message.c_str());
    QFile checkpointFile(checkpointPath);
    QVERIFY(checkpointFile.open(QIODevice::Append));
    QVERIFY(checkpointFile.write("x", 1) == 1);
    checkpointFile.close();
    const auto restoredCreated = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(restoredCreated.IsSuccess(), restoredCreated.IsSuccess() ? "" : restoredCreated.Failure().message.c_str());
    auto restored = restoredCreated.Value();
    torch::optim::Adam restoredOptimizer(restored->parameters(), torch::optim::AdamOptions(0.01));
    const auto loaded = visionaiflow::training::LoadYolo11TinyDetectorCheckpoint(checkpointPath, restored, restoredOptimizer, torch::kCPU);
    QVERIFY(!loaded.IsSuccess());
    QVERIFY(!loaded.Failure().message.empty());

    const QString manifestCheckpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_tampered_manifest_test.pt"));
    const auto savedManifestCase = visionaiflow::training::SaveYolo11TinyDetectorCheckpoint(manifestCheckpointPath, model, optimizer);
    QVERIFY2(savedManifestCase.IsSuccess(), savedManifestCase.IsSuccess() ? "" : savedManifestCase.Failure().message.c_str());
    const QString manifestPath = manifestCheckpointPath + QStringLiteral(".manifest.json");
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();
    QJsonArray parameterNames = manifest.value(QStringLiteral("parameterNames")).toArray();
    parameterNames.replace(0, QStringLiteral("conv1.unexpected"));
    manifest.insert(QStringLiteral("parameterNames"), parameterNames);
    QVERIFY(WriteJsonObject(manifestPath, manifest));
    const auto manifestRestoredCreated = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(manifestRestoredCreated.IsSuccess(), manifestRestoredCreated.IsSuccess() ? "" : manifestRestoredCreated.Failure().message.c_str());
    auto manifestRestored = manifestRestoredCreated.Value();
    torch::optim::Adam manifestRestoredOptimizer(manifestRestored->parameters(), torch::optim::AdamOptions(0.01));
    const auto manifestLoaded = visionaiflow::training::LoadYolo11TinyDetectorCheckpoint(manifestCheckpointPath, manifestRestored, manifestRestoredOptimizer, torch::kCPU);
    QVERIFY(!manifestLoaded.IsSuccess());
    QVERIFY(!manifestLoaded.Failure().message.empty());

    const QString shapeManifestCheckpointPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_tampered_shape_manifest_test.pt"));
    const auto savedShapeManifestCase = visionaiflow::training::SaveYolo11TinyDetectorCheckpoint(shapeManifestCheckpointPath, model, optimizer);
    QVERIFY2(savedShapeManifestCase.IsSuccess(), savedShapeManifestCase.IsSuccess() ? "" : savedShapeManifestCase.Failure().message.c_str());
    const QString shapeManifestPath = shapeManifestCheckpointPath + QStringLiteral(".manifest.json");
    QFile shapeManifestFile(shapeManifestPath);
    QVERIFY(shapeManifestFile.open(QIODevice::ReadOnly));
    QJsonObject shapeManifest = QJsonDocument::fromJson(shapeManifestFile.readAll()).object();
    shapeManifestFile.close();
    QJsonArray parameterShapes = shapeManifest.value(QStringLiteral("parameterShapes")).toArray();
    QJsonObject firstParameter = parameterShapes.at(0).toObject();
    QJsonArray firstShape = firstParameter.value(QStringLiteral("shape")).toArray();
    firstShape.replace(0, 999);
    firstParameter.insert(QStringLiteral("shape"), firstShape);
    parameterShapes.replace(0, firstParameter);
    shapeManifest.insert(QStringLiteral("parameterShapes"), parameterShapes);
    QVERIFY(WriteJsonObject(shapeManifestPath, shapeManifest));
    const auto shapeManifestRestoredCreated = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(shapeManifestRestoredCreated.IsSuccess(), shapeManifestRestoredCreated.IsSuccess() ? "" : shapeManifestRestoredCreated.Failure().message.c_str());
    auto shapeManifestRestored = shapeManifestRestoredCreated.Value();
    torch::optim::Adam shapeManifestRestoredOptimizer(shapeManifestRestored->parameters(), torch::optim::AdamOptions(0.01));
    const auto shapeManifestLoaded = visionaiflow::training::LoadYolo11TinyDetectorCheckpoint(shapeManifestCheckpointPath, shapeManifestRestored, shapeManifestRestoredOptimizer, torch::kCPU);
    QVERIFY(!shapeManifestLoaded.IsSuccess());
    QVERIFY(!shapeManifestLoaded.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::CreatesYolo11DetectionModelPackage()
{
    const auto created = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto model = created.Value();
    const QString onnxPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_package_") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".onnx"));
    const auto exported = visionaiflow::exporter::ExportYolo11TinyDetectorOnnx(onnxPath, model, 3, 16, 16, 2, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());
    const QString packageRoot = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_package_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    visionaiflow::exporter::Yolo11DetectionPackageMetadata metadata;
    metadata.packageId = QStringLiteral("yolo11-test");
    metadata.packageVersion = QStringLiteral("1.0.0");
    metadata.adapterId = QStringLiteral("visionaiflow.yolo11");
    metadata.adapterVersion = QStringLiteral("0.1.0");
    metadata.trainingRunId = QStringLiteral("yolo11-run-test");
    metadata.datasetId = QStringLiteral("yolo11-dataset-test");
    metadata.trainingConfigSha256 = QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    metadata.sourceCheckpointSha256 = QStringLiteral("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    metadata.exporterProductVersion = QStringLiteral("0.1.0");
    metadata.minSupportedProductVersion = QStringLiteral("0.1.0");
    metadata.maxSupportedProductVersion = QStringLiteral("1.0.0");
    metadata.licenseId = QStringLiteral("internal-test-license");
    metadata.licenseName = QStringLiteral("Internal Test License");
    metadata.labels = QStringList{QStringLiteral("OK"), QStringLiteral("NG")};
    metadata.inputChannels = 3;
    metadata.imageHeight = 16;
    metadata.imageWidth = 16;
    metadata.rowCount = 2;
    metadata.classCount = 2;
    const auto packaged = visionaiflow::exporter::CreateUnsignedYolo11DetectionModelPackage(packageRoot, onnxPath, metadata);
    QVERIFY2(packaged.IsSuccess(), packaged.IsSuccess() ? "" : packaged.Failure().message.c_str());
    QVERIFY(visionaiflow::exporter::VerifyModelPackage(packageRoot, false).IsSuccess());
    QFile packageFile(QDir(packageRoot).filePath(QStringLiteral("package.json")));
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    const QJsonObject packageObject = QJsonDocument::fromJson(packageFile.readAll()).object();
    QVERIFY(QFileInfo::exists(QDir(packageRoot).filePath(QStringLiteral("signature.json"))));
    QVERIFY(QFileInfo::exists(QDir(packageRoot).filePath(QStringLiteral("plugins/win-x64/plugins.json"))));
    QVERIFY(QFileInfo::exists(QDir(packageRoot).filePath(QStringLiteral("licenses/license.json"))));
    QFile signatureFile(QDir(packageRoot).filePath(QStringLiteral("signature.json")));
    QVERIFY(signatureFile.open(QIODevice::ReadOnly));
    const QJsonObject signatureObject = QJsonDocument::fromJson(signatureFile.readAll()).object();
    QFile pluginsFile(QDir(packageRoot).filePath(QStringLiteral("plugins/win-x64/plugins.json")));
    QVERIFY(pluginsFile.open(QIODevice::ReadOnly));
    const QJsonObject pluginsObject = QJsonDocument::fromJson(pluginsFile.readAll()).object();
    QFile licenseFile(QDir(packageRoot).filePath(QStringLiteral("licenses/license.json")));
    QVERIFY(licenseFile.open(QIODevice::ReadOnly));
    const QJsonObject licenseObject = QJsonDocument::fromJson(licenseFile.readAll()).object();
    QFile preprocessingFile(QDir(packageRoot).filePath(QStringLiteral("preprocessing.json")));
    QVERIFY(preprocessingFile.open(QIODevice::ReadOnly));
    const QJsonObject preprocessingObject = QJsonDocument::fromJson(preprocessingFile.readAll()).object();
    QFile postprocessingFile(QDir(packageRoot).filePath(QStringLiteral("postprocessing.json")));
    QVERIFY(postprocessingFile.open(QIODevice::ReadOnly));
    const QJsonObject postprocessingObject = QJsonDocument::fromJson(postprocessingFile.readAll()).object();
    QCOMPARE(packageObject.value(QStringLiteral("projectType")).toString(), QStringLiteral("detection"));
    QCOMPARE(packageObject.value(QStringLiteral("decoderId")).toString(), QStringLiteral("yolo11.center_nms"));
    QCOMPARE(packageObject.value(QStringLiteral("inputContract")).toObject().value(QStringLiteral("layout")).toString(), QStringLiteral("NCHW"));
    QCOMPARE(packageObject.value(QStringLiteral("outputContract")).toObject().value(QStringLiteral("name")).toString(), QStringLiteral("rawHead"));
    const QJsonObject inputProfile = packageObject.value(QStringLiteral("inputContract")).toObject().value(QStringLiteral("shapeProfile")).toObject();
    const QJsonObject outputProfile = packageObject.value(QStringLiteral("outputContract")).toObject().value(QStringLiteral("shapeProfile")).toObject();
    QCOMPARE(inputProfile.value(QStringLiteral("dynamicDimensions")).toBool(true), false);
    QCOMPARE(inputProfile.value(QStringLiteral("opt")).toArray().size(), 4);
    QCOMPARE(inputProfile.value(QStringLiteral("opt")).toArray().at(1).toInt(), 3);
    QCOMPARE(outputProfile.value(QStringLiteral("opt")).toArray().size(), 3);
    QCOMPARE(outputProfile.value(QStringLiteral("opt")).toArray().at(2).toInt(), 6);
    QCOMPARE(preprocessingObject.value(QStringLiteral("resizePolicy")).toString(), QStringLiteral("letterbox_supported"));
    QCOMPARE(preprocessingObject.value(QStringLiteral("shapeProfile")).toObject().value(QStringLiteral("opt")).toArray().at(2).toInt(), 16);
    QCOMPARE(postprocessingObject.value(QStringLiteral("coordinateRestore")).toString(), QStringLiteral("letterbox_inverse_if_present"));
    const QJsonObject trainingProvenance = packageObject.value(QStringLiteral("trainingProvenance")).toObject();
    QCOMPARE(trainingProvenance.value(QStringLiteral("trainingRunId")).toString(), metadata.trainingRunId);
    QCOMPARE(trainingProvenance.value(QStringLiteral("datasetId")).toString(), metadata.datasetId);
    QCOMPARE(trainingProvenance.value(QStringLiteral("trainingConfigSha256")).toString(), metadata.trainingConfigSha256);
    QCOMPARE(trainingProvenance.value(QStringLiteral("sourceCheckpointSha256")).toString(), metadata.sourceCheckpointSha256);
    QVERIFY(!trainingProvenance.value(QStringLiteral("exportedUtc")).toString().isEmpty());
    const QJsonObject productRange = packageObject.value(QStringLiteral("supportedProductRange")).toObject();
    QCOMPARE(productRange.value(QStringLiteral("productId")).toString(), QStringLiteral("VisionAIFlowV1"));
    QCOMPARE(productRange.value(QStringLiteral("minVersion")).toString(), metadata.minSupportedProductVersion);
    QCOMPARE(productRange.value(QStringLiteral("maxVersion")).toString(), metadata.maxSupportedProductVersion);
    QVERIFY(packageObject.value(QStringLiteral("pluginRequirements")).isArray());
    QCOMPARE(packageObject.value(QStringLiteral("pluginRequirements")).toArray().size(), 0);
    const QJsonObject licenseMetadata = packageObject.value(QStringLiteral("licenseMetadata")).toObject();
    QCOMPARE(licenseMetadata.value(QStringLiteral("licenseId")).toString(), metadata.licenseId);
    QCOMPARE(licenseMetadata.value(QStringLiteral("name")).toString(), metadata.licenseName);
    QCOMPARE(licenseMetadata.value(QStringLiteral("file")).toString(), QStringLiteral("licenses/license.json"));
    QCOMPARE(signatureObject.value(QStringLiteral("signatureState")).toString(), QStringLiteral("unsigned"));
    QCOMPARE(signatureObject.value(QStringLiteral("signedPayload")).toString(), QStringLiteral("checksums.json"));
    QCOMPARE(pluginsObject.value(QStringLiteral("platform")).toString(), QStringLiteral("win-x64"));
    QCOMPARE(pluginsObject.value(QStringLiteral("plugins")).toArray().size(), 0);
    QCOMPARE(licenseObject.value(QStringLiteral("licenseId")).toString(), metadata.licenseId);
    QCOMPARE(licenseObject.value(QStringLiteral("name")).toString(), metadata.licenseName);
    visionaiflow::exporter::Yolo11DetectionPackageMetadata missingProvenance = metadata;
    missingProvenance.trainingConfigSha256.clear();
    const QString invalidPackageRoot = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_package_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto invalidPackage = visionaiflow::exporter::CreateUnsignedYolo11DetectionModelPackage(invalidPackageRoot, onnxPath, missingProvenance);
    QVERIFY(!invalidPackage.IsSuccess());
    QVERIFY(!invalidPackage.Failure().message.empty());
    const auto duplicate = visionaiflow::exporter::CreateUnsignedYolo11DetectionModelPackage(packageRoot, onnxPath, metadata);
    QVERIFY(!duplicate.IsSuccess());
    QVERIFY(!duplicate.Failure().message.empty());
}

void Yolo11DetectionDecoderTest::ExportedHeadRunsInOpenVinoAndTensorRt()
{
    const auto created = visionaiflow::training::CreateYolo11TinyDetector(3, 2, 2);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
    auto model = created.Value();
    torch::NoGradGuard noGrad;
    model->Conv1Weight().zero_();
    model->Conv1Bias().zero_();
    model->Conv2Weight().zero_();
    model->Conv2Bias().zero_();
    model->HeadWeight().zero_();
    const std::vector<float> expectedRaw{50.0F, 50.0F, 40.0F, 40.0F, 0.90F, 0.10F, 80.0F, 80.0F, 10.0F, 10.0F, 0.10F, 0.70F};
    model->HeadBias().copy_(torch::from_blob(const_cast<float *>(expectedRaw.data()), {12}, torch::TensorOptions().dtype(torch::kFloat32)).clone());
    const torch::Tensor libTorchRaw = model->forward(torch::zeros({1, 3, 16, 16}, torch::TensorOptions().dtype(torch::kFloat32))).contiguous().view({12}).to(torch::kCPU);
    std::vector<float> libTorchValues(12);
    std::memcpy(libTorchValues.data(), libTorchRaw.data_ptr<float>(), libTorchValues.size() * sizeof(float));
    VerifyRawHead(libTorchValues, expectedRaw);
    const QString onnxPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/yolo11_tiny_") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".onnx"));
    const auto exported = visionaiflow::exporter::ExportYolo11TinyDetectorOnnx(onnxPath, model, 3, 16, 16, 2, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());
    const HostRunResult openVino = RunHostYolo11Inference(NativeExecutablePath(QStringLiteral("VisionOpenVinoHost")), onnxPath, QStringList{QStringLiteral("F:/VisionAIFlowDeps/openvino2025.3.0/bin"), QStringLiteral("F:/Qt6.9.2/6.9.2/msvc2022_64/bin")});
    QVERIFY2(openVino.ok, qPrintable(openVino.errorMessage));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("runtime:")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("requestedDevice=CPU")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("executionDevices=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("inferencePrecision=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("performanceHint=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("inferenceNumThreads=")));
    VerifyRawHead(openVino.rawHead, expectedRaw);
    QCOMPARE(static_cast<qsizetype>(openVino.detections.size()), 2);
    VerifyDetection(openVino.detections.at(0), 0, 0.90F, 30.0F, 30.0F, 70.0F, 70.0F);
    VerifyDetection(openVino.detections.at(1), 1, 0.70F, 75.0F, 75.0F, 85.0F, 85.0F);
    const HostRunResult tensorRt = RunHostYolo11Inference(NativeExecutablePath(QStringLiteral("VisionTensorRtHost")), onnxPath, QStringList{QStringLiteral("C:/PROGRA~1/NVIDIA~2/CUDA/v11.8/bin"), QStringLiteral("E:/TensorRT-10.0.1.6/lib"), QStringLiteral("F:/Qt6.9.2/6.9.2/msvc2022_64/bin")});
    QVERIFY2(tensorRt.ok, qPrintable(tensorRt.errorMessage));
    QVERIFY(HasYolo11EngineCacheManifest());
    VerifyRawHead(tensorRt.rawHead, expectedRaw);
    QCOMPARE(static_cast<qsizetype>(tensorRt.detections.size()), 2);
    VerifyDetection(tensorRt.detections.at(0), 0, 0.90F, 30.0F, 30.0F, 70.0F, 70.0F);
    VerifyDetection(tensorRt.detections.at(1), 1, 0.70F, 75.0F, 75.0F, 85.0F, 85.0F);
}

QTEST_GUILESS_MAIN(Yolo11DetectionDecoderTest)

#include "tst_Yolo11DetectionDecoder.moc"
