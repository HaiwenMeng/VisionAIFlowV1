#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

#if defined(VISIONAIFLOW_DOMAIN_LIBRARY)
#define VISIONAIFLOW_DOMAIN_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_DOMAIN_EXPORT __declspec(dllimport)
#endif

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

VISIONAIFLOW_DOMAIN_EXPORT QString ToString(ProjectType type);
VISIONAIFLOW_DOMAIN_EXPORT QString ToString(ClassificationMode mode);
VISIONAIFLOW_DOMAIN_EXPORT foundation::Result<ProjectType> ProjectTypeFromString(const QString &value);
VISIONAIFLOW_DOMAIN_EXPORT foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value);
}
