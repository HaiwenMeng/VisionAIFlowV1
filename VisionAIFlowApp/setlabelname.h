#ifndef SETLABELNAME_H
#define SETLABELNAME_H

#include <QDialog>

namespace Ui {
class SetLabelName;
}

class SetLabelName : public QDialog
{
    Q_OBJECT

public:
    explicit SetLabelName(QWidget *parent = nullptr);
    ~SetLabelName();

private slots:
    void on_PB_Confirm_clicked();

    void on_PB_SetColor_clicked();
    void on_PB_Cancle_clicked();

public:
    QColor m_SetColor;
    QString m_SetName;
public:
    void toSetData(QString NameS,QColor SetColor);
    void toGetData(QString &NameS,QColor &SetColor);
private:
    Ui::SetLabelName *ui;
};

#endif // SETLABELNAME_H
