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

#ifndef VISIONAIFLOW_TRAINING_EXPORT
#if defined(VISIONAIFLOW_TRAINING_LIBRARY)
#define VISIONAIFLOW_TRAINING_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_TRAINING_EXPORT __declspec(dllimport)
#endif
#endif

namespace visionaiflow::training
{
enum class PrecisionMode
{
    Fp32,
    AmpFp16
};

struct AmpSettings final
{
    double initialScale{65536.0};
    double growthFactor{2.0};
    double backoffFactor{0.5};
    int64_t growthInterval{2000};
};

struct AmpState final
{
    PrecisionMode mode{PrecisionMode::Fp32};
    double scale{1.0};
    int64_t consecutiveFiniteSteps{0};
};

class VISIONAIFLOW_TRAINING_EXPORT AmpController final
{
public:
    static foundation::Result<AmpController> Create(PrecisionMode mode, const torch::Device &device, const AmpSettings &settings = {});

    [[nodiscard]] AmpState State() const noexcept;
    foundation::Result<void> RestoreState(const AmpState &state);
    foundation::Result<torch::Tensor> ScaleLoss(const torch::Tensor &loss) const;
    foundation::Result<bool> UnscaleAndStep(torch::optim::Optimizer &optimizer, const std::vector<torch::Tensor> &parameters, double maxGradientNorm = 0.0);

private:
    AmpController(PrecisionMode mode, AmpSettings settings, double scale);
    void RecordFiniteStep() noexcept;
    void RecordOverflow() noexcept;

    PrecisionMode m_mode;
    AmpSettings m_settings;
    double m_scale;
    int64_t m_consecutiveFiniteSteps{0};
};
}
