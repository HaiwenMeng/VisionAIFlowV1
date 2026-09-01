#include "systemform.h"
#include "ui_systemform.h"
#include "ytyolodefine.h"

#include <QFileDialog>

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
