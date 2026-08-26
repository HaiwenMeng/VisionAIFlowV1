#include "visionaiflow/models/classification/linear/LinearClassificationAdapter.h"

namespace visionaiflow::models::classification::linear
{
LinearClassificationAdapter::LinearClassificationAdapter()
{
    m_descriptor.modelId = QStringLiteral("classification.linear.v1");
    m_descriptor.adapterVersion = QStringLiteral("1.0.0");
    m_descriptor.displayName = QStringLiteral("Linear Classification");
    m_descriptor.projectTypes = {domain::ProjectType::Classification};
    m_descriptor.capabilities = api::ModelCapability::Train | api::ModelCapability::Resume | api::ModelCapability::Evaluate | api::ModelCapability::ExportOnnx;
    m_descriptor.signature.inputs = {{QStringLiteral("features"), QStringLiteral("float32"), {-1, -1}}};
    m_descriptor.signature.outputs = {{QStringLiteral("logits"), QStringLiteral("float32"), {-1, -1}}};
    m_descriptor.configSchemaVersion = 1;
    m_descriptor.artifactContractVersion = 1;
    m_descriptor.artifactFormats = {QStringLiteral("pt"), QStringLiteral("onnx")};
}

const api::ModelDescriptor &LinearClassificationAdapter::Descriptor() const noexcept { return m_descriptor; }

foundation::Result<void> LinearClassificationAdapter::ValidateConfiguration(const api::ModelConfigurationRequest &request) const
{
    if (request.modelId != m_descriptor.modelId) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classification configuration model id is unsupported"));
    if (request.schemaVersion != m_descriptor.configSchemaVersion) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Linear classification configuration schema version is unsupported"));
    const QJsonValue inputFeatures = request.configuration.value(QStringLiteral("inputFeatures"));
    const QJsonValue classCount = request.configuration.value(QStringLiteral("classCount"));
    if (!inputFeatures.isDouble() || inputFeatures.toInt() <= 0 || !classCount.isDouble()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classification configuration requires positive integer inputFeatures and classCount"));
    return ValidateClassCount(classCount.toInt());
}

foundation::Result<void> LinearClassificationAdapter::ValidateDataset(const api::DatasetDescriptor &dataset) const
{
    if (dataset.projectType != domain::ProjectType::Classification) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classification adapter only accepts classification datasets"));
    if (dataset.datasetId.trimmed().isEmpty() || dataset.sampleCount <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classification dataset id and sample count must be valid"));
    return ValidateClassCount(dataset.classNames.size());
}

QVector<domain::ClassificationMode> LinearClassificationAdapter::SupportedModes() const { return {domain::ClassificationMode::SingleLabel, domain::ClassificationMode::MultiLabel}; }

foundation::Result<void> LinearClassificationAdapter::ValidateClassCount(const int64_t classCount) const
{
    if (classCount < 2) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Linear classification requires at least two classes"));
    return foundation::Result<void>::Success();
}

foundation::Result<LinearClassifier> LinearClassificationAdapter::CreateClassifier(const int64_t inputFeatures, const int64_t classCount) const
{
    const auto classCountValidation = ValidateClassCount(classCount);
    if (!classCountValidation.IsSuccess()) return foundation::Result<LinearClassifier>::Failure(classCountValidation.Failure());
    return CreateLinearClassifier(inputFeatures, classCount);
}
}
