#include "visionaiflow/annotation/AnnotationStore.h"
#include "visionaiflow/annotation/LabelImpactAnalyzer.h"
#include "visionaiflow/annotation/LabelMutationTransaction.h"
#include "visionaiflow/project_store/LabelStore.h"
#include "visionaiflow/project_store/ProjectStore.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <functional>

class AnnotationStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void SavesAndLoadsLineAnnotation();
    void SavesAndLoadsMaskAndOcrAnnotations();
    void RejectsDuplicateAnnotationIdsOnSave();
    void RejectsTamperedAnnotationJsonSchema();
    void AnalyzesLabelImpactAcrossAnnotationFiles();
    void CreatesLabelMutationTransactionFile();
    void AppliesAndRollsBackLabelMutationTransaction();
};

void AnnotationStoreTest::SavesAndLoadsLineAnnotation()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_annotation_store_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const QString imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::LineDetection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const visionaiflow::annotation::Annotation annotation{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::Line, {}, {}, {}, {{10.0, 4.0}, {2.0, 1.0}}, {}};
    visionaiflow::annotation::AnnotationStore store;
    QVERIFY(store.Save(projectRoot, imageId, {annotation}).IsSuccess());
    const auto loaded = store.Load(projectRoot, imageId);
    QVERIFY(loaded.IsSuccess());
    QCOMPARE(static_cast<qsizetype>(loaded.Value().size()), 1);
    QCOMPARE(loaded.Value().front().line.first.x, 10.0);
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::SavesAndLoadsMaskAndOcrAnnotations()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_annotation_mask_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const QString imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::InstanceSegmentation, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const visionaiflow::annotation::Annotation mask{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::InstanceMask, QStringLiteral("part"), {}, {}, {}, {}, {2, 2, {{0U, 1U}, {1U, 2U}, {0U, 1U}}}, {}, {}};
    const visionaiflow::annotation::Annotation ocr{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::OcrQuadrilateral, QStringLiteral("text"), {}, {{0.0, 0.0}, {10.0, 0.0}, {10.0, 3.0}, {0.0, 3.0}}, {}, {}, {}, QStringLiteral("AB12"), QStringLiteral("latin")};
    visionaiflow::annotation::AnnotationStore store;
    QVERIFY(store.Save(projectRoot, imageId, {mask, ocr}).IsSuccess());
    const auto loaded = store.Load(projectRoot, imageId);
    QVERIFY(loaded.IsSuccess());
    QCOMPARE(static_cast<qsizetype>(loaded.Value().size()), 2);
    QCOMPARE(loaded.Value().at(0).mask.runs.at(1).value, 1U);
    QCOMPARE(loaded.Value().at(1).transcription, QStringLiteral("AB12"));
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::RejectsDuplicateAnnotationIdsOnSave()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_annotation_duplicate_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const QString imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const QString annotationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::annotation::Annotation first{annotationId, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("part"), {0.0, 0.0, 3.0, 2.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation second{annotationId, visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("part"), {1.0, 1.0, 3.0, 2.0}, {}, {}, {}};
    visionaiflow::annotation::AnnotationStore store;
    const auto saved = store.Save(projectRoot, imageId, {first, second});
    QVERIFY(!saved.IsSuccess());
    QVERIFY(!saved.Failure().message.empty());
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::RejectsTamperedAnnotationJsonSchema()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_annotation_tamper_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const QString imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const visionaiflow::annotation::Annotation annotation{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, QStringLiteral("part"), {0.0, 0.0, 3.0, 2.0}, {}, {}, {}};
    visionaiflow::annotation::AnnotationStore store;
    QVERIFY(store.Save(projectRoot, imageId, {annotation}).IsSuccess());

    auto rewriteAnnotation = [](const QString &projectRoot, const QString &imageId, const std::function<void(QJsonObject &)> &mutate) -> bool {
        const QString path = QDir(projectRoot).filePath(QStringLiteral("data/annotations/") + imageId + QStringLiteral(".json"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        mutate(object);
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly)) return false;
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        return output.write(bytes) == bytes.size() && output.commit();
    };

    QVERIFY(rewriteAnnotation(projectRoot, imageId, [](QJsonObject &object) { object.insert(QStringLiteral("unexpectedRoot"), true); }));
    const auto rootTampered = store.Load(projectRoot, imageId);
    QVERIFY(!rootTampered.IsSuccess());
    QVERIFY(!rootTampered.Failure().message.empty());

    QVERIFY(store.Save(projectRoot, imageId, {annotation}).IsSuccess());
    QVERIFY(rewriteAnnotation(projectRoot, imageId, [](QJsonObject &object) {
        QJsonArray annotations = object.value(QStringLiteral("annotations")).toArray();
        QJsonObject entry = annotations.at(0).toObject();
        entry.insert(QStringLiteral("unexpectedEntry"), true);
        annotations.replace(0, entry);
        object.insert(QStringLiteral("annotations"), annotations);
    }));
    const auto entryTampered = store.Load(projectRoot, imageId);
    QVERIFY(!entryTampered.IsSuccess());
    QVERIFY(!entryTampered.Failure().message.empty());
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::AnalyzesLabelImpactAcrossAnnotationFiles()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_label_impact_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const QString imageA = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString imageB = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString part = QStringLiteral("part");
    const QString scratch = QStringLiteral("scratch");
    visionaiflow::annotation::AnnotationStore store;
    const visionaiflow::annotation::Annotation partBox{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, part, {0.0, 0.0, 3.0, 2.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation partPolygon{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::Polygon, part, {}, {{0.0, 0.0}, {3.0, 0.0}, {3.0, 2.0}}, {}, {}};
    const visionaiflow::annotation::Annotation scratchBox{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, scratch, {1.0, 1.0, 3.0, 2.0}, {}, {}, {}};
    QVERIFY(store.Save(projectRoot, imageA, {partBox, scratchBox}).IsSuccess());
    QVERIFY(store.Save(projectRoot, imageB, {partPolygon}).IsSuccess());

    visionaiflow::annotation::LabelImpactAnalyzer analyzer;
    const auto impact = analyzer.Analyze(projectRoot, {part, scratch, QStringLiteral("unused")});
    QVERIFY2(impact.IsSuccess(), impact.IsSuccess() ? "" : impact.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(impact.Value().size()), 3);
    QCOMPARE(impact.Value().at(0).labelId, part);
    QCOMPARE(impact.Value().at(0).totalAnnotationCount, static_cast<qsizetype>(2));
    QCOMPARE(static_cast<qsizetype>(impact.Value().at(0).images.size()), 2);
    QCOMPARE(impact.Value().at(1).labelId, scratch);
    QCOMPARE(impact.Value().at(1).totalAnnotationCount, static_cast<qsizetype>(1));
    QCOMPARE(static_cast<qsizetype>(impact.Value().at(2).images.size()), 0);

    const QString brokenImageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSaveFile broken(QDir(projectRoot).filePath(QStringLiteral("data/annotations/") + brokenImageId + QStringLiteral(".json")));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    const QByteArray badJson("{");
    QCOMPARE(broken.write(badJson), static_cast<qint64>(badJson.size()));
    QVERIFY(broken.commit());
    const auto brokenImpact = analyzer.Analyze(projectRoot, {part});
    QVERIFY(!brokenImpact.IsSuccess());
    QVERIFY(!brokenImpact.Failure().message.empty());
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::CreatesLabelMutationTransactionFile()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_label_transaction_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));
    const QString imageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    visionaiflow::project_store::LabelStore labelStore;
    const auto partLabel = labelStore.AddLabel(projectRoot, QStringLiteral("part"), QStringLiteral("#22AAFF"));
    QVERIFY2(partLabel.IsSuccess(), partLabel.IsSuccess() ? "" : partLabel.Failure().message.c_str());
    const auto scratchLabel = labelStore.AddLabel(projectRoot, QStringLiteral("scratch"), QStringLiteral("#FF6622"));
    QVERIFY2(scratchLabel.IsSuccess(), scratchLabel.IsSuccess() ? "" : scratchLabel.Failure().message.c_str());
    const QString part = partLabel.Value().labelId;
    const visionaiflow::annotation::Annotation first{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, part, {0.0, 0.0, 3.0, 2.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation second{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::Polygon, part, {}, {{0.0, 0.0}, {3.0, 0.0}, {3.0, 2.0}}, {}, {}};
    visionaiflow::annotation::AnnotationStore store;
    QVERIFY(store.Save(projectRoot, imageId, {first, second}).IsSuccess());

    visionaiflow::annotation::LabelMutationTransactionWriter writer;
    const visionaiflow::annotation::LabelMutationRequest request{visionaiflow::annotation::LabelMutationAction::Rename, {part}, {}, QStringLiteral("part-renamed")};
    const auto transaction = writer.Create(projectRoot, request);
    QVERIFY2(transaction.IsSuccess(), transaction.IsSuccess() ? "" : transaction.Failure().message.c_str());
    QVERIFY(transaction.Value().relativePath.startsWith(QStringLiteral("backups/label-transactions/")));
    QFile file(QDir(projectRoot).filePath(transaction.Value().relativePath));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    QVERIFY(error.error == QJsonParseError::NoError);
    QVERIFY(document.isObject());
    const QJsonObject object = document.object();
    QCOMPARE(object.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(object.value(QStringLiteral("transactionId")).toString(), transaction.Value().transactionId);
    QCOMPARE(object.value(QStringLiteral("action")).toString(), QStringLiteral("rename"));
    QCOMPARE(object.value(QStringLiteral("newName")).toString(), QStringLiteral("part-renamed"));
    const QJsonArray impacts = object.value(QStringLiteral("impacts")).toArray();
    QCOMPARE(impacts.size(), 1);
    QCOMPARE(impacts.at(0).toObject().value(QStringLiteral("totalAnnotationCount")).toInt(), 2);
    const QJsonArray files = object.value(QStringLiteral("affectedAnnotationFiles")).toArray();
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.at(0).toObject().value(QStringLiteral("imageId")).toString(), imageId);
    QVERIFY(!files.at(0).toObject().value(QStringLiteral("sha256")).toString().isEmpty());
    const QString annotationBackup = files.at(0).toObject().value(QStringLiteral("backupRelativePath")).toString();
    QVERIFY(annotationBackup.startsWith(QStringLiteral("backups/label-transactions/")));
    QVERIFY(QFileInfo::exists(QDir(projectRoot).filePath(annotationBackup)));
    const QJsonArray rollbackFiles = object.value(QStringLiteral("rollbackFiles")).toArray();
    QCOMPARE(rollbackFiles.size(), 2);
    bool labelsBackedUp = false;
    bool annotationBackedUp = false;
    for (const QJsonValue &rollbackValue : rollbackFiles)
    {
        const QJsonObject rollback = rollbackValue.toObject();
        const QString relativePath = rollback.value(QStringLiteral("relativePath")).toString();
        const QString backupRelativePath = rollback.value(QStringLiteral("backupRelativePath")).toString();
        QVERIFY(!rollback.value(QStringLiteral("sha256")).toString().isEmpty());
        QVERIFY(QFileInfo::exists(QDir(projectRoot).filePath(backupRelativePath)));
        labelsBackedUp = labelsBackedUp || relativePath == QStringLiteral("labels.json");
        annotationBackedUp = annotationBackedUp || relativePath == QStringLiteral("data/annotations/") + imageId + QStringLiteral(".json");
    }
    QVERIFY(labelsBackedUp);
    QVERIFY(annotationBackedUp);
    visionaiflow::annotation::LabelMutationTransactionVerifier verifier;
    const auto verified = verifier.VerifyRollbackFiles(projectRoot, transaction.Value().relativePath);
    QVERIFY2(verified.IsSuccess(), verified.IsSuccess() ? "" : verified.Failure().message.c_str());

    const visionaiflow::annotation::LabelMutationRequest invalidMerge{visionaiflow::annotation::LabelMutationAction::Merge, {part}, part, QStringLiteral("part")};
    const auto invalid = writer.Create(projectRoot, invalidMerge);
    QVERIFY(!invalid.IsSuccess());
    QVERIFY(!invalid.Failure().message.empty());
    const visionaiflow::annotation::LabelMutationRequest missingSource{visionaiflow::annotation::LabelMutationAction::Delete, {QUuid::createUuid().toString(QUuid::WithoutBraces)}, {}, {}};
    const auto missing = writer.Create(projectRoot, missingSource);
    QVERIFY(!missing.IsSuccess());
    QVERIFY(!missing.Failure().message.empty());
    const visionaiflow::annotation::LabelMutationRequest renameConflict{visionaiflow::annotation::LabelMutationAction::Rename, {part}, {}, QStringLiteral("scratch")};
    const auto conflict = writer.Create(projectRoot, renameConflict);
    QVERIFY(!conflict.IsSuccess());
    QVERIFY(!conflict.Failure().message.empty());
    const visionaiflow::annotation::LabelMutationRequest missingTarget{visionaiflow::annotation::LabelMutationAction::Merge, {part}, QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("part")};
    const auto targetFailure = writer.Create(projectRoot, missingTarget);
    QVERIFY(!targetFailure.IsSuccess());
    QVERIFY(!targetFailure.Failure().message.empty());
    QFile tamperedBackup(QDir(projectRoot).filePath(annotationBackup));
    QVERIFY(tamperedBackup.open(QIODevice::Append));
    QCOMPARE(tamperedBackup.write("x"), static_cast<qint64>(1));
    tamperedBackup.close();
    const auto tamperedVerification = verifier.VerifyRollbackFiles(projectRoot, transaction.Value().relativePath);
    QVERIFY(!tamperedVerification.IsSuccess());
    QVERIFY(!tamperedVerification.Failure().message.empty());
    QVERIFY(QDir(root).removeRecursively());
}

void AnnotationStoreTest::AppliesAndRollsBackLabelMutationTransaction()
{
    const QString root = QDir::current().filePath(QStringLiteral(".vaf_label_apply_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QVERIFY(QDir().mkpath(root));
    const visionaiflow::project_store::ProjectDefinition project{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore projectStore;
    QVERIFY(projectStore.Create(QDir(root).filePath(QStringLiteral("project")), project).IsSuccess());
    const QString projectRoot = QDir(root).filePath(QStringLiteral("project"));

    visionaiflow::project_store::LabelStore labelStore;
    const auto partLabel = labelStore.AddLabel(projectRoot, QStringLiteral("part"), QStringLiteral("#22AAFF"));
    QVERIFY2(partLabel.IsSuccess(), partLabel.IsSuccess() ? "" : partLabel.Failure().message.c_str());
    const auto scratchLabel = labelStore.AddLabel(projectRoot, QStringLiteral("scratch"), QStringLiteral("#FF6622"));
    QVERIFY2(scratchLabel.IsSuccess(), scratchLabel.IsSuccess() ? "" : scratchLabel.Failure().message.c_str());
    const auto targetLabel = labelStore.AddLabel(projectRoot, QStringLiteral("target"), QStringLiteral("#44DD55"));
    QVERIFY2(targetLabel.IsSuccess(), targetLabel.IsSuccess() ? "" : targetLabel.Failure().message.c_str());

    const QString imageA = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString imageB = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const visionaiflow::annotation::Annotation partBox{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, partLabel.Value().labelId, {0.0, 0.0, 3.0, 2.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation scratchBox{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::BoundingBox, scratchLabel.Value().labelId, {1.0, 1.0, 3.0, 2.0}, {}, {}, {}};
    const visionaiflow::annotation::Annotation partPolygon{QUuid::createUuid().toString(QUuid::WithoutBraces), visionaiflow::annotation::AnnotationKind::Polygon, partLabel.Value().labelId, {}, {{0.0, 0.0}, {3.0, 0.0}, {3.0, 2.0}}, {}, {}};
    visionaiflow::annotation::AnnotationStore annotationStore;
    QVERIFY(annotationStore.Save(projectRoot, imageA, {partBox, scratchBox}).IsSuccess());
    QVERIFY(annotationStore.Save(projectRoot, imageB, {partPolygon}).IsSuccess());

    auto labelName = [&labelStore, &projectRoot](const QString &labelId) -> QString {
        const auto labels = labelStore.Load(projectRoot);
        if (!labels.IsSuccess()) return {};
        for (const visionaiflow::project_store::LabelDefinition &label : labels.Value())
        {
            if (label.labelId == labelId) return label.name;
        }
        return {};
    };

    visionaiflow::annotation::LabelMutationTransactionApplier applier;
    const auto renamed = applier.Apply(projectRoot, {visionaiflow::annotation::LabelMutationAction::Rename, {scratchLabel.Value().labelId}, {}, QStringLiteral("scratch-renamed")});
    QVERIFY2(renamed.IsSuccess(), renamed.IsSuccess() ? "" : renamed.Failure().message.c_str());
    QCOMPARE(renamed.Value().changedLabelCount, static_cast<qsizetype>(1));
    QCOMPARE(renamed.Value().changedAnnotationCount, static_cast<qsizetype>(0));
    QCOMPARE(labelName(scratchLabel.Value().labelId), QStringLiteral("scratch-renamed"));
    QVERIFY(applier.Rollback(projectRoot, renamed.Value().transaction.relativePath).IsSuccess());
    QCOMPARE(labelName(scratchLabel.Value().labelId), QStringLiteral("scratch"));

    const auto merged = applier.Apply(projectRoot, {visionaiflow::annotation::LabelMutationAction::Merge, {partLabel.Value().labelId}, targetLabel.Value().labelId, QStringLiteral("target")});
    QVERIFY2(merged.IsSuccess(), merged.IsSuccess() ? "" : merged.Failure().message.c_str());
    QCOMPARE(merged.Value().changedAnnotationCount, static_cast<qsizetype>(2));
    auto mergedA = annotationStore.Load(projectRoot, imageA);
    QVERIFY(mergedA.IsSuccess());
    QCOMPARE(mergedA.Value().at(0).labelId, targetLabel.Value().labelId);
    auto mergedB = annotationStore.Load(projectRoot, imageB);
    QVERIFY(mergedB.IsSuccess());
    QCOMPARE(mergedB.Value().front().labelId, targetLabel.Value().labelId);
    QVERIFY(labelName(partLabel.Value().labelId).isEmpty());
    QVERIFY(applier.Rollback(projectRoot, merged.Value().transaction.relativePath).IsSuccess());
    auto rolledBackA = annotationStore.Load(projectRoot, imageA);
    QVERIFY(rolledBackA.IsSuccess());
    QCOMPARE(rolledBackA.Value().at(0).labelId, partLabel.Value().labelId);
    QCOMPARE(labelName(partLabel.Value().labelId), QStringLiteral("part"));

    const auto deleted = applier.Apply(projectRoot, {visionaiflow::annotation::LabelMutationAction::Delete, {scratchLabel.Value().labelId}, {}, {}});
    QVERIFY2(deleted.IsSuccess(), deleted.IsSuccess() ? "" : deleted.Failure().message.c_str());
    QCOMPARE(deleted.Value().changedLabelCount, static_cast<qsizetype>(1));
    QCOMPARE(deleted.Value().changedAnnotationCount, static_cast<qsizetype>(1));
    auto deletedA = annotationStore.Load(projectRoot, imageA);
    QVERIFY(deletedA.IsSuccess());
    QCOMPARE(static_cast<qsizetype>(deletedA.Value().size()), 1);
    QVERIFY(labelName(scratchLabel.Value().labelId).isEmpty());
    QVERIFY(applier.Rollback(projectRoot, deleted.Value().transaction.relativePath).IsSuccess());
    auto restoredA = annotationStore.Load(projectRoot, imageA);
    QVERIFY(restoredA.IsSuccess());
    QCOMPARE(static_cast<qsizetype>(restoredA.Value().size()), 2);
    QCOMPARE(labelName(scratchLabel.Value().labelId), QStringLiteral("scratch"));

    QVERIFY(QDir(root).removeRecursively());
}

QTEST_APPLESS_MAIN(AnnotationStoreTest)

#include "tst_AnnotationStore.moc"
