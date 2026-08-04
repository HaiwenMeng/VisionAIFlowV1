#include "visionaiflow/training/AmpController.h"
#include "visionaiflow/training/LinearClassifierTrainer.h"

#include <QtTest>

#include <cmath>
#include <limits>

class AmpControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void RejectsAmpFp16OnCpu();
    void RejectsInvalidGradientClipNorm();
    void SkipsNonFiniteGradientAndPreservesParameters();
    void ClipsGradientBeforeStep();
};

void AmpControllerTest::RejectsAmpFp16OnCpu()
{
    const auto controller = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::AmpFp16, torch::kCPU);
    QVERIFY(!controller.IsSuccess());
    QVERIFY(!controller.Failure().message.empty());
}

void AmpControllerTest::RejectsInvalidGradientClipNorm()
{
    torch::Tensor parameter = torch::ones({1}, torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true));
    std::vector<torch::Tensor> parameters{parameter};
    torch::optim::SGD optimizer(parameters, torch::optim::SGDOptions(1.0));
    const auto controller = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::Fp32, torch::kCPU);
    QVERIFY(controller.IsSuccess());
    auto amp = controller.Value();
    parameter.mutable_grad() = torch::ones_like(parameter);
    const auto rejected = amp.UnscaleAndStep(optimizer, parameters, -1.0);
    QVERIFY(!rejected.IsSuccess());
    QVERIFY(!rejected.Failure().message.empty());
}

void AmpControllerTest::SkipsNonFiniteGradientAndPreservesParameters()
{
    const auto created = visionaiflow::training::CreateLinearClassifier(2, 2);
    QVERIFY(created.IsSuccess());
    auto model = created.Value();
    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.1));
    const auto controller = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::Fp32, torch::kCPU);
    QVERIFY(controller.IsSuccess());
    auto amp = controller.Value();
    const auto parameters = model->parameters();
    const torch::Tensor before = parameters.front().detach().clone();
    parameters.front().mutable_grad() = torch::full_like(parameters.front(), std::numeric_limits<float>::infinity());
    const auto skipped = amp.UnscaleAndStep(optimizer, parameters);
    QVERIFY(skipped.IsSuccess());
    QVERIFY(!skipped.Value());
    QVERIFY(torch::allclose(before, parameters.front()));
    parameters.front().mutable_grad() = torch::ones_like(parameters.front());
    const auto stepped = amp.UnscaleAndStep(optimizer, parameters);
    QVERIFY(stepped.IsSuccess());
    QVERIFY(stepped.Value());
    QVERIFY(!torch::allclose(before, parameters.front()));
}

void AmpControllerTest::ClipsGradientBeforeStep()
{
    torch::Tensor parameter = torch::ones({1}, torch::TensorOptions().dtype(torch::kFloat32).requires_grad(true));
    std::vector<torch::Tensor> parameters{parameter};
    torch::optim::SGD optimizer(parameters, torch::optim::SGDOptions(1.0));
    const auto controller = visionaiflow::training::AmpController::Create(visionaiflow::training::PrecisionMode::Fp32, torch::kCPU);
    QVERIFY(controller.IsSuccess());
    auto amp = controller.Value();
    parameter.mutable_grad() = torch::full_like(parameter, 10.0F);
    const auto stepped = amp.UnscaleAndStep(optimizer, parameters, 0.25);
    QVERIFY(stepped.IsSuccess());
    QVERIFY(stepped.Value());
    const double actual = parameter.detach().item<double>();
    QVERIFY(std::abs(actual - 0.75) < 1.0e-5);
}

QTEST_APPLESS_MAIN(AmpControllerTest)

#include "tst_AmpController.moc"
