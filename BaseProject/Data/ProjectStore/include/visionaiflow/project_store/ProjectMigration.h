#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QJsonObject>
#include <QVector>

#if defined(VISIONAIFLOW_PROJECT_STORE_LIBRARY)
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::project_store
{
constexpr int CurrentProjectSchemaVersion = 2;

struct ProjectMigrationResult final
{
    QJsonObject project;
    QVector<int> appliedSourceVersions;
};

VISIONAIFLOW_PROJECT_STORE_EXPORT foundation::Result<ProjectMigrationResult> MigrateProjectJson(const QJsonObject &project);
}
