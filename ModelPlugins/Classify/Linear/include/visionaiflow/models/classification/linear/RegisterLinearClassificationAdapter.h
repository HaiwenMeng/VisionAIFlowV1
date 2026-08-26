#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/models/classification/linear/LinearClassifier.h"

namespace visionaiflow::models::api
{
class ModelRegistry;
}

namespace visionaiflow::models::classification::linear
{
VISIONAIFLOW_LINEAR_EXPORT foundation::Result<void> RegisterLinearClassificationAdapter(api::ModelRegistry &registry);
}
