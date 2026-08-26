#include "visionaiflow/models/classification/linear/RegisterLinearClassificationAdapter.h"

#include "visionaiflow/models/api/ModelRegistry.h"
#include "visionaiflow/models/classification/linear/LinearClassificationAdapter.h"

#include <memory>

namespace visionaiflow::models::classification::linear
{
foundation::Result<void> RegisterLinearClassificationAdapter(api::ModelRegistry &registry)
{
    LinearClassificationAdapter descriptorSource;
    return registry.Register(descriptorSource.Descriptor(), []() -> foundation::Result<api::ModelAdapterPtr> {
        return foundation::Result<api::ModelAdapterPtr>::Success(std::make_unique<LinearClassificationAdapter>());
    });
}
}
