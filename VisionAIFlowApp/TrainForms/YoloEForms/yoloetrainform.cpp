#include "yoloetrainform.h"

#include "taskrepository.h"
#include "TrainForms/trainingprotocol.h"
#include "ui_yoloetrainform.h"
#include "ytyolodefine.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QUuid>
#include <QUrl>
#include <QtMath>
#include <cmath>

namespace
{
constexpr auto kEventPrefix = "YT_YOLOE_EVENT:";
}

YoloeTrainForm::YoloeTrainForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::YoloeTrainForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    TrainingProtocol::configurePythonProcess(m_process);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &YoloeTrainForm::readProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &YoloeTrainForm::readProcessError);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &YoloeTrainForm::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, &YoloeTrainForm::processError);
    m_startTimer.setSingleShot(true);
    m_cancelTimer.setSingleShot(true);
    connect(&m_startTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                appendLog(QString(u8"训练启动超时: 180秒内未完成首轮"));
                m_protocolError = true;
                TrainingProtocol::terminateWithGrace(m_process, this);
            });
    connect(&m_cancelTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                if (!isIdle())
                {
                    appendLog(QString(u8"等待取消超时, 正在终止进程"));
                    TrainingProtocol::terminateWithGrace(m_process, this);
                }
            });
    const QStringList metricNames{QStringLiteral("Precision"),
                                  QStringLiteral("Recall"),
                                  QStringLiteral("mAP50"),
                                  QStringLiteral("mAP50-95")};
    const QStringList lossNames{QStringLiteral("Loss"),
                                QStringLiteral("Box"),
                                QStringLiteral("Seg"),
                                QStringLiteral("Cls"),
                                QStringLiteral("DFL")};
    const QVector<QColor> colors{Qt::green, Qt::yellow, Qt::cyan, Qt::magenta, Qt::red};
    for (int stage = 0; stage < 2; ++stage)
    {
        for (int i = 0; i < metricNames.size(); ++i)
            ui->chartMetrics->toAddLineSerices(stage == 0 ? colors[i] : colors[i].darker(160),
                                               (stage == 0 ? QStringLiteral("Seg ") : QStringLiteral("VPE ")) +
                                                   metricNames[i]);
        for (int i = 0; i < lossNames.size(); ++i)
            ui->chartLoss->toAddLineSerices(stage == 0 ? colors[i] : colors[i].darker(160),
                                            (stage == 0 ? QStringLiteral("Seg ") : QStringLiteral("VPE ")) +
                                                lossNames[i]);
    }
    ui->chartMetrics->toInitChart(QString(u8"验证精度"));
    ui->chartLoss->toInitChart(QString(u8"训练损失"));
    ui->chartMetrics->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->chartLoss->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->chartMetrics->toSetYAxis(0, 1, 5, QString(u8"精度"));
    ui->chartLoss->toSetYAxis(0, 1, 5, QStringLiteral("Loss"));
    resetCharts();
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
    m_errorDecoder.resetState();
    m_completeEventReceived = false;
    m_cancelled = false;
    m_protocolError = false;
    m_workerReady = false;
    m_cancelEventReceived = false;
    m_completedSegEpochs = 0;
    m_runningOperation = RunningOperation::Training;
    ui->PTE_Log->clear();
    ui->PB_Progress->setValue(0);
    resetCharts();

    const QString runtimeRoot = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLOE"));
    const QString script = QDir(runtimeRoot).filePath(QStringLiteral("train_project_yoloe_seg.py"));
    const int inputImageSize = effectiveImageSize();
    m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString runDirectory = QDir(outputRoot()).filePath(QStringLiteral("runs/") + m_runId);
    if (!QDir().mkpath(runDirectory))
    {
        QMessageBox::critical(this, QString(u8"YOLOE训练失败"), QString(u8"无法创建运行目录: %1").arg(runDirectory));
        m_runningOperation = RunningOperation::None;
        return;
    }
    const QJsonObject config{{"job_id", m_taskName},
                             {"run_id", m_runId},
                             {"task", QStringLiteral("yoloe")},
                             {"data", ui->LE_Data->text()},
                             {"weights", QDir(runtimeRoot).filePath(QStringLiteral("models/yoloe-11s-seg.pt"))},
                             {"project", outputRoot()},
                             {"seg_epochs", ui->SB_SegEpochs->value()},
                             {"vpe_epochs", ui->SB_VpeEpochs->value()},
                             {"imgsz", inputImageSize},
                             {"batch", ui->SB_Batch->value()},
                             {"workers", ui->SB_Workers->value()},
                             {"device", QStringLiteral("0")},
                             {"reference_samples_per_class", ui->SB_ReferenceCount->value()}};
    const QString configPath = QDir(runDirectory).filePath(QStringLiteral("config.json"));
    QSaveFile configFile(configPath);
    const QByteArray configBytes = QJsonDocument(config).toJson();
    if (!configFile.open(QIODevice::WriteOnly) || configFile.write(configBytes) != configBytes.size() ||
        !configFile.commit())
    {
        QMessageBox::critical(this, QString(u8"YOLOE训练失败"), QString(u8"无法写入训练配置: %1").arg(configPath));
        m_runningOperation = RunningOperation::None;
        return;
    }
    const QStringList arguments{QStringLiteral("-u"),
                                script,
                                QStringLiteral("--config"),
                                configPath,
                                QStringLiteral("--run-dir"),
                                runDirectory};

    m_process->setWorkingDirectory(runtimeRoot);
    setRunning(true);
    updateTaskProgress(QStringLiteral("training"));
    appendLog(QString(u8"启动 YOLOE Segment + SAVPE 两阶段训练, 原始尺寸 %1, 缩放比例 %2, 实际输入尺寸 %3")
                  .arg(ui->SB_ImageSize->value())
                  .arg(ui->DSB_ZoomRatio->value(), 0, 'f', 2)
                  .arg(inputImageSize));
    m_process->start(ui->LE_Python->text(), arguments);
    m_startTimer.start(180000);
}

