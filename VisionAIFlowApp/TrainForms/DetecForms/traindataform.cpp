#include "traindataform.h"

#include "TrainForms/trainingprotocol.h"
#include "taskrepository.h"
#include "release/ui_traindataform.h"
#include "ytyolodefine.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QUuid>
#include <QUrl>
#include <cmath>

TrainDataForm::TrainDataForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::TrainDataForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    TrainingProtocol::configurePythonProcess(m_process);
    connect(ui->buttonWeights,
            &QPushButton::clicked,
            this,
            [this]()
            {
                const QString path = QFileDialog::getOpenFileName(this,
                                                                  QString(u8"选择预训练模型"),
                                                                  ui->editWeights->text(),
                                                                  QStringLiteral("PyTorch (*.pt)"));
                if (!path.isEmpty())
                {
                    ui->editWeights->setText(path);
                }
            });
    connect(ui->buttonStart, &QPushButton::clicked, this, &TrainDataForm::startTraining);
    connect(ui->buttonStop, &QPushButton::clicked, this, &TrainDataForm::stopTraining);
    connect(ui->buttonExport, &QPushButton::clicked, this, &TrainDataForm::exportOnnx);
    connect(ui->buttonOpen,
            &QPushButton::clicked,
            this,
            [this]()
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputRoot()));
            });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TrainDataForm::readOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TrainDataForm::readError);
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &TrainDataForm::processFinished);
    connect(m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error)
            {
                logMessage(QString(u8"Python进程错误: %1").arg(m_process->errorString()));
                if (error == QProcess::FailedToStart)
                {
                    m_startTimer.stop();
                    if (m_operation == Operation::Training)
                    {
                        updateProgress(QStringLiteral("training_failed"));
                    }
                    m_operation = Operation::None;
                    setRunning(false);
                }
            });
    m_startTimer.setSingleShot(true);
    m_cancelTimer.setSingleShot(true);
    connect(&m_startTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                m_protocolError = true;
                logMessage(QString(u8"训练启动超时: 180秒内未完成首轮"));
                TrainingProtocol::terminateWithGrace(m_process, this);
            });
    connect(&m_cancelTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                if (!isrunstate())
                {
                    logMessage(QString(u8"等待取消超时, 正在终止进程"));
                    TrainingProtocol::terminateWithGrace(m_process, this);
                }
            });
    const QVector<QColor>
        colors{Qt::green, Qt::yellow, Qt::cyan, Qt::magenta, Qt::red, Qt::blue, Qt::gray, Qt::darkCyan};
    for (int i = 0; i < 8; ++i)
    {
        ui->chartMetrics->toAddLineSerices(colors.at(i), QString::number(i));
    }
    for (int i = 0; i < 6; ++i)
    {
        ui->chartLoss->toAddLineSerices(colors.at(i), QString::number(i));
    }
    ui->chartMetrics->toInitChart(QString(u8"验证指标"));
    ui->chartLoss->toInitChart(QString(u8"训练损失"));
    ui->chartMetrics->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->chartLoss->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->chartMetrics->toSetYAxis(0, 1, 5, QString(u8"精度"));
    ui->chartLoss->toSetYAxis(0, 1, 5, QStringLiteral("Loss"));
    setRunning(false);
}

