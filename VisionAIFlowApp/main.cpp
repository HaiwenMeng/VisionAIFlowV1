#include "maindlg.h"
#include "taskrepository.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMessageBox>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
void ConfigureModelSearchPath()
{
#ifdef Q_OS_WIN
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    const QString modelsPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Models"));
    if (AddDllDirectory(reinterpret_cast<LPCWSTR>(modelsPath.utf16())) == nullptr)
    {
        qCritical().noquote() << QString(u8"无法注册模型 DLL 目录: %1, 错误码: %2").arg(modelsPath).arg(GetLastError());
    }
#endif
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VisionAIFlowApp"));
    application.setOrganizationName(QStringLiteral("VisionAIFlow"));
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    ConfigureModelSearchPath();

    QString errorMessage;
    if (!TaskRepository::Initialize(&errorMessage))
    {
        qCritical().noquote() << errorMessage;
        QMessageBox::critical(nullptr, QString(u8"VisionAIFlow"), errorMessage);
        return 1;
    }

    MainDlg dialog;
    dialog.resize(1280, 820);
    dialog.show();
    return application.exec();
}
