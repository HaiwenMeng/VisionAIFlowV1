#include "visionaiflow/models/api/ModelRegistry.h"

namespace visionaiflow::models::api
{
foundation::Result<void> ModelRegistry::Register(ModelDescriptor descriptor, Factory factory)
{
    const auto validation = ValidateModelDescriptor(descriptor);
    if (!validation.IsSuccess()) return validation;
    if (!factory) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model factory must not be empty"));
    const QString modelId = descriptor.modelId;
    if (m_entries.contains(modelId)) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Model id is already registered"));
    m_entries.insert(modelId, Entry{std::move(descriptor), std::move(factory)});
    return foundation::Result<void>::Success();
}

foundation::Result<ModelAdapterPtr> ModelRegistry::Create(const QString &modelId, const domain::ProjectType projectType) const
{
    const auto iterator = m_entries.constFind(modelId);
    if (iterator == m_entries.cend()) return foundation::Result<ModelAdapterPtr>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Unknown model id"));
    if (!iterator->descriptor.projectTypes.contains(projectType)) return foundation::Result<ModelAdapterPtr>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Model does not support the requested project type"));
    auto adapter = iterator->factory();
    if (!adapter.IsSuccess()) return foundation::Result<ModelAdapterPtr>::Failure(adapter.Failure());
    if (!adapter.Value()) return foundation::Result<ModelAdapterPtr>::Failure(foundation::Error::Create(foundation::ErrorCode::InternalFailure, "Model factory returned an empty adapter"));
    return foundation::Result<ModelAdapterPtr>::Success(std::move(adapter.Value()));
}

QVector<ModelDescriptor> ModelRegistry::FindByProjectType(const domain::ProjectType projectType) const
{
    QVector<ModelDescriptor> descriptors;
    for (const auto &entry : m_entries) if (entry.descriptor.projectTypes.contains(projectType)) descriptors.append(entry.descriptor);
    return descriptors;
}
}
