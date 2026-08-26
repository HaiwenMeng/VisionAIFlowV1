#include "visionaiflow/models/detection/DetectionContracts.h"

namespace visionaiflow::models::detection
{
foundation::Result<void> ValidateDetectionDatasetContract(const DetectionDatasetContract &contract)
{
    if (contract.datasetId.trimmed().isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection dataset id must not be empty"));
    if (contract.classNames.isEmpty() || contract.sampleCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection dataset requires classes and at least one sample"));
    if (contract.imageWidth <= 0 || contract.imageHeight <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Detection dataset image dimensions must be positive"));
    return foundation::Result<void>::Success();
}
}
