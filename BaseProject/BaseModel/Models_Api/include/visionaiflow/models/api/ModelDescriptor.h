#pragma once

#include "visionaiflow/domain/ProjectType.h"
#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/api/ModelCapability.h"
#include "visionaiflow/models/api/ModelSignature.h"

#include <QString>
#include <QStringList>
#include <QVector>

#if defined(VISIONAIFLOW_MODELS_API_LIBRARY)
#define VISIONAIFLOW_MODELS_API_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_MODELS_API_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::models::api
{
struct ModelDescriptor
{
    QString modelId;
    QString adapterVersion;
    QString displayName;
    QVector<domain::ProjectType> projectTypes;
    ModelCapabilities capabilities;
    ModelSignature signature;
    int configSchemaVersion = 0;
    int artifactContractVersion = 0;
    QStringList artifactFormats;
};

VISIONAIFLOW_MODELS_API_EXPORT foundation::Result<void> ValidateModelDescriptor(const ModelDescriptor &descriptor);
}
