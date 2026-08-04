#include "visionaiflow/project_store/ProjectStore.h"
#include "visionaiflow/project_store/ProjectLock.h"
#include "visionaiflow/project_store/DatasetIndex.h"
#include "visionaiflow/project_store/LabelStore.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <functional>
#include <utility>
#include <vector>

namespace
{
class ScopedTestDirectory final
{
public:
    explicit ScopedTestDirectory(const QString &prefix)
        : m_path(QDir(QDir::currentPath()).filePath(prefix + QUuid::createUuid().toString(QUuid::WithoutBraces)))
        , m_valid(QDir().mkpath(m_path))
    {
    }

    ~ScopedTestDirectory()
    {
        if (m_valid && !QDir(m_path).removeRecursively()) qWarning("Failed to remove test directory: %s", qPrintable(m_path));
    }

    bool IsValid() const noexcept { return m_valid; }
    const QString &Path() const noexcept { return m_path; }

private:
    QString m_path;
    bool m_valid{false};
};
}

class ProjectStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void CreatesAndReopensEveryProjectType();
    void RejectsMutableClassificationConstraint();
    void RejectsIncompleteProjectLayout();
    void RejectsTamperedProjectJsonSchema();
    void RejectsUnsupportedProjectSchemaVersions();
    void AcquiresAndReleasesProjectLock();
    void RejectsConcurrentProjectLock();
    void RejectsMalformedProjectLockWithoutDeletingIt();
    void ImportsImageAndRejectsDuplicateHash();
    void RejectsTamperedDatasetIndex();
    void SavesAndRejectsInvalidLabels();
};

void ProjectStoreTest::CreatesAndReopensEveryProjectType()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_store_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    visionaiflow::project_store::ProjectStore store;
    const std::vector<std::pair<visionaiflow::project_store::ProjectType, visionaiflow::project_store::ClassificationMode>> types{
        {visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::Classification, visionaiflow::project_store::ClassificationMode::SingleLabel},
        {visionaiflow::project_store::ProjectType::Classification, visionaiflow::project_store::ClassificationMode::MultiLabel},
        {visionaiflow::project_store::ProjectType::InstanceSegmentation, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::SemanticSegmentation, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::AnomalyDetection, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::LineDetection, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::OcrDetection, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::OcrRecognition, visionaiflow::project_store::ClassificationMode::NotApplicable},
        {visionaiflow::project_store::ProjectType::OcrPipeline, visionaiflow::project_store::ClassificationMode::NotApplicable}};
    for (const auto &[type, mode] : types)
    {
        const auto root = QDir(temporaryDirectory.Path()).filePath(visionaiflow::project_store::ToString(type) + QLatin1Char('-') + visionaiflow::project_store::ToString(mode));
        const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), type, mode, 1};
        const auto created = store.Create(root, definition);
        QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
        const auto reopened = store.Open(root);
        QVERIFY2(reopened.IsSuccess(), reopened.IsSuccess() ? "" : reopened.Failure().message.c_str());
        QCOMPARE(reopened.Value().projectId, definition.projectId);
        QVERIFY(reopened.Value().type == definition.type);
        QVERIFY(reopened.Value().classificationMode == definition.classificationMode);
    }
}

void ProjectStoreTest::RejectsMutableClassificationConstraint()
{
    const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::MultiLabel, 1};
    const auto result = visionaiflow::project_store::ValidateProjectDefinition(definition);
    QVERIFY(!result.IsSuccess());
    QVERIFY(!result.Failure().message.empty());
}

void ProjectStoreTest::RejectsIncompleteProjectLayout()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_layout_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString root = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("incomplete"));
    QVERIFY(QDir().mkpath(root));
    visionaiflow::project_store::ProjectStore store;
    const auto opened = store.Open(root);
    QVERIFY(!opened.IsSuccess());
    QVERIFY(!opened.Failure().message.empty());
}

