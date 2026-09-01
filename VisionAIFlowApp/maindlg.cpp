#include "maindlg.h"

#include "datasetform.h"
#include "AnnoForms/BaseAnnoForm/CommonSetForm.h"
#include "app/MainWindow.h"
#include "projectform.h"
#include "release/ui_maindlg.h"
#include "systemform.h"
#include "taskrepository.h"
#include "TrainForms/DetecForms/traindataform.h"
#include "valsetform.h"

#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>

MainDlg::MainDlg(QWidget *parent)
    : QWidget(parent), ui(new Ui::MainDlg), m_projectForm(new ProJectForm(this)), m_dataSetForm(new DataSetForm(this)),
      m_trainDataForm(new TrainDataForm(this)), m_valSetForm(new ValSetForm(this)),
      m_commonSetForm(new CommonSetForm(this)), m_systemForm(new SystemForm(this))
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
                                 m_valSetForm,
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
            m_titleBarDragStartPosition = mouseEvent->globalPosition().toPoint();
            m_titleBarDragStartGeometry = geometry();
            m_isTitleBarDragging = true;
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove && m_isTitleBarDragging)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint delta = mouseEvent->globalPosition().toPoint() - m_titleBarDragStartPosition;
        if (isMaximized())
        {
            showNormal();
            m_titleBarDragStartPosition = mouseEvent->globalPosition().toPoint();
            m_titleBarDragStartGeometry = geometry();
            return true;
        }

        if (qAbs(delta.y()) >= qAbs(delta.x()))
        {
            const int minimumTop = m_titleBarDragStartGeometry.bottom() - minimumHeight() + 1;
            QRect resizedGeometry = m_titleBarDragStartGeometry;
            resizedGeometry.setTop(qMin(m_titleBarDragStartGeometry.top() + delta.y(), minimumTop));
            setGeometry(resizedGeometry);
        }
        else
        {
            move(m_titleBarDragStartGeometry.topLeft() + QPoint(delta.x(), 0));
        }
        return true;
    }
    else if (event->type() == QEvent::MouseButtonRelease && m_isTitleBarDragging)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            m_isTitleBarDragging = false;
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

void MainDlg::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);

    if (m_isConstrainingPosition || isMaximized() || isMinimized())
    {
        return;
    }

    QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr)
    {
        return;
    }

    const QRect availableGeometry = screen->availableGeometry();
    const QRect windowFrame = frameGeometry();
    if (windowFrame.width() > availableGeometry.width() || windowFrame.height() > availableGeometry.height())
    {
        return;
    }

    const int maxLeft = availableGeometry.right() - windowFrame.width() + 1;
    const int maxTop = availableGeometry.bottom() - windowFrame.height() + 1;
    const QPoint constrainedFrameTopLeft(qBound(availableGeometry.left(), windowFrame.left(), maxLeft),
                                         qBound(availableGeometry.top(), windowFrame.top(), maxTop));
    if (constrainedFrameTopLeft == windowFrame.topLeft())
    {
        return;
    }

    m_isConstrainingPosition = true;
    move(pos() + constrainedFrameTopLeft - windowFrame.topLeft());
    m_isConstrainingPosition = false;
}

void MainDlg::UpdateWindowControlButtons()
{
    ui->TB_Maximize->setText(isMaximized() ? QStringLiteral("▣") : QStringLiteral("□"));
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

    if (m_semiAutoAnnoForm != nullptr)
    {
        m_semiAutoAnnoForm->releaseSam3();
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
    m_systemForm->Refresh();
    ui->stackedWidget->setCurrentWidget(m_systemForm);
    SetCurrentNavButton(ui->PB_SystemSet);
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
