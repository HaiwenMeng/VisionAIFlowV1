#include "visionaiflow/app/MainWindow.h"

#include <QApplication>

#include <exception>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionAIFlow"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    try
    {
        visionaiflow::app::MainWindow window;
        window.show();
        return application.exec();
    }
    catch (const std::exception &exception)
    {
        qCritical("VisionAIFlow startup failed: %s", exception.what());
        return 1;
    }
    catch (...)
    {
        qCritical("VisionAIFlow startup failed with an unknown exception");
        return 1;
    }
}
