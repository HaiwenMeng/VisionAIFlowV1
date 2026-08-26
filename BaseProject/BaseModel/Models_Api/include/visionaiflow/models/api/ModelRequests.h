#pragma once

#include "visionaiflow/domain/ProjectType.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace visionaiflow::models::api
{
struct ModelConfigurationRequest
{
    QString modelId;
    int schemaVersion = 0;
    QJsonObject configuration;
};

struct DatasetDescriptor
{
    domain::ProjectType projectType = domain::ProjectType::Classification;
    QString datasetId;
    QStringList classNames;
    int sampleCount = 0;
};
}