void YoloeTrainForm::on_PB_Stop_clicked()
{
    if (isIdle())
    {
        return;
    }
    m_cancelled = true;
    if (m_runningOperation == RunningOperation::Training)
    {
        QString error;
        if (!TrainingProtocol::requestCancellation(m_process, &m_startTimer, &m_cancelTimer, &error))
        {
            m_protocolError = true;
            appendLog(error);
            TrainingProtocol::terminateWithGrace(m_process, this);
            return;
        }
        appendLog(QString(u8"已请求停止训练"));
    }
    else
        m_process->terminate();
}

void YoloeTrainForm::on_PB_ExportOnnx_clicked()
{
    if (!isIdle())
    {
        QMessageBox::critical(this, QString(u8"YOLOE导出失败"), QString(u8"YOLOE进程正在运行"));
        return;
    }

    QString errorMessage;
    if (!validateExportRequest(&errorMessage))
    {
        QMessageBox::critical(this, QString(u8"YOLOE导出失败"), errorMessage);
        return;
    }

    const QString runtimeRoot = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLOE"));
    const QString script = QDir(runtimeRoot).filePath(QStringLiteral("export_trained_yoloe_seg.py"));
    const QString checkpoint = QDir(outputRoot()).filePath(QStringLiteral("vp_seg_stage/weights/best.pt"));
    const QString outputDirectory = QDir(outputRoot()).filePath(QStringLiteral("model11"));
    const QStringList arguments{script,
                                QStringLiteral("--weights"),
                                checkpoint,
                                QStringLiteral("--data"),
                                ui->LE_Data->text(),
                                QStringLiteral("--output"),
                                outputDirectory,
                                QStringLiteral("--imgsz"),
                                QString::number(effectiveImageSize()),
                                QStringLiteral("--contract"),
                                QStringLiteral("detect")};

    m_pendingOutput.clear();
    m_errorDecoder.resetState();
    m_completeEventReceived = false;
    m_cancelled = false;
    m_runningOperation = RunningOperation::ExportOnnx;
    m_protocolError = false;
    m_process->setWorkingDirectory(runtimeRoot);
    setRunning(true);
    appendLog(QString(u8"开始导出YOLOE检测契约和VPE ONNX模型: %1").arg(checkpoint));
    m_process->start(ui->LE_Python->text(), arguments);
}

void YoloeTrainForm::on_PB_OpenOutput_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputRoot()));
}

void YoloeTrainForm::readProcessOutput()
{
    const QList<QByteArray> lines = TrainingProtocol::takeLines(m_pendingOutput, m_process->readAllStandardOutput());
    for (QByteArray line : lines)
    {
        if (m_runningOperation != RunningOperation::Training)
            line.replace(QByteArray(1, '\0'), QByteArray());
        if (m_runningOperation == RunningOperation::Training && !line.isEmpty())
        {
            handleEvent(line);
        }
        else if (line.startsWith(kEventPrefix))
        {
            handleEvent(line.mid(QByteArray(kEventPrefix).size()));
        }
        else if (!line.isEmpty())
        {
            appendLog(QString::fromUtf8(line));
        }
    }
}

