#include "visionaiflow/trainer_host/CudaRuntimeProbe.h"
#include "visionaiflow/trainer_host/TrainerJobCoordinator.h"
#include "visionaiflow/qt_foundation/HostRuntime.h"

#include <QCoreApplication>

#include <exception>

namespace
{
visionaiflow::foundation::Result<QCborMap> ExecuteTrainerOperation(const QCborMap &request)
{
    const QString operation = request.value(QStringLiteral("operation")).toString();
    if (operation != QStringLiteral("cudaInfo")) return visionaiflow::foundation::Result<QCborMap>::Failure(visionaiflow::foundation::Error::Create(visionaiflow::foundation::ErrorCode::UnsupportedOperation, "Trainer operation is unsupported"));
    const auto device = visionaiflow::trainer_host::QueryCudaDeviceInfo(0);
    if (!device.IsSuccess()) return visionaiflow::foundation::Result<QCborMap>::Failure(device.Failure());
    QCborMap response;
    response.insert(QStringLiteral("operation"), operation);
    response.insert(QStringLiteral("deviceName"), QString::fromStdString(device.Value().name));
    response.insert(QStringLiteral("deviceUuid"), QString::fromStdString(device.Value().uuid));
    response.insert(QStringLiteral("runtimeVersion"), device.Value().runtimeVersion);
    response.insert(QStringLiteral("driverVersion"), device.Value().driverVersion);
    response.insert(QStringLiteral("computeMajor"), device.Value().computeCapabilityMajor);
    response.insert(QStringLiteral("computeMinor"), device.Value().computeCapabilityMinor);
    response.insert(QStringLiteral("freeMemoryBytes"), static_cast<qint64>(device.Value().freeMemoryBytes));
    response.insert(QStringLiteral("totalMemoryBytes"), static_cast<qint64>(device.Value().totalMemoryBytes));
    return visionaiflow::foundation::Result<QCborMap>::Success(std::move(response));
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionTrainerHost"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    visionaiflow::trainer_host::TrainerJobCoordinator coordinator(&application);
    const auto runHostRuntime = [&application, &coordinator]() {
        return visionaiflow::qt_foundation::RunHostApplication(application, QStringLiteral("trainer"), QStringLiteral("0.1.0"), ExecuteTrainerOperation, [&coordinator](const QCborMap &request, const visionaiflow::qt_foundation::HostAsyncResponder &responder) { return coordinator.Handle(request, responder); });
    };
    const QStringList arguments = application.arguments();
    if (arguments.contains(QStringLiteral("--help")) || arguments.contains(QStringLiteral("-h")) || arguments.contains(QStringLiteral("--version")) || arguments.contains(QStringLiteral("-v"))) return runHostRuntime();
    try
    {
        const auto cudaRuntime = visionaiflow::trainer_host::VerifyCudaRuntime();
        if (!cudaRuntime.IsSuccess())
        {
            qCritical("VisionTrainerHost CUDA startup failed: %s", cudaRuntime.Failure().message.c_str());
            return 1;
        }
        return runHostRuntime();
    }
    catch (const std::exception &exception)
    {
        qCritical("VisionTrainerHost startup failed: %s", exception.what());
        return 1;
    }
    catch (...)
    {
        qCritical("VisionTrainerHost startup failed with an unknown exception");
        return 1;
    }
}