TrainDataForm::~TrainDataForm()
{
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

void TrainDataForm::toSetProcessName(const QString &setName)
{
    m_taskName = setName;
}

void TrainDataForm::toInitShow()
{
    if (!isrunstate())
    {
        return;
    }
    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(m_taskName, &task, &error))
    {
        logMessage(error);
        return;
    }
    using visionaiflow::domain::ProjectType;
    if (task.type == ProjectType::Classification)
    {
        m_task = QStringLiteral("classify");
    }
    else if (task.type == ProjectType::Detection)
    {
        m_task = QStringLiteral("detect");
    }
    else if (task.type == ProjectType::InstanceSegmentation)
    {
        m_task = QStringLiteral("segment");
    }
    else
    {
        logMessage(QString(u8"当前项目类型不支持普通 YOLO 训练"));
        return;
    }
    ui->labelTaskValue->setText(TaskRepository::DisplayType(task.type));
    const bool classify = m_task == QStringLiteral("classify");
    ui->spinImageSize->setValue(classify ? 224 : 640);
    const QString suffix =
        classify ? QStringLiteral("-cls") : (m_task == QStringLiteral("segment") ? QStringLiteral("-seg") : QString());
    ui->editWeights->setText(QDir(YtYoloDefine::toGetPythonPath())
                                 .filePath(QStringLiteral("model/yolo11n") + suffix + QStringLiteral(".pt")));
    ui->spinMosaic->setEnabled(!classify);
    ui->spinFlip->setEnabled(!classify);
    configureCharts();
}

bool TrainDataForm::isrunstate() const
{
    return m_process->state() == QProcess::NotRunning;
}

QString TrainDataForm::outputRoot() const
{
    return QDir(TaskRepository::TrainRoot()).filePath(m_taskName + QStringLiteral("/yolo"));
}

