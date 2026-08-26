#include "visionaiflow/training/AmpController.h"

#include <ATen/cuda/CUDAContext.h>
#include <cuda_runtime_api.h>

#include <QtTest>

#include <cmath>
#include <limits>

class AmpControllerCudaTest final : public QObject
{
    Q_OBJECT

private slots:
    void AmpFp16BacksOffGrowsAndRestoresScalerOnCuda();
};

void AmpControllerCudaTest::AmpFp16BacksOffGrowsAndRestoresScalerOnCuda()
{
    try
    {
        int deviceCount = 0;
        const cudaError_t deviceStatus = cudaGetDeviceCount(&deviceCount);
        QVERIFY2(deviceStatus == cudaSuccess, cudaGetErrorString(deviceStatus));
        QVERIFY2(deviceCount > 0, "CUDA is required for AMP FP16 validation");
        QVERIFY(at::cuda::getDeviceProperties(0) != nullptr);
        const torch::Device device(torch::kCUDA, 0);
        visionaiflow::training::AmpSettings settings;
        settings.initialScale = 8.0;
        settings.growthFactor = 2.0;
        settings.backoffFactor = 0.5;
        settings.growthInterval = 2;
        const auto created = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::AmpFp16, device, settings);
        QVERIFY2(created.IsSuccess(), created.IsSuccess() ? "" : created.Failure().message.c_str());
        auto amp = created.Value();

    torch::Tensor parameter = torch::ones({1}, torch::TensorOptions().device(device).dtype(torch::kFloat32).requires_grad(true));
    std::vector<torch::Tensor> parameters{parameter};
    torch::optim::SGD optimizer(parameters, torch::optim::SGDOptions(1.0));

    parameter.mutable_grad() = torch::full_like(parameter, 8.0F);
    const auto firstStep = amp.UnscaleAndStep(optimizer, parameters);
    QVERIFY2(firstStep.IsSuccess(), firstStep.IsSuccess() ? "" : firstStep.Failure().message.c_str());
    QVERIFY(firstStep.Value());
    QCOMPARE(amp.State().scale, 8.0);
    QCOMPARE(amp.State().consecutiveFiniteSteps, int64_t{1});
    QVERIFY(std::abs(parameter.detach().cpu().item<double>()) < 1.0e-5);

    parameter.mutable_grad() = torch::full_like(parameter, 8.0F);
    const auto secondStep = amp.UnscaleAndStep(optimizer, parameters);
    QVERIFY2(secondStep.IsSuccess(), secondStep.IsSuccess() ? "" : secondStep.Failure().message.c_str());
    QVERIFY(secondStep.Value());
    QCOMPARE(amp.State().scale, 16.0);
    QCOMPARE(amp.State().consecutiveFiniteSteps, int64_t{0});
    QVERIFY(std::abs(parameter.detach().cpu().item<double>() + 1.0) < 1.0e-5);

    const torch::Tensor beforeOverflow = parameter.detach().clone();
    parameter.mutable_grad() = torch::full_like(parameter, std::numeric_limits<float>::infinity());
    const auto overflowStep = amp.UnscaleAndStep(optimizer, parameters);
    QVERIFY2(overflowStep.IsSuccess(), overflowStep.IsSuccess() ? "" : overflowStep.Failure().message.c_str());
    QVERIFY(!overflowStep.Value());
    QCOMPARE(amp.State().scale, 8.0);
    QCOMPARE(amp.State().consecutiveFiniteSteps, int64_t{0});
    QVERIFY(torch::allclose(beforeOverflow, parameter.detach(), 0.0, 0.0));

    const auto restoredController = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::AmpFp16, device, settings);
    QVERIFY(restoredController.IsSuccess());
    auto restoredAmp = restoredController.Value();
    visionaiflow::training::AmpState restoredState;
    restoredState.mode = visionaiflow::training::PrecisionMode::AmpFp16;
    restoredState.scale = 4.0;
    restoredState.consecutiveFiniteSteps = 1;
    const auto restored = restoredAmp.RestoreState(restoredState);
    QVERIFY2(restored.IsSuccess(), restored.IsSuccess() ? "" : restored.Failure().message.c_str());
    QCOMPARE(restoredAmp.State().scale, 4.0);
    QCOMPARE(restoredAmp.State().consecutiveFiniteSteps, int64_t{1});

    parameter.mutable_grad() = torch::full_like(parameter, 4.0F);
    const auto restoredStep = restoredAmp.UnscaleAndStep(optimizer, parameters);
    QVERIFY2(restoredStep.IsSuccess(), restoredStep.IsSuccess() ? "" : restoredStep.Failure().message.c_str());
    QVERIFY(restoredStep.Value());
        QCOMPARE(restoredAmp.State().scale, 8.0);
        QCOMPARE(restoredAmp.State().consecutiveFiniteSteps, int64_t{0});
    }
    catch (const c10::Error &error)
    {
        QFAIL(error.what());
    }
    catch (const std::exception &error)
    {
        QFAIL(error.what());
    }
}

QTEST_APPLESS_MAIN(AmpControllerCudaTest)

#include "tst_AmpControllerCuda.moc"
