#include "setlabelname.h"
#include "ui_setlabelname.h"
#include <QColorDialog>
SetLabelName::SetLabelName(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SetLabelName)
{
    ui->setupUi(this);
    m_SetName=u8"±êÇ©1";
    m_SetColor=Qt::red;
    toSetData(m_SetName,m_SetColor);
}

SetLabelName::~SetLabelName()
{
    delete ui;
}

void SetLabelName::on_PB_Confirm_clicked()
{
    m_SetName=ui->lineEdit->text();
    accept();
}

void SetLabelName::on_PB_SetColor_clicked()
{

    QColorDialog tDlg(m_SetColor);
    if(QDialog::Accepted==tDlg.exec())
    {
        m_SetColor=tDlg.selectedColor();
        ui->PB_SetColor->setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(m_SetColor.red())
                                                                                    .arg(m_SetColor.green())
                                                                                    .arg(m_SetColor.blue()));

    }
}

void SetLabelName::toSetData(QString NameS, QColor SetColor)
{
    m_SetName=NameS;
    m_SetColor=SetColor;
    ui->PB_SetColor->setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(m_SetColor.red())
                                                                                .arg(m_SetColor.green())
                                                                                .arg(m_SetColor.blue()));
    ui->lineEdit->setText(m_SetName);

}

void SetLabelName::toGetData(QString &NameS, QColor &SetColor)
{
    NameS=m_SetName;
    SetColor=m_SetColor;
}

void SetLabelName::on_PB_Cancle_clicked()
{
    reject();
}
