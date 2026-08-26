#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/api/IModelAdapter.h"

#include <functional>

namespace visionaiflow::models::api
{
class VISIONAIFLOW_MODELS_API_EXPORT ModelRegistry final
{
public:
    using Factory = std::function<foundation::Result<ModelAdapterPtr>()>;

    foundation::Result<void> Register(ModelDescriptor descriptor, Factory factory);
    foundation::Result<ModelAdapterPtr> Create(const QString &modelId, domain::ProjectType projectType) const;
    QVector<ModelDescriptor> FindByProjectType(domain::ProjectType projectType) const;

private:
    struct Entry
    {
        ModelDescriptor descriptor;
        Factory factory;
    };
    QHash<QString, Entry> m_entries;
};
}
