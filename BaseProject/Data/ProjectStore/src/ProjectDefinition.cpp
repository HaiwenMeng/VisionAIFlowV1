#include "visionaiflow/project_store/ProjectDefinition.h"
#include "visionaiflow/project_store/ProjectMigration.h"

#include <QUuid>

namespace visionaiflow::project_store
{
foundation::Result<void> ValidateProjectDefinition(const ProjectDefinition &definition)
{
    if (definition.projectId.isEmpty() || QUuid(definition.projectId).isNull())
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project id must be a valid UUID"));
    }
    if (definition.name.trimmed().isEmpty())
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project name must not be empty"));
    }
    if (definition.type == ProjectType::Classification && definition.classificationMode == ClassificationMode::NotApplicable)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Classification project requires single_label or multi_label mode"));
    }
    if (definition.type != ProjectType::Classification && definition.classificationMode != ClassificationMode::NotApplicable)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Only classification projects may specify classificationMode"));
    }
    if (definition.schemaVersion != CurrentProjectSchemaVersion)
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::UnsupportedOperation, "Unsupported project schema version"));
    }
    if (definition.modelId != definition.modelId.trimmed())
    {
        return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project modelId must not contain leading or trailing whitespace"));
    }
    return foundation::Result<void>::Success();
}

QString ToString(const ProjectType type)
{
    return domain::ToString(type);
}

QString ToString(const ClassificationMode mode)
{
    return domain::ToString(mode);
}

foundation::Result<ProjectType> ProjectTypeFromString(const QString &value)
{
    return domain::ProjectTypeFromString(value);
}

foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value)
{
    return domain::ClassificationModeFromString(value);
}
}
