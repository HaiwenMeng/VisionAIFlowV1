#pragma once

#include "visionaiflow/models/api/IModelAdapter.h"

#include <QVector>

namespace visionaiflow::models::classification
{
class IClassificationModelAdapter : public api::IModelAdapter
{
public:
    ~IClassificationModelAdapter() override = default;

    [[nodiscard]] virtual QVector<domain::ClassificationMode> SupportedModes() const = 0;
    [[nodiscard]] virtual foundation::Result<void> ValidateClassCount(int64_t classCount) const = 0;
};
}
