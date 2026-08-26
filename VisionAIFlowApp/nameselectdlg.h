#ifndef NAMESELECTDLG_H
#define NAMESELECTDLG_H

#include <QDialog>
#include <QRadioButton>

namespace Ui {
class NameSelectDlg;
}

class NameSelectDlg : public QDialog
{
    Q_OBJECT

public:
    explicit NameSelectDlg(QWidget *parent = nullptr);
    ~NameSelectDlg();

private:
    Ui::NameSelectDlg *ui;
public:
    QList<QRadioButton *> m_SetRadis;
public:
    void toInitLabeLis(QVector<QString> SetLabes);
    QString toGetSetName();
private slots:
    void on_PB_Confirm_clicked();
    void on_PB_Reject_clicked();
};

#endif // NAMESELECTDLG_H
