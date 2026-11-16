#include "linetrainform.h"

#include "TrainForms/trainingprotocol.h"
#include "taskrepository.h"
#include "ytyolodefine.h"
#include "release/ui_linetrainform.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QUuid>

LineTrainForm::LineTrainForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::LineTrainForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    TrainingProtocol::configurePythonProcess(m_process);
    ui->metrics->toAddLineSerices(Qt::green, QStringLiteral("Precision"));
    ui->metrics->toAddLineSerices(Qt::yellow, QStringLiteral("Recall"));
    ui->metrics->toAddLineSerices(Qt::cyan, QStringLiteral("F1"));
    ui->loss->toAddLineSerices(Qt::red, QStringLiteral("Loss"));
    ui->metrics->toInitChart(QString(u8"验证指标"));
    ui->loss->toInitChart(QString(u8"训练损失"));
    ui->metrics->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->loss->toSetXAxis(0, 1, 1, 10, QString(u8"轮次"));
    ui->metrics->toSetYAxis(0, 1, 5, QString(u8"精度"));
    ui->loss->toSetYAxis(0, 1, 5, QStringLiteral("Loss"));
    m_startTimer.setSingleShot(true);
    m_cancelTimer.setSingleShot(true);
    connect(ui->start, &QPushButton::clicked, this, &LineTrainForm::startTraining);
    connect(ui->stop, &QPushButton::clicked, this, &LineTrainForm::stopTraining);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LineTrainForm::readOutput);
    connect(m_process,
            &QProcess::readyReadStandardError,
            this,
            [this]()
            {
                ui->log->appendPlainText(QString::fromUtf8(m_process->readAllStandardError()).trimmed());
            });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &LineTrainForm::finish);
    connect(m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error)
            {
                reportError(QString(u8"训练进程错误: %1").arg(m_process->errorString()));
                if (error == QProcess::FailedToStart)
                {
                    m_startTimer.stop();
                    setTaskProgress(QStringLiteral("training_failed"));
                    setRunning(false);
                }
            });
    connect(&m_startTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                reportError(QString(u8"训练启动超时, 180秒内未完成首轮"));
                TrainingProtocol::terminateWithGrace(m_process, this);
            });
    connect(&m_cancelTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                TrainingProtocol::terminateWithGrace(m_process, this);
            });
    setRunning(false);
}

LineTrainForm::~LineTrainForm()
{
    if (!isIdle())
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

void LineTrainForm::initialize(const QString &taskName)
{
    m_taskName = taskName;
}

bool LineTrainForm::isIdle() const
{
    return m_process->state() == QProcess::NotRunning;
}

void LineTrainForm::startTraining()
{
    const QDir pythonRoot(YtYoloDefine::toGetPythonPath());
    const QString python = pythonRoot.filePath(QStringLiteral("python.exe"));
    const QString script = pythonRoot.filePath(QStringLiteral("pidiNet/training_worker.py"));
    const QString weights = pythonRoot.filePath(QStringLiteral("pidiNet/table5_pidinet.pth"));
    const QString dataset = QDir(TaskRepository::DataRoot()).filePath(m_taskName + QStringLiteral("/dataset.json"));
    if (!QFileInfo(python).isFile() || !QFileInfo(script).isFile() || !QFileInfo(weights).isFile() ||
        !QFileInfo(dataset).isFile())
    {
        reportError(QString(u8"训练文件缺失: %1, %2, %3, %4").arg(python, script, weights, dataset));
        return;
    }
    m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runDir = QDir(TaskRepository::TrainRoot()).filePath(m_taskName + QStringLiteral("/pidinet/runs/") + m_runId);
    if (!QDir().mkpath(m_runDir))
    {
        reportError(QString(u8"无法创建训练目录: %1").arg(m_runDir));
        return;
    }
    const QJsonObject config{{"job_id", m_taskName},
                             {"run_id", m_runId},
                             {"dataset", dataset},
                             {"weights", weights},
                             {"epochs", ui->epochs->value()},
                             {"learning_rate", ui->rate->value()}};
    QSaveFile file(QDir(m_runDir).filePath(QStringLiteral("config.json")));
    const QByteArray bytes = QJsonDocument(config).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        reportError(QString(u8"无法保存训练配置: %1").arg(file.errorString()));
        return;
    }
    ui->metrics->removeAllPoints();
    ui->loss->removeAllPoints();
    ui->metrics->toSetXAxis(0, ui->epochs->value(), 1, 10, QString(u8"轮次"), 1);
    ui->loss->toSetXAxis(0, ui->epochs->value(), 1, 10, QString(u8"轮次"), 1);
    ui->progress->setRange(0, ui->epochs->value() * 1000);
    ui->progress->setValue(0);
    m_pending.clear();
    m_ready = false;
    m_finished = false;
    m_failed = false;
    m_cancelled = false;
    setRunning(true);
    setTaskProgress(QStringLiteral("training"));
    m_startTimer.start(180000);
    m_process->start(python,
                     {QStringLiteral("-u"),
                      script,
                      QStringLiteral("--config"),
                      file.fileName(),
                      QStringLiteral("--run-dir"),
                      m_runDir});
}

