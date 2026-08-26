#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/api/ModelDescriptor.h"
#include "visionaiflow/models/api/ModelRequests.h"

#include <memory>

namespace visionaiflow::models::api
{
class IModelAdapter
{
public:
    virtual ~IModelAdapter() = default;

    [[nodiscard]] virtual const ModelDescriptor &Descriptor() const noexcept = 0;
    [[nodiscard]] virtual foundation::Result<void> ValidateConfiguration(const ModelConfigurationRequest &request) const = 0;
    [[nodiscard]] virtual foundation::Result<void> ValidateDataset(const DatasetDescriptor &dataset) const = 0;
};

using ModelAdapterPtr = std::unique_ptr<IModelAdapter>;
}
