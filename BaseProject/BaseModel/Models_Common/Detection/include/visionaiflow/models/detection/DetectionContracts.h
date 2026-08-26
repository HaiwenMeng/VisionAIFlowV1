#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>

#if defined(VISIONAIFLOW_MODELS_COMMON_LIBRARY)
#define VISIONAIFLOW_MODELS_COMMON_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_MODELS_COMMON_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::models::detection
{
struct DetectionDatasetContract final
{
    QString datasetId;
    QStringList classNames;
    int sampleCount{0};
    int imageWidth{0};
    int imageHeight{0};
};

VISIONAIFLOW_MODELS_COMMON_EXPORT foundation::Result<void> ValidateDetectionDatasetContract(const DetectionDatasetContract &contract);
}
