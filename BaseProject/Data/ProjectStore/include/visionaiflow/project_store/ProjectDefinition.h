#pragma once

#include "visionaiflow/domain/ProjectType.h"

#include <QString>

#if defined(VISIONAIFLOW_PROJECT_STORE_LIBRARY)
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::project_store
{
using ProjectType = domain::ProjectType;
using ClassificationMode = domain::ClassificationMode;

struct ProjectDefinition
{
    QString projectId;
    QString name;
    ProjectType type;
    ClassificationMode classificationMode;
    int schemaVersion = 1;
    QString modelId;
};

VISIONAIFLOW_PROJECT_STORE_EXPORT foundation::Result<void> ValidateProjectDefinition(const ProjectDefinition &definition);
VISIONAIFLOW_PROJECT_STORE_EXPORT QString ToString(ProjectType type);
VISIONAIFLOW_PROJECT_STORE_EXPORT QString ToString(ClassificationMode mode);
VISIONAIFLOW_PROJECT_STORE_EXPORT foundation::Result<ProjectType> ProjectTypeFromString(const QString &value);
VISIONAIFLOW_PROJECT_STORE_EXPORT foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value);
}
