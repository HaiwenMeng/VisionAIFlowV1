#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

#include <vector>

#if defined(VISIONAIFLOW_PROJECT_STORE_LIBRARY)
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_PROJECT_STORE_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::project_store
{
struct LabelDefinition final
{
    QString labelId;
    QString name;
    QString colorHex;
};

class VISIONAIFLOW_PROJECT_STORE_EXPORT LabelStore final
{
public:
    foundation::Result<std::vector<LabelDefinition>> Load(const QString &projectRoot) const;
    foundation::Result<void> Save(const QString &projectRoot, const std::vector<LabelDefinition> &labels) const;
    foundation::Result<LabelDefinition> AddLabel(const QString &projectRoot, const QString &name, const QString &colorHex) const;
};

VISIONAIFLOW_PROJECT_STORE_EXPORT foundation::Result<void> ValidateLabelDefinition(const LabelDefinition &label);
}