bool TrainDataForm::writeJson(const QString &path, const QJsonObject &value)
{
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(value).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        logMessage(QString(u8"无法写入 %1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

void TrainDataForm::startTraining()
{
    const QString python = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("python.exe"));
    const QString script = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLO/training_worker.py"));
    const QString dataRoot = QDir(TaskRepository::DataRoot()).filePath(m_taskName);
    const QString data =
        m_task == QStringLiteral("classify") ? dataRoot : QDir(dataRoot).filePath(QStringLiteral("Config.yaml"));
    QString error;
    if (m_task.isEmpty())
    {
        error = QString(u8"未选择支持的任务类型");
    }
    else if (!QFileInfo(python).isFile())
    {
        error = QString(u8"Python 解释器不存在: %1").arg(python);
    }
    else if (!QFileInfo(script).isFile())
    {
        error = QString(u8"训练脚本不存在: %1").arg(script);
    }
    else if (!QFileInfo(ui->editWeights->text()).isFile())
    {
        error = QString(u8"预训练模型不存在: %1").arg(ui->editWeights->text());
    }
    else if (!QFileInfo::exists(data))
    {
        error = QString(u8"数据集不存在: %1").arg(data);
    }
    else if (ui->editDevice->text().trimmed().isEmpty() || ui->spinImageSize->value() % 32 != 0)
    {
        error = QString(u8"设备不能为空, 输入尺寸必须是 32 的倍数");
    }
    if (!error.isEmpty())
    {
        logMessage(error);
        QMessageBox::critical(this, QString(u8"启动失败"), error);
        return;
    }
    m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runDirectory = QDir(outputRoot()).filePath(QStringLiteral("runs/") + m_runId);
    if (!QDir().mkpath(m_runDirectory))
    {
        const QString error = QString(u8"无法创建运行目录: %1").arg(m_runDirectory);
        logMessage(error);
        QMessageBox::critical(this, QString(u8"启动失败"), error);
        return;
    }
    QJsonObject config{{"job_id", m_taskName},
                       {"run_id", m_runId},
                       {"task", m_task},
                       {"weights", ui->editWeights->text()},
                       {"data", data},
                       {"epochs", ui->spinEpochs->value()},
                       {"imgsz", ui->spinImageSize->value()},
                       {"batch", ui->spinBatch->value()},
                       {"workers", ui->spinWorkers->value()},
                       {"device", ui->editDevice->text().trimmed()},
                       {"lr0", ui->spinLearningRate->value()},
                       {"mosaic", ui->spinMosaic->value()},
                       {"fliplr", ui->spinFlip->value()}};
    const QString configPath = QDir(m_runDirectory).filePath(QStringLiteral("config.json"));
    if (!writeJson(configPath, config))
    {
        return;
    }
    configureCharts();
    m_pending.clear();
    m_errorDecoder.resetState();
    m_ready = false;
    m_finished = false;
    m_cancelled = false;
    m_cancelEvent = false;
    m_protocolError = false;
    m_operation = Operation::Training;
    setRunning(true);
    updateProgress(QStringLiteral("training"));
    m_startTimer.start(180000);
    m_process->start(python,
                     QStringList{QStringLiteral("-u"),
                                 script,
                                 QStringLiteral("--config"),
                                 configPath,
                                 QStringLiteral("--run-dir"),
                                 m_runDirectory});
}

void TrainDataForm::stopTraining()
{
    if (isrunstate())
    {
        return;
    }
    m_cancelled = true;
    m_startTimer.stop();
    if (m_operation == Operation::Training)
    {
        QString error;
        if (!TrainingProtocol::requestCancellation(m_process, &m_startTimer, &m_cancelTimer, &error))
        {
            m_protocolError = true;
            logMessage(error);
            TrainingProtocol::terminateWithGrace(m_process, this);
        }
    }
    else
    {
        m_process->terminate();
        m_cancelTimer.start(5000);
    }
    ui->buttonStop->setEnabled(false);
}

void TrainDataForm::exportOnnx()
{
    const QString python = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("python.exe"));
    const QString script = QDir(YtYoloDefine::toGetPythonPath()).filePath(QStringLiteral("YOLO/export_worker.py"));
    const QString weights =
        QFileDialog::getOpenFileName(this, QString(u8"选择导出模型"), outputRoot(), QStringLiteral("PyTorch (*.pt)"));
    if (weights.isEmpty())
    {
        return;
    }
    if (!QFileInfo(python).isFile() || !QFileInfo(script).isFile())
    {
        const QString error = QString(u8"Python 或导出脚本不存在: %1, %2").arg(python, script);
        logMessage(error);
        QMessageBox::critical(this, QString(u8"导出失败"), error);
        return;
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString runDir = QDir(outputRoot()).filePath(QStringLiteral("export_runs/") + id);
    if (!QDir().mkpath(runDir))
    {
        const QString error = QString(u8"无法创建目录: %1").arg(runDir);
        logMessage(error);
        QMessageBox::critical(this, QString(u8"导出失败"), error);
        return;
    }
    QJsonObject config{{"task", m_task},
                       {"model", weights},
                       {"output_dir", QDir(outputRoot()).filePath(QStringLiteral("exports/") + id)},
                       {"imgsz", ui->spinImageSize->value()},
                       {"dynamic", ui->checkDynamic->isChecked()},
                       {"provider", ui->comboProvider->currentText()}};
    const QString configPath = QDir(runDir).filePath(QStringLiteral("config.json"));
    if (!writeJson(configPath, config))
    {
        return;
    }
    m_pending.clear();
    m_errorDecoder.resetState();
    m_finished = false;
    m_cancelled = false;
    m_protocolError = false;
    m_operation = Operation::Export;
    setRunning(true);
    m_process->start(python, QStringList{QStringLiteral("-u"), script, QStringLiteral("--config"), configPath});
}

void TrainDataForm::readOutput()
{
    const QList<QByteArray> lines = TrainingProtocol::takeLines(m_pending, m_process->readAllStandardOutput());
    for (const QByteArray &line : lines)
    {
        processLine(line);
    }
}

void TrainDataForm::readError()
{
    logMessage(QString(m_errorDecoder.decode(m_process->readAllStandardError())).trimmed());
}

void TrainDataForm::processLine(const QByteArray &line)
{
    QJsonObject event;
    QString error;
    if (!TrainingProtocol::parseEvent(line, &event, &error))
    {
        m_protocolError = true;
        logMessage(error);
        return;
    }
    const QString type = event.value(QStringLiteral("type")).toString();
    if (m_operation == Operation::Training)
    {
        if (!TrainingProtocol::validateTrainingEvent(event, m_taskName, m_runId, m_task, &m_ready, &error))
        {
            m_protocolError = true;
            logMessage(error);
            return;
        }
        if (type == QStringLiteral("batch"))
        {
            const int epoch = event.value(QStringLiteral("epoch")).toInt();
            const int step = event.value(QStringLiteral("step")).toInt();
            const int total = event.value(QStringLiteral("total_steps")).toInt();
            if (epoch > 0 && total > 0)
            {
                ui->progressBar->setMaximum(ui->spinEpochs->value() * 1000);
                ui->progressBar->setValue((epoch - 1) * 1000 + qMin(step, total) * 1000 / total);
                ui->labelProgress->setText(
                    QString(u8"轮次 %1/%2, 批次 %3/%4").arg(epoch).arg(ui->spinEpochs->value()).arg(step).arg(total));
            }
        }
        else if (type == QStringLiteral("epoch"))
        {
            const int epoch = event.value(QStringLiteral("epoch")).toInt();
            if (epoch < 1)
            {
                m_protocolError = true;
                logMessage(QString(u8"轮次事件无效"));
                return;
            }
            m_startTimer.stop();
            const QStringList metrics =
                m_task == QStringLiteral("classify")
                    ? QStringList{"top1", "top5"}
                    : (m_task == QStringLiteral("detect")
                           ? QStringList{"box_precision", "box_recall", "box_map50", "box_map5095"}
                           : QStringList{"box_precision",
                                         "box_recall",
                                         "box_map50",
                                         "box_map5095",
                                         "mask_precision",
                                         "mask_recall",
                                         "mask_map50",
                                         "mask_map5095"});
            const QStringList losses = m_task == QStringLiteral("classify")
                                           ? QStringList{"loss", "val_loss"}
                                           : QStringList{"loss", "box_loss", "seg_loss", "cls_loss", "dfl_loss"};
            for (int i = 0; i < metrics.size(); ++i)
            {
                if (event.value(metrics[i]).isDouble())
                {
                    ui->chartMetrics->toInsertData(i, epoch, static_cast<float>(event.value(metrics[i]).toDouble()));
                }
            }
            for (int i = 0; i < losses.size(); ++i)
            {
                if (event.value(losses[i]).isDouble())
                {
                    ui->chartLoss->toInsertData(i, epoch, static_cast<float>(event.value(losses[i]).toDouble()));
                }
            }
            ui->progressBar->setValue(epoch * 1000);
        }
        else if (type == QStringLiteral("cancelled"))
        {
            m_cancelEvent = true;
        }
    }
    if (type == QStringLiteral("finished"))
    {
        m_finished = true;
    }
    else if (type == QStringLiteral("error"))
    {
        m_protocolError = true;
        logMessage(QString(u8"失败: %1").arg(event.value(QStringLiteral("message")).toString()));
    }
    if (type != QStringLiteral("batch") && type != QStringLiteral("heartbeat"))
    {
        logMessage(QString::fromUtf8(line));
    }
}

void TrainDataForm::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_startTimer.stop();
    m_cancelTimer.stop();
    readOutput();
    readError();
    if (!m_pending.trimmed().isEmpty())
    {
        processLine(m_pending.trimmed());
        m_pending.clear();
    }
    const Operation operation = m_operation;
    m_operation = Operation::None;
    setRunning(false);
    if (m_cancelled && !m_protocolError)
    {
        logMessage(m_cancelEvent ? QString(u8"训练已取消") : QString(u8"进程已终止, 未收到取消事件"));
        if (operation == Operation::Training)
        {
            updateProgress(QStringLiteral("dataset_ready"));
        }
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0 || !m_finished || m_protocolError)
    {
        logMessage(QString(u8"进程失败, 退出码 %1").arg(exitCode));
        if (operation == Operation::Training)
        {
            updateProgress(QStringLiteral("training_failed"));
        }
        return;
    }
    if (operation == Operation::Training)
    {
        const QString best = QDir(m_runDirectory).filePath(QStringLiteral("training/weights/best.pt"));
        if (!QFileInfo(best).isFile() || !writeJson(QDir(outputRoot()).filePath(QStringLiteral("latest.json")),
                                                    QJsonObject{{"task", m_task},
                                                                {"model", best},
                                                                {"imgsz", ui->spinImageSize->value()},
                                                                {"run_id", m_runId}}))
        {
            updateProgress(QStringLiteral("training_failed"));
            logMessage(QString(u8"训练完成但无法记录最佳模型"));
            return;
        }
        updateProgress(QStringLiteral("trained"));
        ui->labelProgress->setText(QString(u8"训练完成"));
    }
    else
    {
        logMessage(QString(u8"ONNX 导出和校验完成"));
    }
}

void TrainDataForm::configureCharts()
{
    const QStringList metricNames =
        m_task == QStringLiteral("classify")
            ? QStringList{"Top-1", "Top-5"}
            : (m_task == QStringLiteral("detect") ? QStringList{"Precision", "Recall", "mAP50", "mAP50-95"}
                                                  : QStringList{"Box Precision",
                                                                "Box Recall",
                                                                "Box mAP50",
                                                                "Box mAP50-95",
                                                                "Mask Precision",
                                                                "Mask Recall",
                                                                "Mask mAP50",
                                                                "Mask mAP50-95"});
    const QStringList lossNames = m_task == QStringLiteral("classify")
                                      ? QStringList{QString(u8"训练 Loss"), QString(u8"验证 Loss")}
                                      : QStringList{"Loss", "Box", "Seg", "Cls", "DFL"};
    ui->chartMetrics->removeAllPoints();
    ui->chartLoss->removeAllPoints();
    ui->chartMetrics->toSetMinVal(1, 0);
    ui->chartMetrics->toSetMaxVal(1, 1);
    ui->chartLoss->toSetMinVal(1, 0);
    ui->chartLoss->toSetMaxVal(1, 1);
    const auto metrics = ui->chartMetrics->chart()->series();
    const auto losses = ui->chartLoss->chart()->series();
    for (int i = 0; i < metrics.size(); ++i)
    {
        metrics[i]->setVisible(i < metricNames.size());
        if (i < metricNames.size())
            metrics[i]->setName(metricNames[i]);
    }
    for (int i = 0; i < losses.size(); ++i)
    {
        const bool visible = i < lossNames.size() && !(m_task == QStringLiteral("detect") && i == 2);
        losses[i]->setVisible(visible);
        if (i < lossNames.size())
            losses[i]->setName(lossNames[i]);
    }
    ui->chartMetrics->toSetXAxis(0, qMax(1, ui->spinEpochs->value()), 1, 10, QString(u8"轮次"), 1);
    ui->chartLoss->toSetXAxis(0, qMax(1, ui->spinEpochs->value()), 1, 10, QString(u8"轮次"), 1);
    ui->progressBar->setValue(0);
    ui->labelProgress->setText(QString(u8"等待训练"));
}

void TrainDataForm::setRunning(bool running)
{
    for (QWidget *widget : QList<QWidget *>{ui->buttonStart,
                                            ui->buttonExport,
                                            ui->buttonWeights,
                                            ui->spinEpochs,
                                            ui->spinImageSize,
                                            ui->spinBatch,
                                            ui->spinWorkers,
                                            ui->spinLearningRate,
                                            ui->editWeights,
                                            ui->editDevice,
                                            ui->spinMosaic,
                                            ui->spinFlip,
                                            ui->checkDynamic,
                                            ui->comboProvider})
    {
        widget->setEnabled(!running);
    }
    if (!running && m_task == QStringLiteral("classify"))
    {
        ui->spinMosaic->setEnabled(false);
        ui->spinFlip->setEnabled(false);
    }
    ui->buttonStop->setEnabled(running);
}

void TrainDataForm::logMessage(const QString &message)
{
    if (!message.isEmpty())
    {
        ui->log->appendPlainText(message);
    }
}

void TrainDataForm::updateProgress(const QString &progress)
{
    QString error;
    if (!TaskRepository::UpdateProgress(m_taskName, progress, &error))
    {
        logMessage(error);
    }
}
