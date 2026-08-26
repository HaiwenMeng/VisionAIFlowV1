#include "maindlg.h"

#include "datasetform.h"
#include "AnnoForms/BaseAnnoForm/CommonSetForm.h"
#include "app/MainWindow.h"
#include "projectform.h"
#include "release/ui_maindlg.h"
#include "taskrepository.h"
#include "TrainForms/DetecForms/traindataform.h"
#include "valsetform.h"

#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>

MainDlg::MainDlg(QWidget *parent)
    : QWidget(parent), ui(new Ui::MainDlg), m_projectForm(new ProJectForm(this)), m_dataSetForm(new DataSetForm(this)),
      m_trainDataForm(new TrainDataForm(this)), m_valSetForm(new ValSetForm(this)), m_commonSetForm(new CommonSetForm(this)),
      m_semiAutoAnnoForm(new MainWindow(this))
{
    ui->setupUi(this);
    ui->stackedWidget->addWidget(m_projectForm);
    ui->stackedWidget->addWidget(m_dataSetForm);
    ui->stackedWidget->addWidget(m_trainDataForm);
    ui->stackedWidget->addWidget(m_valSetForm);
    ui->stackedWidget->addWidget(m_commonSetForm);
    ui->stackedWidget->addWidget(m_semiAutoAnnoForm);
    ui->stackedWidget->setCurrentWidget(m_projectForm);

    connect(m_projectForm, &ProJectForm::TaskSelected, this, &MainDlg::OnTaskSelected);
    SetCurrentNavButton(ui->PB_ProJectMange);
    m_projectForm->Refresh();
}

MainDlg::~MainDlg()
{
    delete ui;
}

void MainDlg::OnTaskSelected(const QString &taskName)
{
    m_taskName = taskName;
    setWindowTitle(QString(u8"VisionAIFlow 标注训练软件 - %1").arg(taskName));
}

void MainDlg::on_PB_ProJectMange_clicked()
{
    if (!m_trainDataForm->isrunstate())
    {
        ReportError(QString(u8"正在训练，无法切换项目"));
        return;
    }

    m_projectForm->Refresh();
    ui->stackedWidget->setCurrentWidget(m_projectForm);
    SetCurrentNavButton(ui->PB_ProJectMange);
}

void MainDlg::on_PB_LabSet_clicked()
{
    if (!RequireDetectionTask())
    {
        return;
    }

    m_commonSetForm->setTaskName(m_taskName);
    ui->stackedWidget->setCurrentWidget(m_commonSetForm);
    SetCurrentNavButton(ui->PB_LabSet);
}

void MainDlg::on_PB_SemiLabSet_clicked()
{
    if (!RequireDetectionTask())
    {
        return;
    }

    ui->stackedWidget->setCurrentWidget(m_semiAutoAnnoForm);
    SetCurrentNavButton(ui->PB_SemiLabSet);
}

void MainDlg::on_PB_DataSet_clicked()
{
    if (!RequireDetectionTask())
    {
        return;
    }

    m_dataSetForm->toSetProcessName(m_taskName);
    m_dataSetForm->toInitShow();
    ui->stackedWidget->setCurrentWidget(m_dataSetForm);
    SetCurrentNavButton(ui->PB_DataSet);
}

void MainDlg::on_PB_TrainPro_clicked()
{
    if (!RequireDetectionTask() || !RequireGeneratedDataset())
    {
        return;
    }

    m_trainDataForm->toSetProcessName(m_taskName);
    m_trainDataForm->m_DataSetForm = m_dataSetForm;
    m_trainDataForm->toInitShow();
    ui->stackedWidget->setCurrentWidget(m_trainDataForm);
    SetCurrentNavButton(ui->PB_TrainPro);
}

void MainDlg::on_PB_ValPro_clicked()
{
    if (!RequireDetectionTask() || !RequireGeneratedDataset())
    {
        return;
    }

    m_valSetForm->m_DataSetForm = m_dataSetForm;
    m_valSetForm->toSetProcessName(m_taskName);
    m_valSetForm->toInitShow();
    ui->stackedWidget->setCurrentWidget(m_valSetForm);
    SetCurrentNavButton(ui->PB_ValPro);
}

void MainDlg::on_PB_SystemSet_clicked()
{
    ShowUnavailable(QString(u8"系统设置"));
}

bool MainDlg::RequireDetectionTask()
{
    if (m_taskName.isEmpty())
    {
        ReportError(QString(u8"请先在项目管理页面选择任务"));
        return false;
    }

    TaskDefinition task;
    QString errorMessage;
    if (!TaskRepository::LoadTask(m_taskName, &task, &errorMessage))
    {
        ReportError(errorMessage);
        return false;
    }
    if (!TaskRepository::IsDetection(task))
    {
        ReportError(QString(u8"当前任务类型暂未打通数据、训练和验证页面"));
        return false;
    }
    return true;
}

bool MainDlg::RequireGeneratedDataset()
{
    m_dataSetForm->toSetProcessName(m_taskName);
    if (m_dataSetForm->toGetRunIndex() < 0)
    {
        ReportError(QString(u8"数据集尚未生成"));
        return false;
    }
    return true;
}

void MainDlg::SetCurrentNavButton(QPushButton *button)
{
    const QList<QPushButton *> buttons = {ui->PB_ProJectMange,
                                          ui->PB_LabSet,
                                          ui->PB_SemiLabSet,
                                          ui->PB_DataSet,
                                          ui->PB_TrainPro,
                                          ui->PB_ValPro,
                                          ui->PB_SystemSet};
    for (QPushButton *navButton : buttons)
    {
        navButton->setCheckable(true);
        navButton->setChecked(navButton == button);
        navButton->style()->unpolish(navButton);
        navButton->style()->polish(navButton);
        navButton->update();
    }
}

void MainDlg::ShowUnavailable(const QString &featureName)
{
    ReportError(QString(u8"%1 页面尚未接入当前版本").arg(featureName));
}

void MainDlg::ReportError(const QString &errorMessage)
{
    qCritical().noquote() << errorMessage;
    QMessageBox::critical(this, QString(u8"VisionAIFlow"), errorMessage);
}