void YoloeTrainForm::readProcessError()
{
    const QByteArray data = m_process->readAllStandardError();
    if (!data.isEmpty())
        appendLog(QString(m_errorDecoder.decode(data)).trimmed());
}

void YoloeTrainForm::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readProcessOutput();
    readProcessError();
    m_startTimer.stop();
    m_cancelTimer.stop();
    if (!m_pendingOutput.trimmed().isEmpty())
    {
        if (m_runningOperation == RunningOperation::Training)
            handleEvent(m_pendingOutput.trimmed());
        else
            appendLog(QString::fromUtf8(m_pendingOutput));
        m_pendingOutput.clear();
    }
    const RunningOperation operation = m_runningOperation;
    m_runningOperation = RunningOperation::None;
    setRunning(false);
    if (m_cancelled && !m_protocolError)
    {
        appendLog(
            operation == RunningOperation::ExportOnnx
                ? QString(u8"ONNX导出已取消")
                : (m_cancelEventReceived ? QString(u8"训练已取消") : QString(u8"训练进程已终止, 未收到取消事件")));
        if (operation == RunningOperation::Training)
        {
            ui->LB_TrainingStatus->setText(QString(u8"训练已取消"));
            updateTaskProgress(QStringLiteral("dataset_ready"));
        }
        return;
    }
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && m_completeEventReceived && !m_protocolError)
    {
        if (operation == RunningOperation::ExportOnnx)
        {
            const QString modelDirectory = QDir(outputRoot()).filePath(QStringLiteral("model11"));
            const QString detectOnnx = QDir(modelDirectory).filePath(QStringLiteral("yoloe_detect.onnx"));
            const QString vpeOnnx = QDir(modelDirectory).filePath(QStringLiteral("yoloe_vpe.onnx"));
            if (!QFileInfo(detectOnnx).isFile() || QFileInfo(detectOnnx).size() <= 0 || !QFileInfo(vpeOnnx).isFile() ||
                QFileInfo(vpeOnnx).size() <= 0)
            {
                appendLog(QString(u8"ONNX导出进程完成，但未生成有效模型: %1, %2").arg(detectOnnx, vpeOnnx));
                QMessageBox::critical(this,
                                      QString(u8"YOLOE导出失败"),
                                      QString(u8"未生成有效的检测契约或VPE ONNX模型"));
                return;
            }
            appendLog(QString(u8"YOLOE检测契约 ONNX导出完成: %1").arg(detectOnnx));
            appendLog(QString(u8"YOLOE VPE ONNX导出完成: %1").arg(vpeOnnx));
            QMessageBox::information(this,
                                     QString(u8"YOLOE导出完成"),
                                     QString(u8"检测契约和VPE ONNX模型已导出到: %1").arg(modelDirectory));
        }
        else
        {
            appendLog(QString(u8"YOLOE训练、导出和校验全部完成"));
            updateTaskProgress(QStringLiteral("trained"));
        }
        return;
    }
    appendLog(operation == RunningOperation::ExportOnnx ? QString(u8"YOLOE导出进程异常结束，退出码 %1").arg(exitCode)
                                                        : QString(u8"YOLOE训练进程异常结束，退出码 %1").arg(exitCode));
    if (operation == RunningOperation::Training)
    {
        ui->LB_TrainingStatus->setText(QString(u8"训练失败"));
        updateTaskProgress(QStringLiteral("training_failed"));
    }
}

