#pragma once

#include "visionaiflow/foundation/Result.h"

#ifdef slots
#pragma push_macro("slots")
#undef slots
#define VAF_RESTORE_QT_SLOTS_MACRO
#endif
#include <torch/torch.h>
#ifdef VAF_RESTORE_QT_SLOTS_MACRO
#pragma pop_macro("slots")
#undef VAF_RESTORE_QT_SLOTS_MACRO
#endif

#include <cstdint>
#include <vector>

namespace visionaiflow::training
{
struct DataLoaderOptions final
{
    int64_t batchSize{1};
    uint64_t randomSeed{0};
    bool shuffle{true};
    bool pinMemory{false};
};

struct TensorBatch final
{
    torch::Tensor features;
    torch::Tensor targets;
};

class TensorDataLoader final
{
public:
    static foundation::Result<TensorDataLoader> Create(torch::Tensor features, torch::Tensor targets, DataLoaderOptions options);
    foundation::Result<std::vector<TensorBatch>> NextEpoch();
    int64_t SampleCount() const noexcept;

private:
    TensorDataLoader(torch::Tensor features, torch::Tensor targets, DataLoaderOptions options);

    torch::Tensor m_features;
    torch::Tensor m_targets;
    DataLoaderOptions m_options;
    uint64_t m_epoch{0};
};
}
