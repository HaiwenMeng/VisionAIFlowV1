#include "visionaiflow/app/WorkspaceWindow.h"

#include <QApplication>
#include <QDir>

#include <exception>

#include <windows.h>

namespace
{
void ConfigureModelDllDirectory()
{
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    const std::wstring modelDirectory =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("model")).toStdWString();
    AddDllDirectory(modelDirectory.c_str());
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ConfigureModelDllDirectory();
    application.setApplicationName(QStringLiteral("VisionAIFlow"));
    application.setApplicationVersion(QStringLiteral("0.1.0"));
    try
    {
        visionaiflow::app::WorkspaceWindow window;
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
