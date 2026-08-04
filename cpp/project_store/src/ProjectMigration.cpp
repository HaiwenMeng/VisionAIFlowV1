#include "visionaiflow/project_store/ProjectMigration.h"

#include <QString>

namespace visionaiflow::project_store
{
namespace
{
foundation::Result<int> ReadSchemaVersion(const QJsonObject &project)
{
    const QJsonValue value = project.value(QStringLiteral("schemaVersion"));
    if (!value.isDouble())
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "project.json schemaVersion is missing or not an integer"));
    }
    const int version = value.toInt(-1);
    if (version <= 0)
    {
        return foundation::Result<int>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "project.json schemaVersion must be a positive integer"));
    }
    return foundation::Result<int>::Success(version);
}
}

foundation::Result<ProjectMigrationResult> MigrateProjectJson(const QJsonObject &project)
{
    const auto version = ReadSchemaVersion(project);
    if (!version.IsSuccess()) return foundation::Result<ProjectMigrationResult>::Failure(version.Failure());
    if (version.Value() == CurrentProjectSchemaVersion)
    {
        return foundation::Result<ProjectMigrationResult>::Success({project, {}});
    }
    if (version.Value() > CurrentProjectSchemaVersion)
    {
        return foundation::Result<ProjectMigrationResult>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "project.json was created by a newer unsupported schema version"));
    }
    return foundation::Result<ProjectMigrationResult>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, QStringLiteral("No registered project schema migration path from version %1 to %2").arg(version.Value()).arg(CurrentProjectSchemaVersion).toStdString()));
}
}
