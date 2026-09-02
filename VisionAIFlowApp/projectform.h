#pragma once

#include "taskrepository.h"

#include <QWidget>

namespace Ui
{
class ProJectForm;
}

class ProJectForm final : public QWidget
{
    Q_OBJECT

public:
    explicit ProJectForm(QWidget *parent = nullptr);
    ~ProJectForm() override;

    void Refresh();
    QString CurrentTaskName() const;
    TaskDefinition CurrentTask() const;

signals:
    void TaskSelected(const QString &taskName);

private slots:
    void on_PB_Add_clicked();
    void on_PB_ReName_clicked();
    void on_PB_DleteProject_clicked();
    void on_PB_AddLabel_clicked();
    void on_PB_RemoveLabel_clicked();
    void on_PB_ViewPos_clicked();
    void on_TW_ListTask_itemSelectionChanged();

private:
    void ShowTask(const TaskDefinition &task);
    void ReportError(const QString &errorMessage);

    Ui::ProJectForm *ui;
    QVector<TaskDefinition> m_tasks;
    int m_currentRow{-1};
};