void LineTrainForm::stopTraining()
{
    QString error;
    if (!TrainingProtocol::requestCancellation(m_process, &m_startTimer, &m_cancelTimer, &error))
    {
        reportError(error);
        TrainingProtocol::terminateWithGrace(m_process, this);
    }
}

void LineTrainForm::readOutput()
{
    for (const QByteArray &line : TrainingProtocol::takeLines(m_pending, m_process->readAllStandardOutput()))
    {
        handleLine(line);
    }
}

void LineTrainForm::handleLine(const QByteArray &line)
{
    QJsonObject event;
    QString error;
    if (!TrainingProtocol::parseEvent(line, &event, &error) ||
        !TrainingProtocol::validateTrainingEvent(event,
                                                 m_taskName,
                                                 m_runId,
                                                 QStringLiteral("line_detection"),
                                                 &m_ready,
                                                 &error))
    {
        reportError(error);
        TrainingProtocol::terminateWithGrace(m_process, this);
        return;
    }
    const QString type = event.value("type").toString();
    const int epoch = event.value("epoch").toInt();
    if (type == QStringLiteral("batch"))
    {
        const int total = event.value("total_steps").toInt();
        if (epoch < 1 || total < 1)
        {
            reportError(QString(u8"训练批次事件无效"));
            return;
        }
        ui->progress->setValue((epoch - 1) * 1000 + event.value("step").toInt() * 1000 / total);
    }
    else if (type == QStringLiteral("epoch"))
    {
        if (epoch < 1 || !event.value("loss").isDouble() || !event.value("precision").isDouble() ||
            !event.value("recall").isDouble() || !event.value("f1").isDouble())
        {
            reportError(QString(u8"训练轮次事件无效"));
            return;
        }
        m_startTimer.stop();
        ui->metrics->toInsertData(0, epoch, static_cast<float>(event.value("precision").toDouble()));
        ui->metrics->toInsertData(1, epoch, static_cast<float>(event.value("recall").toDouble()));
        ui->metrics->toInsertData(2, epoch, static_cast<float>(event.value("f1").toDouble()));
        ui->loss->toInsertData(0, epoch, static_cast<float>(event.value("loss").toDouble()));
        ui->progress->setValue(epoch * 1000);
    }
    else if (type == QStringLiteral("finished"))
    {
        m_finished = true;
    }
    else if (type == QStringLiteral("cancelled"))
    {
        m_cancelled = true;
    }
    else if (type == QStringLiteral("error"))
    {
        reportError(event.value("message").toString());
    }
    if (type != QStringLiteral("batch") && type != QStringLiteral("heartbeat"))
    {
        ui->log->appendPlainText(QString::fromUtf8(line));
    }
}

void LineTrainForm::finish(int exitCode, QProcess::ExitStatus status)
{
    m_startTimer.stop();
    m_cancelTimer.stop();
    readOutput();
    if (!m_pending.trimmed().isEmpty())
    {
        handleLine(m_pending.trimmed());
        m_pending.clear();
    }
    setRunning(false);
    if (m_cancelled && !m_failed)
    {
        setTaskProgress(QStringLiteral("dataset_ready"));
        return;
    }
    const QString best = QDir(m_runDir).filePath(QStringLiteral("best.pth"));
    if (status != QProcess::NormalExit || exitCode != 0 || m_failed || !m_finished || !QFileInfo(best).isFile())
    {
        reportError(QString(u8"训练失败, 退出码 %1").arg(exitCode));
        setTaskProgress(QStringLiteral("training_failed"));
        return;
    }
    const QString path =
        QDir(TaskRepository::TrainRoot()).filePath(m_taskName + QStringLiteral("/pidinet/latest.json"));
    QSaveFile latest(path);
    const QByteArray bytes = QJsonDocument(QJsonObject{{"model", best}, {"run_id", m_runId}}).toJson();
    if (!latest.open(QIODevice::WriteOnly) || latest.write(bytes) != bytes.size() || !latest.commit())
    {
        reportError(QString(u8"无法记录最佳模型: %1").arg(path));
        setTaskProgress(QStringLiteral("training_failed"));
        return;
    }
    setTaskProgress(QStringLiteral("trained"));
}

void LineTrainForm::reportError(const QString &message)
{
    m_failed = true;
    qWarning().noquote() << message;
    ui->log->appendPlainText(message);
}

void LineTrainForm::setRunning(bool running)
{
    ui->start->setEnabled(!running);
    ui->stop->setEnabled(running);
    ui->epochs->setEnabled(!running);
    ui->rate->setEnabled(!running);
}

void LineTrainForm::setTaskProgress(const QString &progress)
{
    QString error;
    if (!TaskRepository::UpdateProgress(m_taskName, progress, &error))
    {
        reportError(error);
    }
}
