#include "visionaiflow/export/OnnxExporter.h"
#include "visionaiflow/export/ModelPackage.h"

#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QVector>

#include <onnx/checker.h>
#include <onnx/onnx_pb.h>

#include <cmath>
#include <string>

#ifndef VAF_TEST_QT_BIN
#define VAF_TEST_QT_BIN "F:/Qt6.9.2/6.9.2/msvc2022_64/bin"
#endif
#ifndef VAF_TEST_OPENVINO_BIN
#define VAF_TEST_OPENVINO_BIN "F:/VisionAIFlowDeps/openvino2025.3.0/bin"
#endif
#ifndef VAF_TEST_CUDA_BIN
#define VAF_TEST_CUDA_BIN "C:/PROGRA~1/NVIDIA~2/CUDA/v11.8/bin"
#endif
#ifndef VAF_TEST_TENSORRT_LIB
#define VAF_TEST_TENSORRT_LIB "E:/TensorRT-10.0.1.6/lib"
#endif

namespace
{
QString NativeExecutablePath(const QString &fileName)
{
    QString executableName = fileName;
#ifdef Q_OS_WIN
    if (!executableName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) executableName.append(QStringLiteral(".exe"));
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(executableName);
}

QVector<float> TensorToVector(const torch::Tensor &tensor)
{
    const torch::Tensor materialized = tensor.detach().to(torch::kCPU).contiguous().view({-1});
    QVector<float> values;
    values.reserve(static_cast<qsizetype>(materialized.numel()));
    for (int64_t index = 0; index < materialized.numel(); ++index) values.append(materialized[index].item<float>());
    return values;
}

QVector<float> ParseLogits(const QByteArray &output)
{
    const QRegularExpression expression(QStringLiteral("logits:\\s*([^\\r\\n]+)"));
    const QRegularExpressionMatch match = expression.match(QString::fromLocal8Bit(output));
    if (!match.hasMatch()) return {};
    QVector<float> logits;
    const QStringList parts = match.captured(1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    logits.reserve(parts.size());
    for (const QString &part : parts)
    {
        bool ok = false;
        const float value = part.toFloat(&ok);
        if (!ok) return {};
        logits.append(value);
    }
    return logits;
}

struct HostInferenceResult final
{
    bool ok{false};
    QVector<float> logits;
    QString stdoutText;
    QString errorMessage;
};

HostInferenceResult RunHostInference(const QString &program, const QStringList &arguments, const QStringList &pathPrefixes)
{
    const QFileInfo programInfo(program);
    if (!programInfo.isFile()) return {false, {}, {}, QStringLiteral("Required inference host is missing: ") + program};
    const QString traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stdoutPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/host-stdout-") + traceId + QStringLiteral(".txt"));
    const QString stderrPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/host-stderr-") + traceId + QStringLiteral(".txt"));
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString currentPath = environment.value(QStringLiteral("PATH"));
    environment.insert(QStringLiteral("PATH"), pathPrefixes.join(QStringLiteral(";")) + QStringLiteral(";") + currentPath);
    environment.insert(QStringLiteral("VISIONAIFLOW_ENGINE_CACHE_ROOT"), QDir::current().filePath(QStringLiteral("out/qmake/Release/engine-cache-tests/classification")));
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments(arguments);
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(stdoutPath);
    process.setStandardErrorFile(stderrPath);
    process.start();
    if (!process.waitForStarted(10000)) return {false, {}, {}, QStringLiteral("Inference host could not start: ") + process.errorString()};
    if (!process.waitForFinished(60000)) return {false, {}, {}, QStringLiteral("Inference host timed out: ") + program};
    QFile outputFile(stdoutPath);
    QFile errorFile(stderrPath);
    const QByteArray output = outputFile.open(QIODevice::ReadOnly) ? outputFile.readAll() : QByteArray();
    const QByteArray error = errorFile.open(QIODevice::ReadOnly) ? errorFile.readAll() : QByteArray();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {false, {}, {}, QStringLiteral("Inference host failed: ") + program + QStringLiteral(" stdout=") + QString::fromLocal8Bit(output) + QStringLiteral(" stderr=") + QString::fromLocal8Bit(error)};
    const QVector<float> logits = ParseLogits(output);
    if (logits.isEmpty()) return {false, {}, {}, QStringLiteral("Inference host did not print logits: ") + program + QStringLiteral(" stdout=") + QString::fromLocal8Bit(output) + QStringLiteral(" stderr=") + QString::fromLocal8Bit(error)};
    return {true, logits, QString::fromLocal8Bit(output), {}};
}

bool HasClassificationEngineCacheManifest()
{
    QDirIterator iterator(QDir::current().filePath(QStringLiteral("out/qmake/Release/engine-cache-tests/classification")), QStringList{QStringLiteral("manifest.json")}, QDir::Files, QDirIterator::Subdirectories);
    return iterator.hasNext();
}

QStringList InitializerNames(const ONNX_NAMESPACE::ModelProto &modelProto)
{
    QStringList names;
    for (const auto &initializer : modelProto.graph().initializer())
    {
        names.append(QString::fromStdString(initializer.name()));
    }
    names.sort();
    return names;
}

void VerifyClose(const QVector<float> &expected, const QVector<float> &actual, const char *backendName)
{
    QCOMPARE(actual.size(), expected.size());
    for (qsizetype index = 0; index < expected.size(); ++index)
    {
        const float delta = std::fabs(expected[index] - actual[index]);
        QVERIFY2(delta <= 1.0e-4F, qPrintable(QStringLiteral("%1 logits differ at %2: expected=%3 actual=%4 delta=%5").arg(QString::fromLatin1(backendName)).arg(index).arg(expected[index], 0, 'g', 9).arg(actual[index], 0, 'g', 9).arg(delta, 0, 'g', 9)));
    }
}
}

class OnnxExporterTest final : public QObject
{
    Q_OBJECT

private slots:
    void ExportsOpsetTwelveModel();
    void ExportedClassifierMatchesOpenVinoAndTensorRt();
    void CreatesAndVerifiesChecksummedPackage();
};

void OnnxExporterTest::ExportsOpsetTwelveModel()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(3, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    const QString path = QDir::current().filePath(QStringLiteral("out/qmake/Release/linear_classifier_test.onnx"));
    const auto exported = visionaiflow::exporter::ExportLinearClassifierOnnx(path, model, 3, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    ONNX_NAMESPACE::ModelProto modelProto;
    const QByteArray bytes = file.readAll();
    QVERIFY(modelProto.ParseFromArray(bytes.constData(), bytes.size()));
    QCOMPARE(modelProto.ir_version(), 9);
    QCOMPARE(modelProto.opset_import_size(), 1);
    QCOMPARE(modelProto.opset_import(0).version(), 12);
    QStringList expectedInitializers = visionaiflow::training::LinearClassifierParameterNames();
    expectedInitializers.sort();
    QCOMPARE(InitializerNames(modelProto), expectedInitializers);
    ONNX_NAMESPACE::checker::check_model(modelProto, true);
    const auto contract = visionaiflow::exporter::ValidateOnnxFileContract(path, {9, 12, {QStringLiteral("Gemm")}});
    QVERIFY2(contract.IsSuccess(), contract.IsSuccess() ? "" : contract.Failure().message.c_str());
    const auto wrongWhitelist = visionaiflow::exporter::ValidateOnnxFileContract(path, {9, 12, {QStringLiteral("Relu")}});
    QVERIFY(!wrongWhitelist.IsSuccess());
    QVERIFY(!wrongWhitelist.Failure().message.empty());

    modelProto.mutable_opset_import(0)->set_version(13);
    std::string serialized;
    QVERIFY(modelProto.SerializeToString(&serialized));
    const QString invalidOpsetPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/linear_classifier_invalid_opset.onnx"));
    QSaveFile invalidOpset(invalidOpsetPath);
    QVERIFY(invalidOpset.open(QIODevice::WriteOnly));
    QCOMPARE(invalidOpset.write(serialized.data(), static_cast<qint64>(serialized.size())), static_cast<qint64>(serialized.size()));
    QVERIFY(invalidOpset.commit());
    const auto wrongOpset = visionaiflow::exporter::ValidateOnnxFileContract(invalidOpsetPath, {9, 12, {QStringLiteral("Gemm")}});
    QVERIFY(!wrongOpset.IsSuccess());
    QVERIFY(!wrongOpset.Failure().message.empty());
}

void OnnxExporterTest::ExportedClassifierMatchesOpenVinoAndTensorRt()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(3, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    const QString onnxPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".onnx"));
    const auto exported = visionaiflow::exporter::ExportLinearClassifierOnnx(onnxPath, model, 3, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());
    torch::NoGradGuard noGrad;
    model->eval();
    const torch::Tensor input = torch::zeros({1, 3}, torch::TensorOptions().dtype(torch::kFloat32));
    const QVector<float> libTorchLogits = TensorToVector(model->forward(input));
    QCOMPARE(libTorchLogits.size(), 2);
    const HostInferenceResult openVino = RunHostInference(NativeExecutablePath(QStringLiteral("VisionOpenVinoHost")), QStringList{QStringLiteral("--validate-onnx"), onnxPath}, QStringList{QStringLiteral(VAF_TEST_OPENVINO_BIN), QStringLiteral(VAF_TEST_QT_BIN)});
    QVERIFY2(openVino.ok, qPrintable(openVino.errorMessage));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("runtime:")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("requestedDevice=CPU")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("executionDevices=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("inferencePrecision=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("performanceHint=")));
    QVERIFY(openVino.stdoutText.contains(QStringLiteral("inferenceNumThreads=")));
    VerifyClose(libTorchLogits, openVino.logits, "OpenVINO");
    const HostInferenceResult tensorRt = RunHostInference(NativeExecutablePath(QStringLiteral("VisionTensorRtHost")), QStringList{QStringLiteral("--infer-onnx"), onnxPath}, QStringList{QStringLiteral(VAF_TEST_CUDA_BIN), QStringLiteral(VAF_TEST_TENSORRT_LIB), QStringLiteral(VAF_TEST_QT_BIN)});
    QVERIFY2(tensorRt.ok, qPrintable(tensorRt.errorMessage));
    QVERIFY(HasClassificationEngineCacheManifest());
    VerifyClose(libTorchLogits, tensorRt.logits, "TensorRT");
}

