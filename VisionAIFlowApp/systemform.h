#ifndef SYSTEMFORM_H
#define SYSTEMFORM_H

#include <QWidget>

namespace Ui {
class SystemForm;
}

class SystemForm : public QWidget
{
    Q_OBJECT

public:
    explicit SystemForm(QWidget *parent = nullptr);
    ~SystemForm();

private slots:
    void on_PB_ViewWorkPath_clicked();

    void on_PB_PythonPath_clicked();

private:
    Ui::SystemForm *ui;
};

#endif // SYSTEMFORM_H
