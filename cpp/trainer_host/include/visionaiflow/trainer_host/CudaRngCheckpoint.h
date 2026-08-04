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

#include <vector>

namespace visionaiflow::trainer_host
{
foundation::Result<std::vector<torch::Tensor>> CaptureCudaRngStates();
foundation::Result<void> RestoreCudaRngStates(const std::vector<torch::Tensor> &states);
}
