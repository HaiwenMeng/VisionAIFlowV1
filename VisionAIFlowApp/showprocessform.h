#ifndef SHOWPROCESSFORM_H
#define SHOWPROCESSFORM_H

#include <QWidget>
#include <QDialog>
namespace Ui {
class ShowProcessForm;
}

class ShowProcessForm : public QDialog
{
    Q_OBJECT

public:
    explicit ShowProcessForm(QWidget *parent = nullptr);
    ~ShowProcessForm();

private:
    Ui::ShowProcessForm *ui;
signals:
    void SIgShowProcess(int percent);
public slots:
    void toSlotShowProcess(int percent);
public:
    void toSetProcess(int percent);
};

#endif // SHOWPROCESSFORM_H
