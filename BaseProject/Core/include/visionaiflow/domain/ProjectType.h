#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

namespace visionaiflow::domain
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

VISIONAIFLOW_CORE_EXPORT QString ToString(ProjectType type);
VISIONAIFLOW_CORE_EXPORT QString ToString(ClassificationMode mode);
VISIONAIFLOW_CORE_EXPORT foundation::Result<ProjectType> ProjectTypeFromString(const QString &value);
VISIONAIFLOW_CORE_EXPORT foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value);
} // namespace visionaiflow::domain
