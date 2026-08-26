#include "nameselectdlg.h"
#include "ui_nameselectdlg.h"
NameSelectDlg::NameSelectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NameSelectDlg)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
}

NameSelectDlg::~NameSelectDlg()
{
    delete ui;
}

void NameSelectDlg::toInitLabeLis(QVector<QString> SetLabes)
{
    for(int i=0;i<SetLabes.size();i++)
    {
        QRadioButton *tButer=new QRadioButton(SetLabes.at(i));
        ui->gridLayout->addWidget(tButer);
        tButer->setText(SetLabes.at(i));
        m_SetRadis.append(tButer);
        tButer->setChecked(true);
    }


}

QString NameSelectDlg::toGetSetName()
{
    for (int i = 0; i < m_SetRadis.size(); i++)
    {
        if(m_SetRadis.at(i)->isChecked())
        {
            return m_SetRadis.at(i)->text();
        }
    }
    return "ZZ";
}

void NameSelectDlg::on_PB_Confirm_clicked()
{

    accept();
}

void NameSelectDlg::on_PB_Reject_clicked()
{
    reject();
}
