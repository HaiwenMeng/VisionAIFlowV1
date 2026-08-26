#ifndef RENAMEDLG_H
#define RENAMEDLG_H

#include <QDialog>

namespace Ui {
class ReNameDlg;
}

class ReNameDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ReNameDlg(QWidget *parent = nullptr);
    ~ReNameDlg();

public:
    void toInitData(QString CunrentName, QVector<QString> NameLis, QVector<QColor> Colorset);
    QString toGetCurentName();
    int toGetIndex();
private slots:
    void on_PB_Confirm_clicked();

    void on_PB_Cancle_clicked();

private:
    Ui::ReNameDlg *ui;
};

#endif // RENAMEDLG_H