void ProjectStoreTest::RejectsTamperedProjectJsonSchema()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_json_schema_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString missingCreatedRoot = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("missing_created"));
    const QString extraFieldRoot = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("extra_field"));
    const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore store;
    QVERIFY(store.Create(missingCreatedRoot, definition).IsSuccess());
    QVERIFY(store.Create(extraFieldRoot, definition).IsSuccess());

    auto rewriteProject = [](const QString &root, const std::function<void(QJsonObject &)> &mutate) -> bool {
        const QString path = QDir(root).filePath(QStringLiteral("project.json"));
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

    QVERIFY(rewriteProject(missingCreatedRoot, [](QJsonObject &object) { object.remove(QStringLiteral("createdUtc")); }));
    const auto missingCreated = store.Open(missingCreatedRoot);
    QVERIFY(!missingCreated.IsSuccess());
    QVERIFY(!missingCreated.Failure().message.empty());

    QVERIFY(rewriteProject(extraFieldRoot, [](QJsonObject &object) { object.insert(QStringLiteral("mutableProjectType"), true); }));
    const auto extraField = store.Open(extraFieldRoot);
    QVERIFY(!extraField.IsSuccess());
    QVERIFY(!extraField.Failure().message.empty());
}

void ProjectStoreTest::RejectsUnsupportedProjectSchemaVersions()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_schema_version_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    visionaiflow::project_store::ProjectStore store;
    const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};

    auto createAndRewriteProject = [&](const QString &name, const int schemaVersion) -> QString {
        const QString root = QDir(temporaryDirectory.Path()).filePath(name);
        const auto created = store.Create(root, definition);
        if (!created.IsSuccess()) return {};
        const QString path = QDir(root).filePath(QStringLiteral("project.json"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        object.insert(QStringLiteral("schemaVersion"), schemaVersion);
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly)) return {};
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (output.write(bytes) != bytes.size() || !output.commit()) return {};
        return root;
    };

    const QString futureRoot = createAndRewriteProject(QStringLiteral("future"), 2);
    QVERIFY(!futureRoot.isEmpty());
    const auto future = store.Open(futureRoot);
    QVERIFY(!future.IsSuccess());
    QCOMPARE(future.Failure().code, visionaiflow::foundation::ErrorCode::UnsupportedOperation);
    QVERIFY(!future.Failure().message.empty());

    const QString legacyRoot = createAndRewriteProject(QStringLiteral("legacy"), 0);
    QVERIFY(!legacyRoot.isEmpty());
    const auto legacy = store.Open(legacyRoot);
    QVERIFY(!legacy.IsSuccess());
    QVERIFY(!legacy.Failure().message.empty());
}

void ProjectStoreTest::AcquiresAndReleasesProjectLock()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_lock_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    visionaiflow::project_store::ProjectLock lock;
    const auto acquired = lock.Acquire(temporaryDirectory.Path(), QStringLiteral("0.1.0"));
    QVERIFY2(acquired.IsSuccess(), acquired.IsSuccess() ? "" : acquired.Failure().message.c_str());
    QVERIFY(QFile::exists(QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project.lock"))));
    QVERIFY(lock.Release().IsSuccess());
    QVERIFY(!QFile::exists(QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project.lock"))));
}

void ProjectStoreTest::RejectsConcurrentProjectLock()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_lock_concurrent_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    visionaiflow::project_store::ProjectLock firstLock;
    const auto first = firstLock.Acquire(temporaryDirectory.Path(), QStringLiteral("0.1.0"));
    QVERIFY2(first.IsSuccess(), first.IsSuccess() ? "" : first.Failure().message.c_str());
    visionaiflow::project_store::ProjectLock secondLock;
    const auto second = secondLock.Acquire(temporaryDirectory.Path(), QStringLiteral("0.1.0"));
    QVERIFY(!second.IsSuccess());
    QVERIFY(!second.Failure().message.empty());
    QVERIFY(QFile::exists(QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project.lock"))));
    QVERIFY(firstLock.Release().IsSuccess());
}

void ProjectStoreTest::RejectsMalformedProjectLockWithoutDeletingIt()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_project_lock_malformed_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString lockPath = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project.lock"));
    QFile file(lockPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray malformedLock = QJsonDocument(QJsonObject{{QStringLiteral("pid"), 1}}).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(malformedLock), static_cast<qint64>(malformedLock.size()));
    file.close();
    visionaiflow::project_store::ProjectLock lock;
    const auto acquired = lock.Acquire(temporaryDirectory.Path(), QStringLiteral("0.1.0"));
    QVERIFY(!acquired.IsSuccess());
    QVERIFY(!acquired.Failure().message.empty());
    QVERIFY(QFile::exists(lockPath));
}

void ProjectStoreTest::ImportsImageAndRejectsDuplicateHash()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_dataset_import_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString sourcePath = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("source.png"));
    QImage source(3, 2, QImage::Format_RGBA8888);
    source.fill(Qt::red);
    QVERIFY2(source.save(sourcePath), "Unable to create image fixture");

    const QString projectRoot = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project"));
    const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore store;
    const auto created = store.Create(projectRoot, definition);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());

    visionaiflow::project_store::DatasetIndex index;
    const auto imported = index.ImportImage(projectRoot, sourcePath);
    QVERIFY2(imported.IsSuccess(), imported.IsSuccess() ? "" : imported.Failure().message.c_str());
    QVERIFY(QFile::exists(QDir(projectRoot).filePath(imported.Value().relativePath)));
    QCOMPARE(imported.Value().size, QSize(3, 2));
    const auto loaded = index.Load(projectRoot);
    QVERIFY2(loaded.IsSuccess(), loaded.IsSuccess() ? "" : loaded.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(loaded.Value().size()), 1);
    QCOMPARE(loaded.Value().front().sha256, imported.Value().sha256);
    const auto duplicate = index.ImportImage(projectRoot, sourcePath);
    QVERIFY(!duplicate.IsSuccess());
    QVERIFY(!duplicate.Failure().message.empty());
}

