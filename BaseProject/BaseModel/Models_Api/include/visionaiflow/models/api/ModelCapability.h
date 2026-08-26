#pragma once

#include <QFlags>

namespace visionaiflow::models::api
{
enum class ModelCapability : unsigned int
{
    None = 0,
    Train = 1U << 0U,
    Resume = 1U << 1U,
    Evaluate = 1U << 2U,
    ExportOnnx = 1U << 3U,
    Decode = 1U << 4U
};
Q_DECLARE_FLAGS(ModelCapabilities, ModelCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ModelCapabilities)
}
