#pragma once

#include "visionaiflow/domain/ProjectType.h"

#include <QDialog>

namespace Ui
{
class SetNameDialog;
}

class SetNameDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SetNameDialog(QWidget *parent = nullptr);
    ~SetNameDialog() override;

    void SetRenameMode(const QString &name, const QString &description);
    QString TaskName() const;
    QString Description() const;
    visionaiflow::domain::ProjectType TaskType() const;

private slots:
    void on_PB_Confirm_clicked();
    void on_PB_Cancel_clicked();

private:
    Ui::SetNameDialog *ui;
    bool m_renameMode{false};
};
