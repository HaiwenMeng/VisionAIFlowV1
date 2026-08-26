#include "systemform.h"
#include "ui_systemform.h"
#include "ytyolodefine.h"
#include <QFileDialog>
SystemForm::SystemForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SystemForm)
{
    ui->setupUi(this);
    ui->LE_WorkPath->setText(YtYoloDefine::toGetWorkPath());
    ui->LE_PythonPath->setText(YtYoloDefine::toGetPythonPath());

}

SystemForm::~SystemForm()
{
    delete ui;
}

void SystemForm::on_PB_ViewWorkPath_clicked()
{
    QString temstr=QFileDialog::getExistingDirectory(this,u8"请选择工作目录",YtYoloDefine::toGetWorkPath(),QFileDialog::ShowDirsOnly);
    if(!temstr.isEmpty())
    {
        YtYoloDefine::toSetWorkPath(temstr);
        ui->LE_WorkPath->setText(YtYoloDefine::toGetWorkPath());
    }
}

void SystemForm::on_PB_PythonPath_clicked()
{
    QString temstr=QFileDialog::getExistingDirectory(this,u8"请环境目录",YtYoloDefine::toGetPythonPath(),QFileDialog::ShowDirsOnly);
    if(!temstr.isEmpty())
    {
        YtYoloDefine::toSetPythonPath(temstr);
        ui->LE_PythonPath->setText(YtYoloDefine::toGetPythonPath());
        //
        QFile temfile;
        temfile.setFileName(QString("%1/AppData/Roaming/Ultralytics/%2").arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).arg("Arial.ttf"));
        qDebug()<<temfile.fileName()<<"zz";
        if(!temfile.exists())
        {
            QFile::copy(":/ResourYolo/Arial.ttf",temfile.fileName());
        }
        temfile.setFileName(QString("%1/AppData/Roaming/Ultralytics/%2").arg(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).arg("Arial.Unicode.ttf"));
        if(!temfile.exists())
        {
            QFile::copy(":/ResourYolo/Arial.Unicode.ttf",temfile.fileName());
        }

    }
}



