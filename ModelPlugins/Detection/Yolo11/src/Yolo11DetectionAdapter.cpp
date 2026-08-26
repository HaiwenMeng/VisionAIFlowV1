#include "visionaiflow/models/yolo11/Yolo11DetectionAdapter.h"

#include "visionaiflow/models/api/ModelRegistry.h"

#include <memory>
#include <utility>

namespace visionaiflow::models::yolo11
{
namespace
{
foundation::Result<void> ValidateVariant(const QString &variant)
{
    if (variant != QStringLiteral("tiny") && variant != QStringLiteral("grid")) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 variant must be tiny or grid"));
    return foundation::Result<void>::Success();
}
}

Yolo11DetectionAdapter::Yolo11DetectionAdapter(QString variant) : m_variant(std::move(variant))
{
    const auto variantValid = ValidateVariant(m_variant);
    if (!variantValid.IsSuccess()) m_variant.clear();
    m_descriptor.modelId = QStringLiteral("detection.yolo11.").append(m_variant).append(QStringLiteral(".v1"));
    m_descriptor.adapterVersion = QStringLiteral("1.0.0");
    m_descriptor.displayName = QStringLiteral("YOLO11 ").append(m_variant);
    m_descriptor.projectTypes = {domain::ProjectType::Detection};
    m_descriptor.capabilities = api::ModelCapability::Train | api::ModelCapability::Resume | api::ModelCapability::Evaluate | api::ModelCapability::ExportOnnx | api::ModelCapability::Decode;
    m_descriptor.signature.inputs = {{QStringLiteral("images"), QStringLiteral("float32"), {-1, 3, -1, -1}}};
    m_descriptor.signature.outputs = {{QStringLiteral("detections"), QStringLiteral("float32"), {-1, -1, -1}}};
    m_descriptor.signature.decoderId = QStringLiteral("detection.yolo11.v1");
    m_descriptor.configSchemaVersion = 1;
    m_descriptor.artifactContractVersion = 1;
    m_descriptor.artifactFormats = {QStringLiteral("pt"), QStringLiteral("onnx")};
}

const api::ModelDescriptor &Yolo11DetectionAdapter::Descriptor() const noexcept { return m_descriptor; }

foundation::Result<void> Yolo11DetectionAdapter::ValidateConfiguration(const api::ModelConfigurationRequest &request) const
{
    if (m_variant.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "YOLO11 adapter variant initialization failed"));
    if (request.modelId != m_descriptor.modelId || request.schemaVersion != m_descriptor.configSchemaVersion) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "YOLO11 model id or configuration schema version is unsupported"));
    const QJsonValue classCount = request.configuration.value(QStringLiteral("classCount"));
    if (!classCount.isDouble() || classCount.toInt() <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 configuration requires a positive classCount"));
    const QString requestedVariant = request.configuration.value(QStringLiteral("variant")).toString();
    if (requestedVariant != m_variant) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 configuration variant does not match modelId"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> Yolo11DetectionAdapter::ValidateDataset(const api::DatasetDescriptor &dataset) const
{
    if (dataset.projectType != domain::ProjectType::Detection) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 adapter only accepts detection datasets"));
    if (dataset.datasetId.trimmed().isEmpty() || dataset.classNames.isEmpty() || dataset.sampleCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "YOLO11 dataset descriptor is incomplete"));
    return foundation::Result<void>::Success();
}

foundation::Result<void> Yolo11DetectionAdapter::ValidateDetectionDataset(const detection::DetectionDatasetContract &dataset) const { return detection::ValidateDetectionDatasetContract(dataset); }

QStringList Yolo11DetectionAdapter::SupportedVariants() const { return {QStringLiteral("tiny"), QStringLiteral("grid")}; }

foundation::Result<void> RegisterYolo11DetectionAdapters(api::ModelRegistry &registry)
{
    for (const QString &variant : {QStringLiteral("tiny"), QStringLiteral("grid")})
    {
        Yolo11DetectionAdapter descriptorSource(variant);
        const auto registered = registry.Register(descriptorSource.Descriptor(), [variant]() -> foundation::Result<api::ModelAdapterPtr> {
            return foundation::Result<api::ModelAdapterPtr>::Success(std::make_unique<Yolo11DetectionAdapter>(variant));
        });
        if (!registered.IsSuccess()) return registered;
    }
    return foundation::Result<void>::Success();
}
}