void YoloeTrainForm::processError(QProcess::ProcessError error)
{
    if (!m_cancelled)
    {
        appendLog(QString(u8"Python进程错误 %1: %2").arg(static_cast<int>(error)).arg(m_process->errorString()));
        if (error == QProcess::FailedToStart)
        {
            m_startTimer.stop();
            setRunning(false);
            if (m_runningOperation == RunningOperation::Training)
                updateTaskProgress(QStringLiteral("training_failed"));
            m_runningOperation = RunningOperation::None;
        }
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
    if (ui->DSB_ZoomRatio->value() <= 0.0 || ui->DSB_ZoomRatio->value() > 1.0)
    {
        *errorMessage = QString(u8"缩放比例必须大于0且不超过1");
        return false;
    }
    if (qRound(ui->SB_ImageSize->value() * ui->DSB_ZoomRatio->value()) < 32)
    {
        *errorMessage = QString(u8"缩放后的训练输入尺寸不能小于32");
        return false;
    }
    return true;
}

bool YoloeTrainForm::validateExportRequest(QString *errorMessage) const
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
        QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLOE/export_trained_yoloe_seg.py"));
    if (!QFileInfo::exists(script))
    {
        *errorMessage = QString(u8"未部署YOLOE导出入口: %1").arg(script);
        return false;
    }
    if (!QFileInfo::exists(ui->LE_Data->text()))
    {
        *errorMessage = QString(u8"未生成YOLO分割数据集: %1").arg(ui->LE_Data->text());
        return false;
    }
    const QString checkpoint = QDir(outputRoot()).filePath(QStringLiteral("vp_seg_stage/weights/best.pt"));
    if (!QFileInfo(checkpoint).isFile())
    {
        *errorMessage = QString(u8"未找到YOLOE VPE阶段最佳模型: %1").arg(checkpoint);
        return false;
    }
    if (ui->SB_ImageSize->value() <= 0 || ui->SB_ImageSize->value() % 32 != 0)
    {
        *errorMessage = QString(u8"输入尺寸必须为正数且能被32整除");
        return false;
    }
    if (ui->DSB_ZoomRatio->value() <= 0.0 || ui->DSB_ZoomRatio->value() > 1.0)
    {
        *errorMessage = QString(u8"缩放比例必须大于0且不超过1");
        return false;
    }
    if (qRound(ui->SB_ImageSize->value() * ui->DSB_ZoomRatio->value()) < 32)
    {
        *errorMessage = QString(u8"缩放后的导出输入尺寸不能小于32");
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
    QJsonObject event;
    QString error;
    if (!TrainingProtocol::parseEvent(line, &event, &error))
    {
        appendLog(error);
        if (m_runningOperation == RunningOperation::Training)
            m_protocolError = true;
        return;
    }
    const QString type = event.value(QStringLiteral("type")).toString();
    if (m_runningOperation == RunningOperation::Training)
    {
        if (!TrainingProtocol::validateTrainingEvent(event,
                                                     m_taskName,
                                                     m_runId,
                                                     QStringLiteral("yoloe"),
                                                     &m_workerReady,
                                                     &error))
        {
            appendLog(error);
            m_protocolError = true;
            return;
        }
        if (type == QStringLiteral("batch"))
        {
            const int stageOffset =
                event.value(QStringLiteral("stage")).toString() == QStringLiteral("vpe") ? m_completedSegEpochs : 0;
            const int epoch = event.value(QStringLiteral("epoch")).toInt();
            const int step = event.value(QStringLiteral("step")).toInt();
            const int totalSteps = event.value(QStringLiteral("total_steps")).toInt();
            if (epoch > 0 && totalSteps > 0)
            {
                ui->PB_Progress->setMaximum((ui->SB_SegEpochs->value() + ui->SB_VpeEpochs->value()) * 1000);
                ui->PB_Progress->setValue((stageOffset + epoch - 1) * 1000 +
                                          qMin(step, totalSteps) * 1000 / totalSteps);
            }
            ui->LB_TrainingStatus->setText(QString(u8"阶段: %1, 轮次: %2/%3, 批次: %4/%5")
                                               .arg(event.value("stage").toString())
                                               .arg(event.value("epoch").toInt())
                                               .arg(event.value("total_epochs").toInt())
                                               .arg(event.value("step").toInt())
                                               .arg(event.value("total_steps").toInt()));
        }
        else if (type == QStringLiteral("epoch"))
        {
            const int x = event.value(QStringLiteral("global_epoch")).toInt();
            const QString stage = event.value(QStringLiteral("stage")).toString();
            const bool vpe = stage == QStringLiteral("vpe");
            if ((stage != QStringLiteral("seg") && !vpe) || x < 1 || event.value(QStringLiteral("epoch")).toInt() < 1)
            {
                appendLog(QString(u8"YOLOE轮次事件缺少有效阶段或轮次: %1").arg(QString::fromUtf8(line)));
                m_protocolError = true;
                return;
            }
            m_startTimer.stop();
            if (!vpe)
                m_completedSegEpochs = event.value(QStringLiteral("epoch")).toInt();
            const QStringList metrics{QStringLiteral("precision"),
                                      QStringLiteral("recall"),
                                      QStringLiteral("map50"),
                                      QStringLiteral("map5095")};
            const QStringList losses{QStringLiteral("loss"),
                                     QStringLiteral("box_loss"),
                                     QStringLiteral("seg_loss"),
                                     QStringLiteral("cls_loss"),
                                     QStringLiteral("dfl_loss")};
            for (int i = 0; i < metrics.size(); ++i)
                if (event.value(metrics[i]).isDouble() && std::isfinite(event.value(metrics[i]).toDouble()))
                    ui->chartMetrics->toInsertData((vpe ? 4 : 0) + i,
                                                   x,
                                                   static_cast<float>(event.value(metrics[i]).toDouble()));
            for (int i = 0; i < losses.size(); ++i)
                if (event.value(losses[i]).isDouble() && std::isfinite(event.value(losses[i]).toDouble()))
                {
                    const double value = event.value(losses[i]).toDouble();
                    ui->chartLoss->toInsertData((vpe ? 5 : 0) + i, x, static_cast<float>(value));
                }
            ui->PB_Progress->setMaximum((ui->SB_SegEpochs->value() + ui->SB_VpeEpochs->value()) * 1000);
            ui->PB_Progress->setValue(x * 1000);
        }
        else if (type == QStringLiteral("finished"))
        {
            m_completeEventReceived = true;
            if (!m_cancelled && !m_protocolError)
            {
                ui->PB_Progress->setValue(ui->PB_Progress->maximum());
                ui->LB_TrainingStatus->setText(QString(u8"训练已完成"));
            }
        }
        else if (type == QStringLiteral("cancelled"))
            m_cancelEventReceived = true;
        else if (type == QStringLiteral("error"))
        {
            m_protocolError = true;
            appendLog(QString(u8"失败: %1").arg(event.value(QStringLiteral("message")).toString()));
        }
        else if (type != QStringLiteral("worker_ready") && type != QStringLiteral("started") &&
                 type != QStringLiteral("stage_started") && type != QStringLiteral("checkpoint") &&
                 type != QStringLiteral("heartbeat") && type != QStringLiteral("reference_ready") &&
                 type != QStringLiteral("log"))
            appendLog(QString(u8"未知YOLOE事件: %1").arg(type));
        if (type != QStringLiteral("batch") && type != QStringLiteral("heartbeat"))
            appendLog(QString::fromUtf8(line));
        return;
    }
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

void YoloeTrainForm::resetCharts()
{
    const int total = qMax(1, ui->SB_SegEpochs->value() + ui->SB_VpeEpochs->value());
    ui->chartMetrics->removeAllPoints();
    ui->chartLoss->removeAllPoints();
    ui->chartMetrics->toSetMinVal(1, 0);
    ui->chartMetrics->toSetMaxVal(1, 1);
    ui->chartLoss->toSetMinVal(1, 0);
    ui->chartLoss->toSetMaxVal(1, 1);
    ui->chartMetrics->toSetXAxis(0, total, 1, 10, QString(u8"轮次"), 1);
    ui->chartLoss->toSetXAxis(0, total, 1, 10, QString(u8"轮次"), 1);
    ui->LB_TrainingStatus->setText(QString(u8"等待训练"));
}

void YoloeTrainForm::setRunning(bool running)
{
    ui->PB_Start->setEnabled(!running);
    ui->PB_Stop->setEnabled(running);
    ui->PB_ExportOnnx->setEnabled(!running);
    ui->SB_SegEpochs->setEnabled(!running);
    ui->SB_VpeEpochs->setEnabled(!running);
    ui->SB_ImageSize->setEnabled(!running);
    ui->SB_Batch->setEnabled(!running);
    ui->SB_Workers->setEnabled(!running);
    ui->SB_ReferenceCount->setEnabled(!running);
    ui->DSB_ZoomRatio->setEnabled(!running);
}

void YoloeTrainForm::updateTaskProgress(const QString &progress)
{
    QString errorMessage;
    if (!TaskRepository::UpdateProgress(m_taskName, progress, &errorMessage))
    {
        appendLog(errorMessage);
    }
}

int YoloeTrainForm::effectiveImageSize() const
{
    const int scaledSize = qRound(ui->SB_ImageSize->value() * ui->DSB_ZoomRatio->value());
    return qMax(32, ((scaledSize + 31) / 32) * 32);
}

QString YoloeTrainForm::outputRoot() const
{
    return QDir(YtYoloDefine::toGetTrainPath()).filePath(m_taskName + QStringLiteral("/yoloe"));
}
