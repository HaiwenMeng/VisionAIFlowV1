#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class CreateProjectDialog; }
QT_END_NAMESPACE

namespace visionaiflow::app
{
class CreateProjectDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CreateProjectDialog(QWidget *parent = nullptr);
    ~CreateProjectDialog() override;

private slots:
    void ChooseDirectory();
    void UpdateClassificationMode();
    void CreateProject();

private:
    Ui::CreateProjectDialog *ui;
};
}
