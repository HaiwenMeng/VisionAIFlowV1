#include "lineinferform.h"

#include "BaseAnnoDisplayWidget.h"
#include "TrainForms/trainingprotocol.h"
#include "taskrepository.h"
#include "ytyolodefine.h"
#include "release/ui_lineinferform.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QUuid>
#include <cstring>

LineInferForm::LineInferForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::LineInferForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    ui->canvas->setShowAnnotationLabels(false);
    TrainingProtocol::configurePythonProcess(m_process);
    connect(ui->buttonAdd, &QPushButton::clicked, this, &LineInferForm::addImages);
    connect(ui->buttonClear,
            &QPushButton::clicked,
            this,
            [this]()
            {
                m_results.clear();
                ui->images->clear();
                ui->canvas->clearAnnotations();
                ui->canvas->clearImage();
            });
    connect(ui->run, &QPushButton::clicked, this, &LineInferForm::runInference);
    connect(ui->images, &QListWidget::currentRowChanged, this, &LineInferForm::showImage);
    connect(ui->showMask, &QCheckBox::toggled, this, &LineInferForm::showImage);
    connect(ui->showLines, &QCheckBox::toggled, this, &LineInferForm::showImage);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LineInferForm::readOutput);
    connect(m_process,
            &QProcess::readyReadStandardError,
            this,
            [this]()
            {
                ui->log->appendPlainText(QString::fromUtf8(m_process->readAllStandardError()).trimmed());
            });
    connect(m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError error)
            {
                reportError(QString(u8"推理进程错误: %1").arg(m_process->errorString()));
                if (error == QProcess::FailedToStart)
                {
                    setRunning(false);
                }
            });
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus status)
            {
                readOutput();
                if (!m_pending.trimmed().isEmpty())
                {
                    handleLine(m_pending.trimmed());
                    m_pending.clear();
                }
                setRunning(false);
                if (status != QProcess::NormalExit || exitCode != 0 || !m_finished || m_failed ||
                    m_results.size() != ui->images->count())
                {
                    reportError(QString(u8"推理失败, 退出码 %1").arg(exitCode));
                    return;
                }
                ui->log->appendPlainText(QString(u8"推理完成"));
                showImage();
            });
}

LineInferForm::~LineInferForm()
{
    if (!isIdle())
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

void LineInferForm::initialize(const QString &taskName)
{
    if (m_taskName != taskName)
    {
        ui->images->clear();
        m_results.clear();
        ui->canvas->clearImage();
        ui->canvas->clearAnnotations();
    }
    m_taskName = taskName;
}

bool LineInferForm::isIdle() const
{
    return m_process->state() == QProcess::NotRunning;
}

void LineInferForm::addImages()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this,
                                                            QString(u8"添加图片"),
                                                            QString(),
                                                            QString(u8"图片 (*.jpg *.jpeg *.png *.bmp *.tif *.tiff)"));
    for (const QString &path : paths)
    {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), ui->images);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
    if (ui->images->currentRow() < 0 && ui->images->count() > 0)
    {
        ui->images->setCurrentRow(0);
    }
}

void LineInferForm::runInference()
{
    const QDir pythonRoot(YtYoloDefine::toGetPythonPath());
    const QString python = pythonRoot.filePath(QStringLiteral("python.exe"));
    const QString script = pythonRoot.filePath(QStringLiteral("pidiNet/inference_worker.py"));
    const QString latestPath =
        QDir(TaskRepository::TrainRoot()).filePath(m_taskName + QStringLiteral("/pidinet/latest.json"));
    QFile latest(latestPath);
    if (!latest.open(QIODevice::ReadOnly))
    {
        reportError(QString(u8"无法读取最近训练模型: %1").arg(latestPath));
        return;
    }
    const QString model = QJsonDocument::fromJson(latest.readAll()).object().value("model").toString();
    if (!QFileInfo(python).isFile() || !QFileInfo(script).isFile() || !QFileInfo(model).isFile() ||
        ui->images->count() == 0)
    {
        reportError(QString(u8"请检查 Python、推理脚本、模型和图片: %1, %2, %3").arg(python, script, model));
        return;
    }
    m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runDir = QDir(TaskRepository::ValidationRoot()).filePath(m_taskName + QStringLiteral("/pidinet/") + m_runId);
    if (!QDir().mkpath(m_runDir))
    {
        reportError(QString(u8"无法创建推理目录: %1").arg(m_runDir));
        return;
    }
    QJsonArray sources;
    for (int index = 0; index < ui->images->count(); ++index)
    {
        sources.append(ui->images->item(index)->data(Qt::UserRole).toString());
    }
    const QJsonObject config{{"job_id", m_taskName},
                             {"run_id", m_runId},
                             {"model", model},
                             {"sources", sources},
                             {"confidence", ui->dSpinBox_conf->value()}};
    QSaveFile file(QDir(m_runDir).filePath(QStringLiteral("config.json")));
    const QByteArray bytes = QJsonDocument(config).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        reportError(QString(u8"无法保存推理配置: %1").arg(file.errorString()));
        return;
    }
    m_pending.clear();
    m_results.clear();
    m_ready = false;
    m_finished = false;
    m_failed = false;
    showImage();
    setRunning(true);
    m_process->start(python,
                     {QStringLiteral("-u"),
                      script,
                      QStringLiteral("--config"),
                      file.fileName(),
                      QStringLiteral("--run-dir"),
                      m_runDir});
}

