#include "maindlg.h"
#include "taskrepository.h"
#include "ytyolodefine.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QMessageBox>
#include <QScreen>
#include <QStyle>

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

    QFile styleSheetFile(QStringLiteral(":/styles/application-dark.qss"));
    if (!styleSheetFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QString errorMessage = QString(u8"无法加载应用深色样式: %1").arg(styleSheetFile.errorString());
        qCritical().noquote() << errorMessage;
        QMessageBox::critical(nullptr, QString(u8"VisionAIFlow"), errorMessage);
        return 1;
    }
    application.setStyleSheet(QString::fromUtf8(styleSheetFile.readAll()));

    if (YtYoloDefine::toGetWorkPath().isEmpty())
    {
        const QString errorMessage = QString(u8"未选择有效工作目录，程序无法启动");
        qCritical().noquote() << errorMessage;
        QMessageBox::critical(nullptr, QString(u8"VisionAIFlow"), errorMessage);
        return 1;
    }

    QString errorMessage;
    if (!TaskRepository::Initialize(&errorMessage))
    {
        qCritical().noquote() << errorMessage;
        QMessageBox::critical(nullptr, QString(u8"VisionAIFlow"), errorMessage);
        return 1;
    }

    MainDlg dialog;
    const QRect availableGeometry = application.primaryScreen()->availableGeometry();
    const QSize initialSize(qMin(1280, availableGeometry.width() * 9 / 10),
                            qMin(760, availableGeometry.height() * 9 / 10));
    dialog.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, initialSize, availableGeometry));
    dialog.show();
    return application.exec();
}
