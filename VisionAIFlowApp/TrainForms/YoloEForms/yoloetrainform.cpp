#include "yoloetrainform.h"

#include "taskrepository.h"
#include "ui_yoloetrainform.h"
#include "ytyolodefine.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QUrl>

namespace
{
constexpr auto kEventPrefix = "YT_YOLOE_EVENT:";
}

YoloeTrainForm::YoloeTrainForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::YoloeTrainForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &YoloeTrainForm::readProcessOutput);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &YoloeTrainForm::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, &YoloeTrainForm::processError);
    setRunning(false);
}

YoloeTrainForm::~YoloeTrainForm()
{
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

void YoloeTrainForm::setTaskName(const QString &taskName)
{
    m_taskName = taskName;
    ui->LE_TaskName->setText(taskName);
}

void YoloeTrainForm::initialize()
{
    ui->LE_Python->setText(QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("python.exe")));
    ui->LE_Data->setText(QDir(YtYoloDefine::toGetDataPath()).filePath(m_taskName + QStringLiteral("/Config.yaml")));
    ui->LE_Output->setText(outputRoot());
}

bool YoloeTrainForm::isIdle() const
{
    return m_process->state() == QProcess::NotRunning;
}

void YoloeTrainForm::on_PB_Start_clicked()
{
    QString errorMessage;
    if (!validateRequest(&errorMessage))
    {
        QMessageBox::critical(this, QString(u8"YOLOE训练失败"), errorMessage);
        return;
    }

    m_pendingOutput.clear();
    m_completeEventReceived = false;
    m_cancelled = false;
    ui->PTE_Log->clear();
    ui->PB_Progress->setValue(0);

    const QString runtimeRoot = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLOE"));
    const QString script = QDir(runtimeRoot).filePath(QStringLiteral("train_project_yoloe_seg.py"));
    QStringList arguments;
    arguments << script << QStringLiteral("--data") << ui->LE_Data->text() << QStringLiteral("--weights")
              << QDir(runtimeRoot).filePath(QStringLiteral("models/yoloe-11s-seg.pt")) << QStringLiteral("--project")
              << outputRoot() << QStringLiteral("--seg-epochs") << QString::number(ui->SB_SegEpochs->value())
              << QStringLiteral("--vpe-epochs") << QString::number(ui->SB_VpeEpochs->value())
              << QStringLiteral("--imgsz") << QString::number(ui->SB_ImageSize->value()) << QStringLiteral("--batch")
              << QString::number(ui->SB_Batch->value()) << QStringLiteral("--workers")
              << QString::number(ui->SB_Workers->value()) << QStringLiteral("--device") << QStringLiteral("0")
              << QStringLiteral("--reference-samples-per-class") << QString::number(ui->SB_ReferenceCount->value());

    m_process->setWorkingDirectory(runtimeRoot);
    setRunning(true);
    updateTaskProgress(QStringLiteral("training"));
    appendLog(QString(u8"启动 YOLOE Segment + SAVPE 两阶段训练"));
    m_process->start(ui->LE_Python->text(), arguments);
}

void YoloeTrainForm::on_PB_Stop_clicked()
{
    if (isIdle())
    {
        return;
    }
    m_cancelled = true;
    m_process->terminate();
    if (!m_process->waitForFinished(3000))
    {
        m_process->kill();
    }
}

void YoloeTrainForm::on_PB_OpenOutput_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputRoot()));
}

void YoloeTrainForm::readProcessOutput()
{
    m_pendingOutput.append(m_process->readAllStandardOutput());
    int newline = -1;
    while ((newline = m_pendingOutput.indexOf('\n')) >= 0)
    {
        const QByteArray line = m_pendingOutput.left(newline).trimmed();
        m_pendingOutput.remove(0, newline + 1);
        if (line.startsWith(kEventPrefix))
        {
            handleEvent(line.mid(QByteArray(kEventPrefix).size()));
        }
        else if (!line.isEmpty())
        {
            appendLog(QString::fromUtf8(line));
        }
    }
}

void YoloeTrainForm::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readProcessOutput();
    setRunning(false);
    if (m_cancelled)
    {
        appendLog(QString(u8"训练已取消"));
        updateTaskProgress(QStringLiteral("dataset_ready"));
        return;
    }
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && m_completeEventReceived)
    {
        appendLog(QString(u8"YOLOE训练、导出和校验全部完成"));
        updateTaskProgress(QStringLiteral("trained"));
        return;
    }
    appendLog(QString(u8"YOLOE进程异常结束，退出码 %1").arg(exitCode));
    updateTaskProgress(QStringLiteral("training_failed"));
}

void YoloeTrainForm::processError(QProcess::ProcessError error)
{
    if (!m_cancelled)
    {
        appendLog(QString(u8"Python进程错误 %1: %2").arg(static_cast<int>(error)).arg(m_process->errorString()));
    }
}

bool YoloeTrainForm::validateRequest(QString *errorMessage) const
{
    if (m_taskName.isEmpty())
    {
        *errorMessage = QString(u8"未选择项目");
        return false;
    }
    if (!QFileInfo::exists(ui->LE_Python->text()))
    {
        *errorMessage = QString(u8"未找到Python解释器: %1").arg(ui->LE_Python->text());
        return false;
    }
    const QString script =
        QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLOE/train_project_yoloe_seg.py"));
    if (!QFileInfo::exists(script))
    {
        *errorMessage = QString(u8"未部署YOLOE训练入口: %1").arg(script);
        return false;
    }
    if (!QFileInfo::exists(ui->LE_Data->text()))
    {
        *errorMessage = QString(u8"未生成YOLO分割数据集: %1").arg(ui->LE_Data->text());
        return false;
    }
    if (ui->SB_ImageSize->value() % 32 != 0)
    {
        *errorMessage = QString(u8"输入尺寸必须能被32整除");
        return false;
    }
    return true;
}

void YoloeTrainForm::appendLog(const QString &message)
{
    ui->PTE_Log->appendPlainText(message);
}

void YoloeTrainForm::handleEvent(const QByteArray &line)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        appendLog(QString(u8"无效YOLOE事件: %1").arg(QString::fromUtf8(line)));
        return;
    }
    const QJsonObject event = document.object();
    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("epoch"))
    {
        const int current = event.value(QStringLiteral("globalEpoch")).toInt();
        const int total = ui->SB_SegEpochs->value() + ui->SB_VpeEpochs->value();
        ui->PB_Progress->setMaximum(total);
        ui->PB_Progress->setValue(current);
    }
    else if (type == QStringLiteral("complete"))
    {
        m_completeEventReceived = true;
    }
    else if (type == QStringLiteral("error"))
    {
        appendLog(QString(u8"失败: %1").arg(event.value(QStringLiteral("message")).toString()));
    }
    appendLog(QString::fromUtf8(line));
}

void YoloeTrainForm::setRunning(bool running)
{
    ui->PB_Start->setEnabled(!running);
    ui->PB_Stop->setEnabled(running);
}

void YoloeTrainForm::updateTaskProgress(const QString &progress)
{
    QString errorMessage;
    if (!TaskRepository::UpdateProgress(m_taskName, progress, &errorMessage))
    {
        appendLog(errorMessage);
    }
}

QString YoloeTrainForm::outputRoot() const
{
    return QDir(YtYoloDefine::toGetTrainPath()).filePath(m_taskName + QStringLiteral("/yoloe"));
}
