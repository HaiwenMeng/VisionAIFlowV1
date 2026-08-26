#include "renamedlg.h"
#include "ui_renamedlg.h"

ReNameDlg::ReNameDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReNameDlg)
{
    ui->setupUi(this);
}

ReNameDlg::~ReNameDlg()
{
    delete ui;
}

void ReNameDlg::toInitData(QString CunrentName, QVector<QString> NameLis,QVector<QColor> Colorset)
{
    ui->LE_Name->setText(CunrentName);
    ui->LE_Name->setReadOnly(true);
    ui->CB_NewName->clear();
    for(int i=0;i<NameLis.size();i++)
    {
        QImage temim=QImage(40,40,QImage::Format_RGB888);
        temim.fill(Colorset.at(i));
        ui->CB_NewName->addItem(QIcon(QPixmap::fromImage(temim)),NameLis.at(i));
    }
    ui->CB_NewName->setCurrentIndex(0);
}

QString ReNameDlg::toGetCurentName()
{
    return ui->CB_NewName->currentText();
}

int ReNameDlg::toGetIndex()
{
    return ui->CB_NewName->currentIndex();
}

void ReNameDlg::on_PB_Confirm_clicked()
{
    accept();
}

void ReNameDlg::on_PB_Cancle_clicked()
{
    reject();
}
