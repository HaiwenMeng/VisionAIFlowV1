#pragma once

#include "visionaiflow/models/classification/IClassificationModelAdapter.h"
#include "visionaiflow/models/classification/linear/LinearClassifier.h"

namespace visionaiflow::models::classification::linear
{
class VISIONAIFLOW_LINEAR_EXPORT LinearClassificationAdapter final : public IClassificationModelAdapter
{
public:
    LinearClassificationAdapter();

    [[nodiscard]] const api::ModelDescriptor &Descriptor() const noexcept override;
    [[nodiscard]] foundation::Result<void> ValidateConfiguration(const api::ModelConfigurationRequest &request) const override;
    [[nodiscard]] foundation::Result<void> ValidateDataset(const api::DatasetDescriptor &dataset) const override;
    [[nodiscard]] QVector<domain::ClassificationMode> SupportedModes() const override;
    [[nodiscard]] foundation::Result<void> ValidateClassCount(int64_t classCount) const override;
    [[nodiscard]] foundation::Result<LinearClassifier> CreateClassifier(int64_t inputFeatures, int64_t classCount) const;

private:
    api::ModelDescriptor m_descriptor;
};
}
