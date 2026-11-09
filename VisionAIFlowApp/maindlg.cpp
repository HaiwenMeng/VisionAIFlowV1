#include "maindlg.h"

#include "datasetform.h"
#include "AnnoForms/BaseAnnoForm/CommonSetForm.h"
#include "app/MainWindow.h"
#include "projectform.h"
#include "release/ui_maindlg.h"
#include "systemform.h"
#include "taskrepository.h"
#include "TrainForms/DetecForms/traindataform.h"
#include "TrainForms/YoloEForms/yoloetrainform.h"
#include "AnnoForms/YoloEAnnoForm/YoloeLabelForm.h"
#include "AnnoForms/ClassForms/classificationlabelform.h"
#include "InferForms/SegForms/YoloeValForm.h"
#include "InferForms/DetecForms/yolovalform.h"

#include <QDebug>
#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QWindow>

MainDlg::MainDlg(QWidget *parent)
    : QWidget(parent), ui(new Ui::MainDlg), m_projectForm(new ProJectForm(this)), m_dataSetForm(new DataSetForm(this)),
      m_trainDataForm(new TrainDataForm(this)), m_commonSetForm(new CommonSetForm(this)),
      m_yoloeTrainForm(new YoloeTrainForm(this)), m_yoloeValForm(new YoloeValForm(this)),
      m_yoloValForm(new YoloValForm(this)), m_systemForm(new SystemForm(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    ui->setupUi(this);
    ui->titleBar->installEventFilter(this);
    connect(ui->TB_Minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(ui->TB_Maximize,
            &QToolButton::clicked,
            this,
            [this]()
            {
                if (isMaximized())
                {
                    showNormal();
                }
                else
                {
                    showMaximized();
                }
                UpdateWindowControlButtons();
            });
    connect(ui->TB_Close, &QToolButton::clicked, this, &QWidget::close);
    connect(this, &QWidget::windowTitleChanged, ui->LB_WindowTitle, &QLabel::setText);
    ui->LB_WindowTitle->setText(windowTitle());
    UpdateWindowControlButtons();
    const QList<QWidget *> pages{m_projectForm,
                                 m_dataSetForm,
                                 m_trainDataForm,
                                 m_yoloeTrainForm,
                                 m_yoloeValForm,
                                 m_yoloValForm,
                                 m_commonSetForm,
                                 m_systemForm};
    for (QWidget *page : pages)
    {
        page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->stackedWidget->addWidget(page);
    }
    ui->stackedWidget->setCurrentWidget(m_projectForm);

    connect(m_projectForm, &ProJectForm::TaskSelected, this, &MainDlg::OnTaskSelected);
    SetCurrentNavButton(ui->PB_ProJectMange);
    m_projectForm->Refresh();
}

MainDlg::~MainDlg()
{
    delete ui;
}

bool MainDlg::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != ui->titleBar)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QWindow *window = windowHandle();
            if (window == nullptr || !window->startSystemMove())
            {
                qCritical().noquote() << QString(u8"无法启动主窗口拖拽移动");
                return false;
            }
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            if (isMaximized())
            {
                showNormal();
            }
            else
            {
                showMaximized();
            }
            UpdateWindowControlButtons();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MainDlg::UpdateWindowControlButtons()
{
    ui->TB_Maximize->setText(isMaximized() ? QStringLiteral("▣") : QStringLiteral("□"));
}

void MainDlg::OnTaskSelected(const QString &taskName)
{
    if (taskName.isEmpty())
    {
        m_taskName.clear();
        setWindowTitle(QString(u8"VisionAIFlow 标注训练软件"));
        return;
    }

    TaskDefinition task;
    QString errorMessage;
    if (!TaskRepository::LoadTask(taskName, &task, &errorMessage))
    {
        ReportError(errorMessage);
        return;
    }

    m_taskName = taskName;
    setWindowTitle(
        QString(u8"VisionAIFlow 标注训练软件 - [%1]-%2").arg(TaskRepository::DisplayType(task.type), taskName));
}

void MainDlg::on_PB_ProJectMange_clicked()
{
    if (!m_trainDataForm->isrunstate() || !m_yoloeTrainForm->isIdle() || !m_yoloValForm->isIdle())
    {
        ReportError(QString(u8"正在训练或推理, 无法切换项目"));
        return;
    }

    m_projectForm->Refresh();
    ui->stackedWidget->setCurrentWidget(m_projectForm);
    SetCurrentNavButton(ui->PB_ProJectMange);
}

void MainDlg::on_PB_LabSet_clicked()
{
    if (!RequireSupportedTask())
    {
        return;
    }

    m_commonSetForm->setTaskName(m_taskName);
    ui->stackedWidget->setCurrentWidget(m_commonSetForm);
    SetCurrentNavButton(ui->PB_LabSet);
}

void MainDlg::on_PB_SemiLabSet_clicked()
{
    if (!RequireSupportedTask())
    {
        return;
    }

    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(m_taskName, &task, &error))
    {
        ReportError(error);
        return;
    }
    using visionaiflow::domain::ProjectType;
    if (task.type == ProjectType::Classification)
    {
        if (m_classificationLabelForm == nullptr)
        {
            m_classificationLabelForm = new ClassificationLabelForm(this);
            m_classificationLabelForm->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            ui->stackedWidget->addWidget(m_classificationLabelForm);
        }
        m_classificationLabelForm->initialize(m_taskName);
        ui->stackedWidget->setCurrentWidget(m_classificationLabelForm);
        SetCurrentNavButton(ui->PB_SemiLabSet);
        return;
    }
    if (task.type == ProjectType::YoloE || task.type == ProjectType::InstanceSegmentation)
    {
        if (m_yoloeLabelForm == nullptr)
        {
            m_yoloeLabelForm = new YoloeLabelForm(this);
            m_yoloeLabelForm->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            ui->stackedWidget->addWidget(m_yoloeLabelForm);
        }
        m_yoloeLabelForm->setIgnoreEnabled(task.type == ProjectType::YoloE);
        m_yoloeLabelForm->setTaskName(m_taskName);
        ui->stackedWidget->setCurrentWidget(m_yoloeLabelForm);
        SetCurrentNavButton(ui->PB_SemiLabSet);
        m_yoloeLabelForm->initialize();
        return;
    }

    if (m_semiAutoAnnoForm == nullptr)
    {
        m_semiAutoAnnoForm = new MainWindow(this);
        m_semiAutoAnnoForm->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        ui->stackedWidget->addWidget(m_semiAutoAnnoForm);
    }

    m_semiAutoAnnoForm->setTaskName(m_taskName);
    ui->stackedWidget->setCurrentWidget(m_semiAutoAnnoForm);
    SetCurrentNavButton(ui->PB_SemiLabSet);
    m_semiAutoAnnoForm->ensureSamInitialized();
}

