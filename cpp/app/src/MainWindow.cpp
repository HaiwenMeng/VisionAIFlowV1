#include "visionaiflow/app/MainWindow.h"
#include "visionaiflow/app/CreateProjectDialog.h"

#include "ui_MainWindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QMessageBox>

namespace visionaiflow::app
{
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->createProjectButton, &QPushButton::clicked, this, [this]() { CreateProjectDialog dialog(this); dialog.exec(); });
    connect(ui->trainerStartButton, &QPushButton::clicked, this, [this]() { StartHost(QStringLiteral("trainer"), QStringLiteral("VisionTrainerHost.exe")); });
    connect(ui->tensorRtStartButton, &QPushButton::clicked, this, [this]() { StartHost(QStringLiteral("tensorrt"), QStringLiteral("VisionTensorRtHost.exe")); });
    connect(ui->openVinoStartButton, &QPushButton::clicked, this, [this]() { StartHost(QStringLiteral("openvino"), QStringLiteral("VisionOpenVinoHost.exe")); });
    connect(&m_supervisor, &HostSupervisor::HostStateChanged, this, &MainWindow::SetStatus);
    connect(&m_supervisor, &HostSupervisor::HostError, this, [this](const QString &role, const QString &code, const QString &message) {
        SetStatus(role, code);
        QMessageBox::critical(this, QString(u8"Host 错误"), code + QStringLiteral(": ") + message);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_supervisor.HasRunningHosts())
    {
        event->accept();
        return;
    }
    const auto choice = QMessageBox::question(this, QString(u8"确认关闭"), QString(u8"仍有 Host 正在运行. 是否发送结构化停止请求并等待其停止?"));
    if (choice == QMessageBox::Yes)
    {
        for (const QString &role : {QStringLiteral("trainer"), QStringLiteral("tensorrt"), QStringLiteral("openvino")})
        {
            const auto stopped = m_supervisor.ShutdownHost(role);
            if (!stopped.IsSuccess() && stopped.Failure().code != foundation::ErrorCode::InvalidArgument && stopped.Failure().code != foundation::ErrorCode::InvalidState)
            {
                QMessageBox::warning(this, QString(u8"停止失败"), QString::fromStdString(stopped.Failure().message));
            }
        }
    }
    event->ignore();
}

void MainWindow::StartHost(const QString &role, const QString &executableName)
{
    const QString executablePath = QCoreApplication::applicationDirPath() + QLatin1Char('/') + executableName;
    const auto started = m_supervisor.StartHost(role, executablePath);
    if (!started.IsSuccess())
    {
        QMessageBox::critical(this, QString(u8"启动失败"), QString::fromStdString(started.Failure().message));
    }
}

void MainWindow::SetStatus(const QString &role, const QString &state)
{
    const QString text = state == QStringLiteral("starting") ? QString(u8"启动中") :
        state == QStringLiteral("running") ? QString(u8"运行中") :
        state == QStringLiteral("stopping") ? QString(u8"停止中") :
        state == QStringLiteral("completed") ? QString(u8"已完成") :
        state == QStringLiteral("stopped") ? QString(u8"已停止") : state;
    if (role == QStringLiteral("trainer")) ui->trainerStatusLabel->setText(text);
    else if (role == QStringLiteral("tensorrt")) ui->tensorRtStatusLabel->setText(text);
    else if (role == QStringLiteral("openvino")) ui->openVinoStatusLabel->setText(text);
}
}