void LineInferForm::readOutput()
{
    for (const QByteArray &line : TrainingProtocol::takeLines(m_pending, m_process->readAllStandardOutput()))
    {
        handleLine(line);
    }
}

void LineInferForm::handleLine(const QByteArray &line)
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
    if (type == QStringLiteral("image"))
    {
        const int index = event.value("index").toInt(-1);
        const QString result = event.value("result").toString();
        if (index < 0 || index >= ui->images->count() || !QFileInfo(result).isFile())
        {
            reportError(QString(u8"推理结果无效: %1").arg(result));
            return;
        }
        m_results.insert(index, result);
        const QString errorMessage = event.value("errorMessage").toString();
        if (!errorMessage.isEmpty())
        {
            reportError(errorMessage);
        }
        if (ui->images->currentRow() == index)
        {
            showImage();
        }
    }
    else if (type == QStringLiteral("finished"))
    {
        m_finished = true;
    }
    else if (type == QStringLiteral("error"))
    {
        reportError(event.value("message").toString());
    }
    if (type != QStringLiteral("heartbeat"))
    {
        ui->log->appendPlainText(QString::fromUtf8(line));
    }
}

void LineInferForm::showImage()
{
    ui->canvas->clearAnnotations();
    const int index = ui->images->currentRow();
    if (index < 0)
    {
        ui->canvas->clearImage();
        return;
    }
    QString error;
    const QString imagePath = ui->images->item(index)->data(Qt::UserRole).toString();
    QImage image = BaseAnnoDisplayWidget::readImage(imagePath, &error);
    if (image.isNull())
    {
        ui->canvas->clearImage();
        reportError(error);
        return;
    }
    ui->canvas->setImage(image);
    if (!m_results.contains(index))
    {
        return;
    }
    QFile file(m_results.value(index));
    if (!file.open(QIODevice::ReadOnly))
    {
        reportError(QString(u8"无法读取推理结果: %1").arg(file.fileName()));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject result = document.object();
    if (parseError.error != QJsonParseError::NoError || !result.value("shapes").isArray() ||
        result.value("imagePath").toString() != imagePath || result.value("imageWidth").toInt() != image.width() ||
        result.value("imageHeight").toInt() != image.height())
    {
        reportError(QString(u8"推理结果无效: %1").arg(file.fileName()));
        return;
    }
    if (ui->showMask->isChecked())
    {
        const QString maskName = result.value("mask").toString();
        const QString maskPath = QFileInfo(file).dir().filePath(maskName);
        const QImage mask = QImage(maskPath).convertToFormat(QImage::Format_Grayscale8);
        if (mask.size() != image.size())
        {
            reportError(QString(u8"模型边缘 mask 缺失或尺寸无效: %1").arg(maskPath));
            return;
        }
        QImage overlay(mask.size(), QImage::Format_Indexed8);
        QList<QRgb> colors(256, qRgba(0, 0, 0, 0));
        colors[255] = qRgba(0, 220, 255, 128);
        overlay.setColorTable(colors);
        for (int row = 0; row < mask.height(); ++row)
        {
            std::memcpy(overlay.scanLine(row), mask.constScanLine(row), static_cast<size_t>(mask.width()));
        }
        image = image.convertToFormat(QImage::Format_RGB32);
        QPainter painter(&image);
        painter.drawImage(0, 0, overlay);
        painter.end();
        ui->canvas->setImage(image);
    }
    if (!ui->showLines->isChecked())
    {
        return;
    }
    BaseAnnoAnnotationList annotations;
    for (const QJsonValue &value : result.value("shapes").toArray())
    {
        const QJsonObject shape = value.toObject();
        const QJsonArray points = shape.value("points").toArray();
        if (shape.value("shape_type").toString() != QStringLiteral("line") || points.size() != 2 ||
            points[0].toArray().size() != 2 || points[1].toArray().size() != 2)
        {
            reportError(QString(u8"推理线段无效: %1").arg(file.fileName()));
            return;
        }
        BaseAnnoAnnotation annotation;
        annotation.label = QStringLiteral("line");
        annotation.colorValue = 0xff0000;
        annotation.shapeType = BaseAnnoShapeType::Line;
        for (const QJsonValue &point : points)
        {
            const QJsonArray xy = point.toArray();
            annotation.pointsImage << QPointF(xy[0].toDouble(), xy[1].toDouble());
        }
        annotations.append(annotation);
    }
    ui->canvas->setAnnotations(annotations);
}

void LineInferForm::reportError(const QString &errorMessage)
{
    m_failed = true;
    qWarning().noquote() << errorMessage;
    ui->log->appendPlainText(errorMessage);
}

void LineInferForm::setRunning(bool running)
{
    ui->buttonAdd->setEnabled(!running);
    ui->buttonClear->setEnabled(!running);
    ui->dSpinBox_conf->setEnabled(!running);
    ui->run->setEnabled(!running);
}
