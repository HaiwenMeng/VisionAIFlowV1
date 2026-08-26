#include "visionaiflow/project_store/ProjectStore.h"

#include "visionaiflow/project_store/ProjectMigration.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStringList>

namespace visionaiflow::project_store
{
namespace
{
foundation::Result<void> WriteJsonAtomically(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, file.errorString().toStdString()));
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, file.errorString().toStdString()));
    if (!file.commit()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, file.errorString().toStdString()));
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateProjectLayout(const QString &projectRoot)
{
    const QFileInfo rootInfo(projectRoot);
    if (!rootInfo.exists() || !rootInfo.isDir()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root does not exist or is not a directory"));
    const QStringList requiredDirectories{QStringLiteral("data"), QStringLiteral("data/images"), QStringLiteral("data/annotations"), QStringLiteral("data/splits"), QStringLiteral("data/thumbnails"), QStringLiteral("runs"), QStringLiteral("models"), QStringLiteral("backups")};
    for (const QString &directory : requiredDirectories)
    {
        const QFileInfo info(QDir(projectRoot).filePath(directory));
        if (!info.exists() || !info.isDir()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Project directory is missing: ").append(directory).toStdString()));
    }
    const QStringList requiredFiles{QStringLiteral("project.json"), QStringLiteral("labels.json"), QStringLiteral("data/index.json")};
    for (const QString &file : requiredFiles)
    {
        const QFileInfo info(QDir(projectRoot).filePath(file));
        if (!info.exists() || !info.isFile()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("Project file is missing: ").append(file).toStdString()));
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> ValidateProjectJsonObject(const QJsonObject &object)
{
    const QSet<QString> requiredKeys{QStringLiteral("schemaVersion"), QStringLiteral("projectId"), QStringLiteral("name"), QStringLiteral("projectType"), QStringLiteral("classificationMode"), QStringLiteral("modelId"), QStringLiteral("createdUtc")};
    for (const QString &key : requiredKeys)
    {
        if (!object.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("project.json is missing required field: ").append(key).toStdString()));
    }
    for (const QString &key : object.keys())
    {
        if (!requiredKeys.contains(key)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("project.json contains an unsupported field: ").append(key).toStdString()));
    }
    const QJsonValue schemaVersion = object.value(QStringLiteral("schemaVersion"));
    if (!schemaVersion.isDouble() || schemaVersion.toInt(-1) != CurrentProjectSchemaVersion) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "project.json schemaVersion is unsupported"));
    for (const QString &key : {QStringLiteral("projectId"), QStringLiteral("name"), QStringLiteral("projectType"), QStringLiteral("classificationMode"), QStringLiteral("modelId"), QStringLiteral("createdUtc")})
    {
        if (!object.value(key).isString()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, QStringLiteral("project.json field must be a string: ").append(key).toStdString()));
    }
    const QDateTime createdUtc = QDateTime::fromString(object.value(QStringLiteral("createdUtc")).toString(), Qt::ISODateWithMs);
    if (!createdUtc.isValid() || createdUtc.timeSpec() != Qt::UTC) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "project.json createdUtc must be a valid UTC ISO timestamp"));
    return foundation::Result<void>::Success();
}

}

foundation::Result<void> ProjectStore::Create(const QString &projectRoot, const ProjectDefinition &definition) const
{
    ProjectDefinition persistedDefinition = definition;
    persistedDefinition.schemaVersion = CurrentProjectSchemaVersion;
    const auto validation = ValidateProjectDefinition(persistedDefinition);
    if (!validation.IsSuccess()) return validation;
    if (projectRoot.isEmpty() || QFileInfo(projectRoot).exists()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root must not exist before project creation"));
    if (!QDir().mkpath(projectRoot)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Failed to create project root: ").append(projectRoot).toStdString()));
    QDir projectDirectory(projectRoot);
    const QStringList directories{QStringLiteral("data/images"), QStringLiteral("data/annotations"), QStringLiteral("data/splits"), QStringLiteral("data/thumbnails"), QStringLiteral("runs"), QStringLiteral("models"), QStringLiteral("backups")};
    for (const QString &directory : directories)
    {
        const QString absoluteDirectory = projectDirectory.filePath(directory);
        if (!QDir().mkpath(absoluteDirectory)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, QStringLiteral("Failed to create project temporary directory: ").append(absoluteDirectory).toStdString()));
    }
    const auto writtenLabels = WriteJsonAtomically(projectDirectory.filePath(QStringLiteral("labels.json")), {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("labels"), QJsonArray{}}});
    if (!writtenLabels.IsSuccess()) return writtenLabels;
    const auto writtenIndex = WriteJsonAtomically(projectDirectory.filePath(QStringLiteral("data/index.json")), {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("images"), QJsonArray{}}});
    if (!writtenIndex.IsSuccess()) return writtenIndex;
    const QJsonObject project{{QStringLiteral("schemaVersion"), persistedDefinition.schemaVersion}, {QStringLiteral("projectId"), persistedDefinition.projectId}, {QStringLiteral("name"), persistedDefinition.name}, {QStringLiteral("projectType"), domain::ToString(persistedDefinition.type)}, {QStringLiteral("classificationMode"), domain::ToString(persistedDefinition.classificationMode)}, {QStringLiteral("modelId"), persistedDefinition.modelId}, {QStringLiteral("createdUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    const auto writtenProject = WriteJsonAtomically(projectDirectory.filePath(QStringLiteral("project.json")), project);
    if (!writtenProject.IsSuccess()) return writtenProject;
    const auto reopened = Open(projectRoot);
    if (!reopened.IsSuccess()) return foundation::Result<void>::Failure(reopened.Failure());
    return foundation::Result<void>::Success();
}

foundation::Result<ProjectDefinition> ProjectStore::Open(const QString &projectRoot) const
{
    const auto layout = ValidateProjectLayout(projectRoot);
    if (!layout.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(layout.Failure());
    QFile file(QDir(projectRoot).filePath(QStringLiteral("project.json")));
    if (!file.open(QIODevice::ReadOnly)) return foundation::Result<ProjectDefinition>::Failure(foundation::Error::Create(foundation::ErrorCode::IoFailure, "Unable to read project.json"));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return foundation::Result<ProjectDefinition>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "project.json is not valid JSON object"));
    const auto migrated = MigrateProjectJson(document.object());
    if (!migrated.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(migrated.Failure());
    const auto object = migrated.Value().project;
    const auto jsonValidation = ValidateProjectJsonObject(object);
    if (!jsonValidation.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(jsonValidation.Failure());
    const auto type = ProjectTypeFromString(object.value(QStringLiteral("projectType")).toString());
    if (!type.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(type.Failure());
    const auto mode = ClassificationModeFromString(object.value(QStringLiteral("classificationMode")).toString());
    if (!mode.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(mode.Failure());
    ProjectDefinition definition{object.value(QStringLiteral("projectId")).toString(), object.value(QStringLiteral("name")).toString(), type.Value(), mode.Value(), object.value(QStringLiteral("schemaVersion")).toInt(), object.value(QStringLiteral("modelId")).toString()};
    const auto validation = ValidateProjectDefinition(definition);
    if (!validation.IsSuccess()) return foundation::Result<ProjectDefinition>::Failure(validation.Failure());
    return foundation::Result<ProjectDefinition>::Success(std::move(definition));
}
}
