#include "projectform.h"
#include "setnamedialog.h"
#include "release/ui_projectform.h"

#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QUrl>

ProJectForm::ProJectForm(QWidget *parent) : QWidget(parent), ui(new Ui::ProJectForm)
{
    ui->setupUi(this);
    ui->TW_ListTask->verticalHeader()->setVisible(false);
    ui->TW_ListTask->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->TW_ListTask->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->TW_ListTask->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->TW_ListTask->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->TW_LabelSet->verticalHeader()->setVisible(false);
    ui->TW_LabelSet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->TW_LabelSet->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

ProJectForm::~ProJectForm()
{
    delete ui;
}

void ProJectForm::Refresh()
{
    const QString selectedTask = CurrentTaskName();
    QString errorMessage;
    if (!TaskRepository::ListTasks(&m_tasks, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }

    ui->TW_ListTask->setRowCount(m_tasks.size());
    m_currentRow = -1;
    for (qsizetype index = 0; index < m_tasks.size(); ++index)
    {
        const TaskDefinition &task = m_tasks.at(index);
        const QStringList values{QString::number(index + 1),
                                 task.name,
                                 QFileInfo(QDir(TaskRepository::LabelRoot()).filePath(task.name))
                                     .birthTime()
                                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                 TaskRepository::DisplayType(task.type),
                                 TaskRepository::DisplayProgress(task.progress)};
        for (qsizetype column = 0; column < values.size(); ++column)
        {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setTextAlignment(Qt::AlignCenter);
            ui->TW_ListTask->setItem(index, column, item);
        }
        if (task.name == selectedTask)
        {
            ui->TW_ListTask->selectRow(index);
        }
    }
    if (!m_tasks.isEmpty() && ui->TW_ListTask->currentRow() < 0)
    {
        ui->TW_ListTask->selectRow(0);
    }
}

QString ProJectForm::CurrentTaskName() const
{
    if (m_currentRow < 0 || m_currentRow >= m_tasks.size())
    {
        return QString();
    }
    return m_tasks.at(m_currentRow).name;
}

TaskDefinition ProJectForm::CurrentTask() const
{
    if (m_currentRow < 0 || m_currentRow >= m_tasks.size())
    {
        return {};
    }
    return m_tasks.at(m_currentRow);
}

void ProJectForm::on_PB_Add_clicked()
{
    SetNameDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    TaskDefinition task;
    task.name = dialog.TaskName();
    task.description = dialog.Description();
    task.type = dialog.TaskType();
    QString errorMessage;
    if (!TaskRepository::CreateTask(task, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }
    Refresh();
}

void ProJectForm::on_PB_ReName_clicked()
{
    const TaskDefinition currentTask = CurrentTask();
    if (currentTask.name.isEmpty())
    {
        ReportError(QString(u8"请先选择任务"));
        return;
    }

    SetNameDialog dialog(this);
    dialog.SetRenameMode(currentTask.name, currentTask.description);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString errorMessage;
    if (!TaskRepository::RenameTask(currentTask.name, dialog.TaskName(), &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }
    TaskDefinition updated = currentTask;
    updated.name = dialog.TaskName();
    updated.description = dialog.Description();
    if (!TaskRepository::SaveTask(updated, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }
    Refresh();
}

void ProJectForm::on_PB_AddLabel_clicked()
{
    const TaskDefinition currentTask = CurrentTask();
    if (!TaskRepository::IsDetection(currentTask))
    {
        ReportError(QString(u8"当前任务类型暂未实现标签配置"));
        return;
    }

    bool accepted = false;
    const QString label = QInputDialog::getText(this,
                                                QString(u8"新增标签"),
                                                QString(u8"标签名称"),
                                                QLineEdit::Normal,
                                                QString(),
                                                &accepted)
                              .trimmed();
    if (!accepted)
    {
        return;
    }
    if (label.isEmpty() || currentTask.labels.contains(label))
    {
        ReportError(QString(u8"标签不能为空且不能重复"));
        return;
    }

    TaskDefinition updated = currentTask;
    updated.labels.append(label);
    QString errorMessage;
    if (!TaskRepository::SaveTask(updated, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }
    Refresh();
}

void ProJectForm::on_PB_RemoveLabel_clicked()
{
    const int labelRow = ui->TW_LabelSet->currentRow();
    TaskDefinition currentTask = CurrentTask();
    if (labelRow < 0 || labelRow >= currentTask.labels.size())
    {
        ReportError(QString(u8"请先选择标签"));
        return;
    }
    currentTask.labels.removeAt(labelRow);
    if (labelRow < currentTask.colors.size())
    {
        currentTask.colors.removeAt(labelRow);
    }
    QString errorMessage;
    if (!TaskRepository::SaveTask(currentTask, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }
    Refresh();
}

void ProJectForm::on_PB_ViewPos_clicked()
{
    const QString taskName = CurrentTaskName();
    if (taskName.isEmpty())
    {
        ReportError(QString(u8"请先选择任务"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(TaskRepository::LabelRoot()).filePath(taskName)));
}

void ProJectForm::on_TW_ListTask_itemSelectionChanged()
{
    m_currentRow = ui->TW_ListTask->currentRow();
    if (m_currentRow < 0 || m_currentRow >= m_tasks.size())
    {
        return;
    }
    ShowTask(m_tasks.at(m_currentRow));
    emit TaskSelected(m_tasks.at(m_currentRow).name);
}

void ProJectForm::ShowTask(const TaskDefinition &task)
{
    ui->PTE_Description->setPlainText(task.description);
    ui->TW_LabelSet->setRowCount(task.labels.size());
    for (qsizetype index = 0; index < task.labels.size(); ++index)
    {
        auto *number = new QTableWidgetItem(QString::number(index + 1));
        auto *name = new QTableWidgetItem(task.labels.at(index));
        auto *color = new QTableWidgetItem();
        color->setBackground(index < task.colors.size() ? task.colors.at(index) : Qt::white);
        number->setTextAlignment(Qt::AlignCenter);
        name->setTextAlignment(Qt::AlignCenter);
        ui->TW_LabelSet->setItem(index, 0, number);
        ui->TW_LabelSet->setItem(index, 1, name);
        ui->TW_LabelSet->setItem(index, 2, color);
    }
}

void ProJectForm::ReportError(const QString &errorMessage)
{
    qCritical().noquote() << errorMessage;
    QMessageBox::critical(this, QString(u8"项目管理"), errorMessage);
}
