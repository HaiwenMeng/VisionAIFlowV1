#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

namespace visionaiflow::project_store
{
enum class ProjectType
{
    Detection,
    Classification,
    InstanceSegmentation,
    SemanticSegmentation,
    AnomalyDetection,
    LineDetection,
    OcrDetection,
    OcrRecognition,
    OcrPipeline
};

enum class ClassificationMode
{
    NotApplicable,
    SingleLabel,
    MultiLabel
};

struct ProjectDefinition
{
    QString projectId;
    QString name;
    ProjectType type;
    ClassificationMode classificationMode;
    int schemaVersion = 1;
};

foundation::Result<void> ValidateProjectDefinition(const ProjectDefinition &definition);
QString ToString(ProjectType type);
QString ToString(ClassificationMode mode);
foundation::Result<ProjectType> ProjectTypeFromString(const QString &value);
foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value);
}
