#include "visionaiflow/training/AmpController.h"

#include <cmath>
#include <exception>

namespace visionaiflow::training
{
namespace
{
foundation::Result<void> ValidateSettings(const AmpSettings &settings)
{
    if (!std::isfinite(settings.initialScale) || settings.initialScale <= 0.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP initial scale must be finite and positive"));
    if (!std::isfinite(settings.growthFactor) || settings.growthFactor <= 1.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP growth factor must be finite and greater than one"));
    if (!std::isfinite(settings.backoffFactor) || settings.backoffFactor <= 0.0 || settings.backoffFactor >= 1.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP backoff factor must be finite and between zero and one"));
    if (settings.growthInterval <= 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP growth interval must be positive"));
    return foundation::Result<void>::Success();
}
}

AmpController::AmpController(const PrecisionMode mode, AmpSettings settings, const double scale) : m_mode(mode), m_settings(std::move(settings)), m_scale(scale) {}

foundation::Result<AmpController> AmpController::Create(const PrecisionMode mode, const torch::Device &device, const AmpSettings &settings)
{
    const auto settingsResult = ValidateSettings(settings);
    if (!settingsResult.IsSuccess()) return foundation::Result<AmpController>::Failure(settingsResult.Failure());
    if (mode == PrecisionMode::AmpFp16 && !device.is_cuda()) return foundation::Result<AmpController>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP FP16 requires a CUDA device and cannot fall back to CPU FP32"));
    return foundation::Result<AmpController>::Success(AmpController(mode, settings, mode == PrecisionMode::AmpFp16 ? settings.initialScale : 1.0));
}

AmpState AmpController::State() const noexcept { return {m_mode, m_scale, m_consecutiveFiniteSteps}; }

foundation::Result<void> AmpController::RestoreState(const AmpState &state)
{
    if (state.mode != m_mode) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP state precision mode does not match the controller"));
    if (!std::isfinite(state.scale) || state.scale <= 0.0 || state.consecutiveFiniteSteps < 0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "AMP state is invalid"));
    if (m_mode == PrecisionMode::Fp32 && state.scale != 1.0) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "FP32 AMP state must use scale one"));
    m_scale = state.scale;
    m_consecutiveFiniteSteps = state.consecutiveFiniteSteps;
    return foundation::Result<void>::Success();
}

foundation::Result<torch::Tensor> AmpController::ScaleLoss(const torch::Tensor &loss) const
{
    if (!loss.defined() || loss.numel() != 1) return foundation::Result<torch::Tensor>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP loss must be a defined scalar tensor"));
    try
    {
        if (!torch::isfinite(loss).all().item<bool>()) return foundation::Result<torch::Tensor>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "AMP cannot scale a NaN or infinite loss"));
        return foundation::Result<torch::Tensor>::Success(loss * m_scale);
    }
    catch (const c10::Error &error) { return foundation::Result<torch::Tensor>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch AMP loss scaling failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<torch::Tensor>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("AMP loss scaling failed: ") + error.what())); }
}

foundation::Result<bool> AmpController::UnscaleAndStep(torch::optim::Optimizer &optimizer, const std::vector<torch::Tensor> &parameters, const double maxGradientNorm)
{
    if (parameters.empty()) return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP optimizer step requires at least one parameter"));
    if (maxGradientNorm < 0.0 || !std::isfinite(maxGradientNorm)) return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP gradient clip norm must be finite and non-negative"));
    try
    {
        bool observedGradient = false;
        bool finite = true;
        double squaredNorm = 0.0;
        for (const torch::Tensor &parameter : parameters)
        {
            if (!parameter.defined()) return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "AMP optimizer parameter is undefined"));
            torch::Tensor gradient = parameter.grad();
            if (!gradient.defined()) continue;
            observedGradient = true;
            if (m_mode == PrecisionMode::AmpFp16) gradient.div_(m_scale);
            if (!torch::isfinite(gradient).all().item<bool>()) finite = false;
            else if (maxGradientNorm > 0.0) squaredNorm += gradient.detach().pow(2).sum().item<double>();
        }
        if (!observedGradient) return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "AMP optimizer step has no gradients"));
        if (!finite)
        {
            optimizer.zero_grad();
            RecordOverflow();
            return foundation::Result<bool>::Success(false);
        }
        if (maxGradientNorm > 0.0)
        {
            const double totalNorm = std::sqrt(squaredNorm);
            if (!std::isfinite(totalNorm)) return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "AMP gradient norm is NaN or infinite"));
            if (totalNorm > maxGradientNorm)
            {
                const double clipScale = maxGradientNorm / (totalNorm + 1.0e-12);
                for (const torch::Tensor &parameter : parameters)
                {
                    torch::Tensor gradient = parameter.grad();
                    if (gradient.defined()) gradient.mul_(clipScale);
                }
            }
        }
        optimizer.step();
        RecordFiniteStep();
        return foundation::Result<bool>::Success(true);
    }
    catch (const c10::Error &error) { return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::DependencyMissing, std::string("LibTorch AMP optimizer step failed: ") + error.what())); }
    catch (const std::exception &error) { return foundation::Result<bool>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, std::string("AMP optimizer step failed: ") + error.what())); }
}

void AmpController::RecordFiniteStep() noexcept
{
    if (m_mode != PrecisionMode::AmpFp16) return;
    ++m_consecutiveFiniteSteps;
    if (m_consecutiveFiniteSteps >= m_settings.growthInterval)
    {
        m_scale *= m_settings.growthFactor;
        m_consecutiveFiniteSteps = 0;
    }
}

void AmpController::RecordOverflow() noexcept
{
    if (m_mode != PrecisionMode::AmpFp16) return;
    m_scale *= m_settings.backoffFactor;
    m_consecutiveFiniteSteps = 0;
}
}
