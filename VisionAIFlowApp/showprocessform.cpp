#include "showprocessform.h"
#include "release/ui_showprocessform.h"

ShowProcessForm::ShowProcessForm(QWidget *parent) : QDialog(parent), ui(new Ui::ShowProcessForm)
{
    ui->setupUi(this);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint);
    setMouseTracking(this);
    connect(this, &ShowProcessForm::SIgShowProcess, this, &ShowProcessForm::toSlotShowProcess);
}

ShowProcessForm::~ShowProcessForm()
{
    delete ui;
}

void ShowProcessForm::toSlotShowProcess(int percent)
{
    ui->progressBar->setValue(percent);
}

void ShowProcessForm::toSetProcess(int percent)
{
    emit SIgShowProcess(percent);
}
