#include "systemform.h"
#include "ui_systemform.h"
#include "ytyolodefine.h"

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

SystemForm::SystemForm(QWidget *parent) : QWidget(parent), ui(new Ui::SystemForm)
{
    ui->setupUi(this);
    Refresh();
}

SystemForm::~SystemForm()
{
    delete ui;
}

void SystemForm::Refresh()
{
    ui->LE_WorkPath->setText(YtYoloDefine::toGetWorkPath());
    ui->LE_PythonPath->setText(YtYoloDefine::toGetPythonPath());
}

void SystemForm::on_PB_PythonPath_clicked()
{
    QString initialDirectory = YtYoloDefine::toGetPythonPath();
    if (initialDirectory.isEmpty())
    {
        initialDirectory = QDir::homePath();
    }
    const QString pythonPath = QFileDialog::getExistingDirectory(this,
                                                                 QString(u8"选择Python环境目录"),
                                                                 initialDirectory,
                                                                 QFileDialog::ShowDirsOnly);
    if (pythonPath.isEmpty())
    {
        return;
    }

    const QDir environmentDirectory(pythonPath);
    const QStringList requiredFiles{
        environmentDirectory.filePath(QStringLiteral("python.exe")),
        environmentDirectory.filePath(QStringLiteral("YOLOE/train_project_yoloe_seg.py")),
        environmentDirectory.filePath(QStringLiteral("YOLOE/infer_project_yoloe_seg.py")),
    };
    for (const QString &requiredFile : requiredFiles)
    {
        if (!QFileInfo::exists(requiredFile))
        {
            const QString errorMessage = QString(u8"所选目录缺少YOLOE运行文件: %1").arg(requiredFile);
            qWarning().noquote() << errorMessage;
            QMessageBox::critical(this, QString(u8"Python环境配置失败"), errorMessage);
            return;
        }
    }

    if (!YtYoloDefine::toSetPythonPath(QDir::cleanPath(pythonPath)))
    {
        const QString errorMessage = QString(u8"无法保存Python环境目录: %1").arg(pythonPath);
        QMessageBox::critical(this, QString(u8"Python环境配置失败"), errorMessage);
        return;
    }
    Refresh();
}

void SystemForm::on_PB_ViewWorkPath_clicked()
{
    const QString workPath = QFileDialog::getExistingDirectory(this,
                                                               QString(u8"选择工作目录"),
                                                               YtYoloDefine::toGetWorkPath(),
                                                               QFileDialog::ShowDirsOnly);
    if (!workPath.isEmpty())
    {
        YtYoloDefine::toSetWorkPath(workPath);
        Refresh();
    }
}
