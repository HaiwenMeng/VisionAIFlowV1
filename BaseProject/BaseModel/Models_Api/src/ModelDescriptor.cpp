#include "visionaiflow/models/api/ModelDescriptor.h"

namespace visionaiflow::models::api
{
foundation::Result<void> ValidateModelDescriptor(const ModelDescriptor &descriptor)
{
    if (descriptor.modelId.trimmed().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model id must not be empty"));
    if (descriptor.adapterVersion.trimmed().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Adapter version must not be empty"));
    if (descriptor.projectTypes.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model must support at least one project type"));
    if (descriptor.signature.inputs.isEmpty() || descriptor.signature.outputs.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model must define input and output signatures"));
    if (descriptor.configSchemaVersion <= 0 || descriptor.artifactContractVersion <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model schema and artifact contract versions must be positive"));
    return foundation::Result<void>::Success();
}
}
