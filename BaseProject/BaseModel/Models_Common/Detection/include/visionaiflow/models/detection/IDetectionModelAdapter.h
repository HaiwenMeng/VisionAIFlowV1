#pragma once

#include "visionaiflow/models/api/IModelAdapter.h"
#include "visionaiflow/models/detection/DetectionContracts.h"

namespace visionaiflow::models::detection
{
class IDetectionModelAdapter : public api::IModelAdapter
{
public:
    ~IDetectionModelAdapter() override = default;

    [[nodiscard]] virtual foundation::Result<void> ValidateDetectionDataset(const DetectionDatasetContract &dataset) const = 0;
    [[nodiscard]] virtual QStringList SupportedVariants() const = 0;
};
}