void OnnxExporterTest::CreatesAndVerifiesChecksummedPackage()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(3, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    const QString root = QDir::current().filePath(QStringLiteral("out/qmake/Release/model_package_tests/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(QFileInfo(root).dir().absolutePath()));
    const QString onnxPath = QDir::current().filePath(QStringLiteral("out/qmake/Release/") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".onnx"));
    const auto exported = visionaiflow::exporter::ExportLinearClassifierOnnx(onnxPath, model, 3, 2);
    QVERIFY2(exported.IsSuccess(), exported.IsSuccess() ? "" : exported.Failure().message.c_str());
    visionaiflow::exporter::ClassificationPackageMetadata metadata;
    metadata.packageId = QStringLiteral("test-package");
    metadata.packageVersion = QStringLiteral("1.0.0");
    metadata.adapterId = QStringLiteral("linear-classifier");
    metadata.adapterVersion = QStringLiteral("1.0.0");
    metadata.trainingRunId = QStringLiteral("classification-run-test");
    metadata.datasetId = QStringLiteral("classification-dataset-test");
    metadata.trainingConfigSha256 = QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    metadata.sourceCheckpointSha256 = QStringLiteral("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    metadata.exporterProductVersion = QStringLiteral("0.1.0");
    metadata.minSupportedProductVersion = QStringLiteral("0.1.0");
    metadata.maxSupportedProductVersion = QStringLiteral("1.0.0");
    metadata.licenseId = QStringLiteral("internal-test-license");
    metadata.licenseName = QStringLiteral("Internal Test License");
    metadata.labels = QStringList{QStringLiteral("zero"), QStringLiteral("one")};
    metadata.inputFeatures = 3;
    metadata.classCount = 2;
    const auto packaged = visionaiflow::exporter::CreateUnsignedClassificationModelPackage(root, onnxPath, metadata);
    QVERIFY2(packaged.IsSuccess(), packaged.IsSuccess() ? "" : packaged.Failure().message.c_str());
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("labels.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("preprocessing.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("postprocessing.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("signature.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("plugins/win-x64/plugins.json"))));
    QVERIFY(QFileInfo::exists(QDir(root).filePath(QStringLiteral("licenses/license.json"))));
    QFile packageFile(QDir(root).filePath(QStringLiteral("package.json")));
    QVERIFY(packageFile.open(QIODevice::ReadOnly));
    const QJsonObject packageObject = QJsonDocument::fromJson(packageFile.readAll()).object();
    QFile signatureFile(QDir(root).filePath(QStringLiteral("signature.json")));
    QVERIFY(signatureFile.open(QIODevice::ReadOnly));
    const QJsonObject signatureObject = QJsonDocument::fromJson(signatureFile.readAll()).object();
    QFile pluginsFile(QDir(root).filePath(QStringLiteral("plugins/win-x64/plugins.json")));
    QVERIFY(pluginsFile.open(QIODevice::ReadOnly));
    const QJsonObject pluginsObject = QJsonDocument::fromJson(pluginsFile.readAll()).object();
    QFile licenseFile(QDir(root).filePath(QStringLiteral("licenses/license.json")));
    QVERIFY(licenseFile.open(QIODevice::ReadOnly));
    const QJsonObject licenseObject = QJsonDocument::fromJson(licenseFile.readAll()).object();
    const QJsonObject inputProfile = packageObject.value(QStringLiteral("inputContract")).toObject().value(QStringLiteral("shapeProfile")).toObject();
    const QJsonObject outputProfile = packageObject.value(QStringLiteral("outputContract")).toObject().value(QStringLiteral("shapeProfile")).toObject();
    QCOMPARE(inputProfile.value(QStringLiteral("dynamicDimensions")).toBool(true), false);
    QCOMPARE(inputProfile.value(QStringLiteral("opt")).toArray().size(), 2);
    QCOMPARE(inputProfile.value(QStringLiteral("opt")).toArray().at(1).toInt(), 3);
    QCOMPARE(outputProfile.value(QStringLiteral("opt")).toArray().at(1).toInt(), 2);
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
    QVERIFY(visionaiflow::exporter::VerifyModelPackage(root, false).IsSuccess());
    const QString installedRoot = QDir::current().filePath(QStringLiteral("out/qmake/Release/model_package_tests/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto installed = visionaiflow::exporter::InstallModelPackage(root, installedRoot, false);
    QVERIFY2(installed.IsSuccess(), installed.IsSuccess() ? "" : installed.Failure().message.c_str());
    QVERIFY(visionaiflow::exporter::VerifyModelPackage(installedRoot, false).IsSuccess());
    QVERIFY(QFileInfo::exists(QDir(installedRoot).filePath(QStringLiteral("signature.json"))));
    QVERIFY(QFileInfo::exists(QDir(installedRoot).filePath(QStringLiteral("plugins/win-x64/plugins.json"))));
    QVERIFY(QFileInfo::exists(QDir(installedRoot).filePath(QStringLiteral("licenses/license.json"))));
    const auto duplicateInstall = visionaiflow::exporter::InstallModelPackage(root, installedRoot, false);
    QVERIFY(!duplicateInstall.IsSuccess());
    QVERIFY(!duplicateInstall.Failure().message.empty());
    visionaiflow::exporter::ClassificationPackageMetadata missingProvenance = metadata;
    missingProvenance.trainingConfigSha256.clear();
    const QString invalidRoot = QDir::current().filePath(QStringLiteral("out/qmake/Release/model_package_tests/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto invalidPackage = visionaiflow::exporter::CreateUnsignedClassificationModelPackage(invalidRoot, onnxPath, missingProvenance);
    QVERIFY(!invalidPackage.IsSuccess());
    QVERIFY(!invalidPackage.Failure().message.empty());
    const auto signatureRequired = visionaiflow::exporter::VerifyModelPackage(root, true);
    QVERIFY(!signatureRequired.IsSuccess());
    QVERIFY(!signatureRequired.Failure().message.empty());
    QFile artifact(QDir(root).filePath(QStringLiteral("artifacts/trt1001_opset12/model.onnx")));
    QVERIFY(artifact.open(QIODevice::Append));
    QVERIFY(artifact.write("x", 1) == 1);
    artifact.close();
    const auto tampered = visionaiflow::exporter::VerifyModelPackage(root, false);
    QVERIFY(!tampered.IsSuccess());
    QVERIFY(!tampered.Failure().message.empty());
    const QString tamperedInstallRoot = QDir::current().filePath(QStringLiteral("out/qmake/Release/model_package_tests/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto tamperedInstall = visionaiflow::exporter::InstallModelPackage(root, tamperedInstallRoot, false);
    QVERIFY(!tamperedInstall.IsSuccess());
    QVERIFY(!tamperedInstall.Failure().message.empty());
    QVERIFY(!QFileInfo::exists(tamperedInstallRoot));
}

QTEST_GUILESS_MAIN(OnnxExporterTest)

#include "tst_OnnxExporter.moc"
