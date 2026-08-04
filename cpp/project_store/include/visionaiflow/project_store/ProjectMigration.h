#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QJsonObject>
#include <QVector>

namespace visionaiflow::project_store
{
constexpr int CurrentProjectSchemaVersion = 1;

struct ProjectMigrationResult final
{
    QJsonObject project;
    QVector<int> appliedSourceVersions;
};

foundation::Result<ProjectMigrationResult> MigrateProjectJson(const QJsonObject &project);
}