void ProjectStoreTest::RejectsTamperedDatasetIndex()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_dataset_tamper_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString sourcePath = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("source.png"));
    QImage source(3, 2, QImage::Format_RGBA8888);
    source.fill(Qt::blue);
    QVERIFY2(source.save(sourcePath), "Unable to create image fixture");

    auto createProjectWithImage = [&](const QString &name) -> QString {
        const QString projectRoot = QDir(temporaryDirectory.Path()).filePath(name);
        const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
        visionaiflow::project_store::ProjectStore store;
        const auto created = store.Create(projectRoot, definition);
        if (!created.IsSuccess()) return {};
        visionaiflow::project_store::DatasetIndex index;
        const auto imported = index.ImportImage(projectRoot, sourcePath);
        if (!imported.IsSuccess()) return {};
        return projectRoot;
    };

    auto rewriteIndex = [](const QString &root, const std::function<void(QJsonObject &)> &mutate) -> bool {
        const QString path = QDir(root).filePath(QStringLiteral("data/index.json"));
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

    const QString badHashRoot = createProjectWithImage(QStringLiteral("bad_hash"));
    QVERIFY(!badHashRoot.isEmpty());
    QVERIFY(rewriteIndex(badHashRoot, [](QJsonObject &object) {
        QJsonArray images = object.value(QStringLiteral("images")).toArray();
        QJsonObject image = images.at(0).toObject();
        image.insert(QStringLiteral("sha256"), QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"));
        images.replace(0, image);
        object.insert(QStringLiteral("images"), images);
    }));
    visionaiflow::project_store::DatasetIndex index;
    const auto badHash = index.Load(badHashRoot);
    QVERIFY(!badHash.IsSuccess());
    QVERIFY(!badHash.Failure().message.empty());

    const QString unsafePathRoot = createProjectWithImage(QStringLiteral("unsafe_path"));
    QVERIFY(!unsafePathRoot.isEmpty());
    QVERIFY(rewriteIndex(unsafePathRoot, [](QJsonObject &object) {
        QJsonArray images = object.value(QStringLiteral("images")).toArray();
        QJsonObject image = images.at(0).toObject();
        image.insert(QStringLiteral("relativePath"), QStringLiteral("../outside.png"));
        images.replace(0, image);
        object.insert(QStringLiteral("images"), images);
    }));
    const auto unsafePath = index.Load(unsafePathRoot);
    QVERIFY(!unsafePath.IsSuccess());
    QVERIFY(!unsafePath.Failure().message.empty());

    const QString extraRootFieldRoot = createProjectWithImage(QStringLiteral("extra_root_field"));
    QVERIFY(!extraRootFieldRoot.isEmpty());
    QVERIFY(rewriteIndex(extraRootFieldRoot, [](QJsonObject &object) {
        object.insert(QStringLiteral("unregisteredMigrationHint"), QStringLiteral("ignored"));
    }));
    const auto extraRootField = index.Load(extraRootFieldRoot);
    QVERIFY(!extraRootField.IsSuccess());
    QVERIFY(!extraRootField.Failure().message.empty());

    const QString extraImageFieldRoot = createProjectWithImage(QStringLiteral("extra_image_field"));
    QVERIFY(!extraImageFieldRoot.isEmpty());
    QVERIFY(rewriteIndex(extraImageFieldRoot, [](QJsonObject &object) {
        QJsonArray images = object.value(QStringLiteral("images")).toArray();
        QJsonObject image = images.at(0).toObject();
        image.insert(QStringLiteral("mutableLabelCache"), true);
        images.replace(0, image);
        object.insert(QStringLiteral("images"), images);
    }));
    const auto extraImageField = index.Load(extraImageFieldRoot);
    QVERIFY(!extraImageField.IsSuccess());
    QVERIFY(!extraImageField.Failure().message.empty());
}

void ProjectStoreTest::SavesAndRejectsInvalidLabels()
{
    ScopedTestDirectory temporaryDirectory(QStringLiteral(".vaf_label_store_test_"));
    QVERIFY(temporaryDirectory.IsValid());
    const QString projectRoot = QDir(temporaryDirectory.Path()).filePath(QStringLiteral("project"));
    const visionaiflow::project_store::ProjectDefinition definition{QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("test"), visionaiflow::project_store::ProjectType::Detection, visionaiflow::project_store::ClassificationMode::NotApplicable, 1};
    visionaiflow::project_store::ProjectStore store;
    const auto created = store.Create(projectRoot, definition);
    QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());

    visionaiflow::project_store::LabelStore labels;
    const auto empty = labels.Load(projectRoot);
    QVERIFY2(empty.IsSuccess(), empty.IsSuccess() ? "" : empty.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(empty.Value().size()), 0);

    const auto added = labels.AddLabel(projectRoot, QStringLiteral("part-a"), QStringLiteral("#22AAFF"));
    QVERIFY2(added.IsSuccess(), added.IsSuccess() ? "" : added.Failure().message.c_str());
    const auto loaded = labels.Load(projectRoot);
    QVERIFY2(loaded.IsSuccess(), loaded.IsSuccess() ? "" : loaded.Failure().message.c_str());
    QCOMPARE(static_cast<qsizetype>(loaded.Value().size()), 1);
    QCOMPARE(loaded.Value().front().name, QStringLiteral("part-a"));
    QCOMPARE(loaded.Value().front().colorHex, QStringLiteral("#22AAFF"));

    std::vector<visionaiflow::project_store::LabelDefinition> duplicateNames{
        {QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("scratch"), QStringLiteral("#000001")},
        {QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("Scratch"), QStringLiteral("#000002")}};
    const auto duplicateName = labels.Save(projectRoot, duplicateNames);
    QVERIFY(!duplicateName.IsSuccess());
    QVERIFY(!duplicateName.Failure().message.empty());

    const auto invalidColor = labels.AddLabel(projectRoot, QStringLiteral("bad"), QStringLiteral("red"));
    QVERIFY(!invalidColor.IsSuccess());
    QVERIFY(!invalidColor.Failure().message.empty());

    const QString labelsPath = QDir(projectRoot).filePath(QStringLiteral("labels.json"));
    QFile file(labelsPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    root.insert(QStringLiteral("cache"), true);
    QSaveFile output(labelsPath);
    QVERIFY(output.open(QIODevice::WriteOnly));
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QCOMPARE(output.write(bytes), static_cast<qint64>(bytes.size()));
    QVERIFY(output.commit());
    const auto tampered = labels.Load(projectRoot);
    QVERIFY(!tampered.IsSuccess());
    QVERIFY(!tampered.Failure().message.empty());
}

QTEST_APPLESS_MAIN(ProjectStoreTest)

#include "tst_ProjectStore.moc"
