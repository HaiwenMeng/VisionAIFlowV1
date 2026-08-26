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
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QVector>

#include <onnx/checker.h>
#include <onnx/onnx_pb.h>

#include <cmath>
#include <string>

#ifndef VAF_TEST_QT_BIN
#define VAF_TEST_QT_BIN "F:/Qt6.7.3/6.7.3/msvc2019_64/bin"
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

}

class OnnxExporterTest final : public QObject
{
    Q_OBJECT

private slots:
    void ExportsOpsetTwelveModel();
    void CreatesAndVerifiesChecksummedPackage();
};

void OnnxExporterTest::ExportsOpsetTwelveModel()
{
    const auto created = visionaiflow::models::classification::linear::CreateLinearClassifier(3, 2);
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
    QStringList expectedInitializers = visionaiflow::models::classification::linear::LinearClassifierParameterNames();
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

void OnnxExporterTest::CreatesAndVerifiesChecksummedPackage()
{
    const auto created = visionaiflow::models::classification::linear::CreateLinearClassifier(3, 2);
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
    metadata.modelId = QStringLiteral("classification.linear.v1");
    metadata.artifactContractVersion = 1;
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
    const auto openVinoContract = visionaiflow::exporter::LoadVerifiedModelPackageRuntimeContract(root, QStringLiteral("openvino_cpu"), false);
    QVERIFY2(openVinoContract.IsSuccess(), openVinoContract.IsSuccess() ? "" : openVinoContract.Failure().message.c_str());
    QCOMPARE(openVinoContract.Value().modelId, metadata.modelId);
    QCOMPARE(openVinoContract.Value().decoderId, QStringLiteral("classification.argmax"));
    QVERIFY(QFileInfo::exists(openVinoContract.Value().artifactPath));
    const auto unsupportedRuntime = visionaiflow::exporter::LoadVerifiedModelPackageRuntimeContract(root, QStringLiteral("unsupported"), false);
    QVERIFY(!unsupportedRuntime.IsSuccess());
    QVERIFY(!unsupportedRuntime.Failure().message.empty());
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
