#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/project_store/ProjectDefinition.h"

namespace visionaiflow::project_store
{
class VISIONAIFLOW_PROJECT_STORE_EXPORT ProjectStore final
{
public:
    foundation::Result<void> Create(const QString &projectRoot, const ProjectDefinition &definition) const;
    foundation::Result<ProjectDefinition> Open(const QString &projectRoot) const;
};
}
