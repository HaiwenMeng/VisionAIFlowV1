#pragma once

#include "visionaiflow/models/detection/IDetectionModelAdapter.h"
#include "visionaiflow/models/yolo11/Yolo11Detector.h"

namespace visionaiflow::models::api
{
class ModelRegistry;
}

namespace visionaiflow::models::yolo11
{
class VISIONAIFLOW_YOLO11_EXPORT Yolo11DetectionAdapter final : public detection::IDetectionModelAdapter
{
public:
    explicit Yolo11DetectionAdapter(QString variant);

    [[nodiscard]] const api::ModelDescriptor &Descriptor() const noexcept override;
    [[nodiscard]] foundation::Result<void> ValidateConfiguration(const api::ModelConfigurationRequest &request) const override;
    [[nodiscard]] foundation::Result<void> ValidateDataset(const api::DatasetDescriptor &dataset) const override;
    [[nodiscard]] foundation::Result<void> ValidateDetectionDataset(const detection::DetectionDatasetContract &dataset) const override;
    [[nodiscard]] QStringList SupportedVariants() const override;

private:
    QString m_variant;
    api::ModelDescriptor m_descriptor;
};

VISIONAIFLOW_YOLO11_EXPORT foundation::Result<void> RegisterYolo11DetectionAdapters(api::ModelRegistry &registry);
}
