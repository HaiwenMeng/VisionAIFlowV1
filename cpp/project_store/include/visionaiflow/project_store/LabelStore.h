#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

#include <vector>

namespace visionaiflow::project_store
{
struct LabelDefinition final
{
    QString labelId;
    QString name;
    QString colorHex;
};

class LabelStore final
{
public:
    foundation::Result<std::vector<LabelDefinition>> Load(const QString &projectRoot) const;
    foundation::Result<void> Save(const QString &projectRoot, const std::vector<LabelDefinition> &labels) const;
    foundation::Result<LabelDefinition> AddLabel(const QString &projectRoot, const QString &name, const QString &colorHex) const;
};

foundation::Result<void> ValidateLabelDefinition(const LabelDefinition &label);
}