void MainDlg::on_PB_DataSet_clicked()
{
    if (!m_trainDataForm->isrunstate() || !m_yoloeTrainForm->isIdle())
    {
        ReportError(QString(u8"训练进行中, 无法修改数据集"));
        return;
    }
    if (!RequireSupportedTask())
    {
        return;
    }

    m_dataSetForm->toSetProcessName(m_taskName);
    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(m_taskName, &task, &error))
    {
        ReportError(error);
        return;
    }
    m_dataSetForm->setProjectType(task.type);
    m_dataSetForm->toInitShow();
    ui->stackedWidget->setCurrentWidget(m_dataSetForm);
    SetCurrentNavButton(ui->PB_DataSet);
}

void MainDlg::on_PB_TrainPro_clicked()
{
    if (!RequireSupportedTask() || !RequireGeneratedDataset())
    {
        return;
    }

    if (m_semiAutoAnnoForm != nullptr)
    {
        m_semiAutoAnnoForm->releaseSam3();
    }

    if (m_yoloeLabelForm != nullptr)
    {
        m_yoloeLabelForm->releaseSam2();
    }

    if (CurrentTaskIsYoloE())
    {
        m_yoloeTrainForm->setTaskName(m_taskName);
        m_yoloeTrainForm->initialize();
        ui->stackedWidget->setCurrentWidget(m_yoloeTrainForm);
        SetCurrentNavButton(ui->PB_TrainPro);
        return;
    }

    m_trainDataForm->toSetProcessName(m_taskName);
    m_trainDataForm->toInitShow();
    ui->stackedWidget->setCurrentWidget(m_trainDataForm);
    SetCurrentNavButton(ui->PB_TrainPro);
}

void MainDlg::on_PB_ValPro_clicked()
{
    if (!RequireSupportedTask() || !RequireGeneratedDataset())
    {
        return;
    }

    if (CurrentTaskIsYoloE())
    {
        m_yoloeValForm->setTaskName(m_taskName);
        m_yoloeValForm->initialize();
        ui->stackedWidget->setCurrentWidget(m_yoloeValForm);
        SetCurrentNavButton(ui->PB_ValPro);
        return;
    }

    m_yoloValForm->initialize(m_taskName);
    ui->stackedWidget->setCurrentWidget(m_yoloValForm);
    SetCurrentNavButton(ui->PB_ValPro);
}

void MainDlg::on_PB_SystemSet_clicked()
{
    m_systemForm->Refresh();
    ui->stackedWidget->setCurrentWidget(m_systemForm);
    SetCurrentNavButton(ui->PB_SystemSet);
}

bool MainDlg::RequireSupportedTask()
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
    if (!TaskRepository::IsSupportedTrainingTask(task))
    {
        ReportError(QString(u8"当前任务类型暂未打通数据、训练和验证页面"));
        return false;
    }
    return true;
}

bool MainDlg::CurrentTaskIsYoloE() const
{
    TaskDefinition task;
    QString errorMessage;
    return !m_taskName.isEmpty() && TaskRepository::LoadTask(m_taskName, &task, &errorMessage) &&
           TaskRepository::IsYoloE(task);
}

bool MainDlg::RequireGeneratedDataset()
{
    m_dataSetForm->toSetProcessName(m_taskName);
    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(m_taskName, &task, &error))
    {
        ReportError(error);
        return false;
    }
    m_dataSetForm->setProjectType(task.type);
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
