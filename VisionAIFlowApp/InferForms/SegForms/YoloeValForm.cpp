#include "InferForms/SegForms/YoloeValForm.h"

#include "baseannolegacycanvas.h"
#include "filecopysetdlg.h"
#include "release/ui_YoloeValForm.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>

#include <QStringConverter>

#include <cstring>

namespace
{
constexpr char EventPrefix[] = "YT_YOLOE_EVENT:";

QString normalizedExistingPath(const QString &path)
{
    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    if (!canonicalPath.isEmpty())
    {
        return QDir::cleanPath(canonicalPath);
    }
    return QDir::cleanPath(fileInfo.absoluteFilePath());
}
} // namespace

YoloeValForm::YoloeValForm(QWidget *parent) : QWidget(parent), ui(new Ui::YoloeValForm), m_process(new QProcess(this))
{
    ui->setupUi(this);
    ui->ytRoiShowInferRest->addOverPlayPtr(&m_resultOverlay, QStringLiteral("result"));
    ui->ytRoiShowBackboneRest->addOverPlayPtr(&m_backboneResultOverlay, QStringLiteral("backbone_result"));
    ui->ytRoiShowInferRest->toSetBGColor(QColor(33, 34, 35));
    ui->ytRoiShowBackboneRest->toSetBGColor(QColor(33, 34, 35));
    ui->ytRoiShowVPE->toSetBGColor(QColor(33, 34, 35));
    ui->ytRoiShowVPE->toSetLabelNames({QStringLiteral("VPE")}, {QColor(0, 200, 255)});
    ui->ytRoiShowVPE->toSetRoiDefaltName(QStringLiteral("VPE"));
    ui->ytRoiShowVPE->toSetRoiDefaltType(LabelSet::LrectangleROI);

    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(ui->PB_SelectBackbone, &QPushButton::clicked, this, &YoloeValForm::selectBackbone);
    connect(ui->PB_AddFiles, &QPushButton::clicked, this, &YoloeValForm::addImages);
    connect(ui->PB_Clear, &QPushButton::clicked, this, &YoloeValForm::clearImages);
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &YoloeValForm::selectImage);
    connect(ui->pushButton_setTemlateImg, &QPushButton::clicked, this, &YoloeValForm::selectTemplateFromCurrentImage);
    connect(ui->PB_OpenVPEImg, &QPushButton::clicked, this, &YoloeValForm::selectTemplateFromFile);
    connect(ui->pushButton_GenVPE, &QPushButton::clicked, this, &YoloeValForm::generateVpe);
    connect(ui->pushButton_DeleteVPE, &QPushButton::clicked, this, &YoloeValForm::deleteVpe);
    connect(ui->PB_ProValOne, &QPushButton::clicked, this, &YoloeValForm::inferCurrentImage);
    connect(ui->PB_ProValAll, &QPushButton::clicked, this, &YoloeValForm::inferAllImages);
    connect(ui->PB_ProBackboneOne, &QPushButton::clicked, this, &YoloeValForm::inferBackboneCurrentImage);
    connect(ui->PB_ProBackboneAll, &QPushButton::clicked, this, &YoloeValForm::inferBackboneImages);
    connect(ui->ytRoiShowVPE,
            &BaseAnnoDisplayWidget::annotationCreated,
            this,
            [this](const BaseAnnoAnnotation &)
            {
                invalidateVpe(QString(u8"模板框已改变"));
            });
    connect(ui->ytRoiShowVPE,
            &BaseAnnoDisplayWidget::annotationChanged,
            this,
            [this](int, const BaseAnnoAnnotation &)
            {
                invalidateVpe(QString(u8"模板框已改变"));
            });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &YoloeValForm::processOutput);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &YoloeValForm::processFinished);
    connect(m_process,
            &QProcess::errorOccurred,
            this,
            [this](QProcess::ProcessError processError)
            {
                if (processError == QProcess::FailedToStart)
                {
                    const QString errorMessage =
                        QString(u8"YOLOE Python进程启动失败: %1").arg(m_process->errorString());
                    appendLog(errorMessage);
                    m_errorReported = true;
                }
            });
}

YoloeValForm::~YoloeValForm()
{
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

void YoloeValForm::setTaskName(const QString &taskName)
{
    if (m_taskName == taskName)
    {
        return;
    }

    m_taskName = taskName;
    m_currentImagePath.clear();
    m_templateImagePath.clear();
    m_templateImage = QImage();
    m_generatedBackbonePath.clear();
    m_generatedTemplatePath.clear();
    m_generatedPromptSignature.clear();
    ui->LE_Backbone->clear();
    m_resultLabels.toClearData();
    m_resultOverlay.toClearData();
    m_backboneResultLabels.toClearData();
    m_backboneResultOverlay.toClearData();
    ui->ytRoiShowVPE->clearImage();
    ui->ytRoiShowInferRest->clearImage();
    ui->ytRoiShowBackboneRest->clearImage();
}

void YoloeValForm::initialize()
{
    if (m_taskName.isEmpty())
    {
        showError(QString(u8"YOLOE推理初始化失败"), QString(u8"任务名称为空"));
        return;
    }

    const QString taskValuePath = QDir(YtYoloDefine::toGetValuePath()).filePath(m_taskName);
    if (!QDir().mkpath(taskValuePath))
    {
        showError(QString(u8"YOLOE推理初始化失败"), QString(u8"无法创建任务推理目录: %1").arg(taskValuePath));
        return;
    }

    m_labelConfig.toLoadData(QDir(YtYoloDefine::toGetLabelPath()).filePath(m_taskName));
    loadImageList();

    const QString defaultModelPath = QDir(YtYoloDefine::toGetTrainPath())
                                         .filePath(m_taskName + QStringLiteral("/yoloe/vp_seg_stage/weights/best.pt"));
    if (ui->LE_Backbone->text().trimmed().isEmpty())
    {
        ui->LE_Backbone->setText(defaultModelPath);
    }

    if (!m_templateImage.isNull())
    {
        ui->ytRoiShowVPE->toSetImage(m_templateImage);
    }

    if (ui->listWidget->currentItem() == nullptr && ui->listWidget->count() > 0)
    {
        ui->listWidget->setCurrentRow(0);
    }
}

void YoloeValForm::appendLog(const QString &message)
{
    ui->plainTextEdit_Log->append(message);
    qInfo().noquote() << message;
}

void YoloeValForm::showError(const QString &title, const QString &errorMessage)
{
    appendLog(errorMessage);
    QMessageBox::critical(this, title, errorMessage);
}

void YoloeValForm::setBusy(const bool busy)
{
    ui->PB_SelectBackbone->setEnabled(!busy);
    ui->PB_AddFiles->setEnabled(!busy);
    ui->PB_Clear->setEnabled(!busy);
    ui->listWidget->setEnabled(!busy);
    ui->pushButton_setTemlateImg->setEnabled(!busy);
    ui->PB_OpenVPEImg->setEnabled(!busy);
    ui->pushButton_GenVPE->setEnabled(!busy);
    ui->pushButton_DeleteVPE->setEnabled(!busy);
    ui->PB_ProValOne->setEnabled(!busy);
    ui->PB_ProValAll->setEnabled(!busy);
    ui->LE_Backbone->setEnabled(!busy);
    ui->DSB_Confidence->setEnabled(!busy);
    ui->DSB_Iou->setEnabled(!busy);
    ui->spinBox_MaxDetNum->setEnabled(!busy);
    ui->LE_BackbonePrompt->setEnabled(!busy);
    ui->PB_ProBackboneOne->setEnabled(!busy);
    ui->PB_ProBackboneAll->setEnabled(!busy);
    ui->ytRoiShowVPE->setAnnotationDrawingEnabled(!busy);
}

bool YoloeValForm::validatePythonRuntime(QString *pythonPath, QString *scriptPath, QString *errorMessage) const
{
    const QString runtimeDirectory = YtYoloDefine::toGetPythonPath().trimmed();
    if (runtimeDirectory.isEmpty())
    {
        *errorMessage = QString(u8"未配置Python环境路径, 请在系统设置中设置LE_PythonPath");
        return false;
    }

    *pythonPath = QDir(runtimeDirectory).filePath(QStringLiteral("python.exe"));
    *scriptPath = QDir(runtimeDirectory).filePath(QStringLiteral("YOLOE/infer_project_yoloe_seg.py"));
    if (!QFileInfo(*pythonPath).isFile())
    {
        *errorMessage = QString(u8"Python解释器不存在: %1").arg(*pythonPath);
        return false;
    }
    if (!QFileInfo(*scriptPath).isFile())
    {
        *errorMessage = QString(u8"YOLOE推理脚本不存在: %1").arg(*scriptPath);
        return false;
    }
    return true;
}

bool YoloeValForm::validateBackbone(QString *modelPath, QString *errorMessage) const
{
    *modelPath = currentBackbonePath();
    if (modelPath->isEmpty() || !QFileInfo(*modelPath).isFile())
    {
        *errorMessage = QString(u8"YOLOE backbone不存在: %1").arg(ui->LE_Backbone->text().trimmed());
        return false;
    }
    return true;
}

void YoloeValForm::saveImageList() const
{
    QFile file(imageListPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        qCritical().noquote() << QString(u8"无法保存YOLOE推理图像列表: %1").arg(file.fileName());
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (int index = 0; index < ui->listWidget->count(); ++index)
    {
        stream << ui->listWidget->item(index)->text() << "\n";
    }
}

bool YoloeValForm::prepareInferenceSourceList(const bool singleImage,
                                              QString *sourceListPath,
                                              QString *errorMessage) const
{
    QStringList imagePaths;
    if (singleImage)
    {
        if (ui->listWidget->currentItem() == nullptr)
        {
            *errorMessage = QString(u8"请先在图像列表中选择一张图片");
            return false;
        }
        imagePaths.append(ui->listWidget->currentItem()->text());
        *sourceListPath = singleImageListPath();
    }
    else
    {
        for (int index = 0; index < ui->listWidget->count(); ++index)
        {
            imagePaths.append(ui->listWidget->item(index)->text());
        }
        *sourceListPath = imageListPath();
    }

    QFile file(*sourceListPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        *errorMessage = QString(u8"无法保存YOLOE推理图像列表: %1").arg(*sourceListPath);
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const QString &imagePath : imagePaths)
    {
        stream << imagePath << "\n";
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok)
    {
        *errorMessage = QString(u8"写入YOLOE推理图像列表失败: %1").arg(*sourceListPath);
        return false;
    }
    return true;
}

void YoloeValForm::loadImageList()
{
    ui->listWidget->clear();
    QFile file(imageListPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    while (!stream.atEnd())
    {
        const QString imagePath = stream.readLine().trimmed();
        if (!imagePath.isEmpty())
        {
            ui->listWidget->addItem(imagePath);
        }
    }
}

QString YoloeValForm::imageListPath() const
{
    return QDir(YtYoloDefine::toGetValuePath()).filePath(m_taskName + QStringLiteral("/ProFile.csv"));
}

QString YoloeValForm::singleImageListPath() const
{
    return QDir(YtYoloDefine::toGetValuePath()).filePath(m_taskName + QStringLiteral("/SingleProFile.csv"));
}

QString YoloeValForm::vpePath() const
{
    return QDir(YtYoloDefine::toGetValuePath()).filePath(m_taskName + QStringLiteral("/yoloe_vpe.pt"));
}

QString YoloeValForm::backboneResultDirectory() const
{
    return QDir(YtYoloDefine::toGetValuePath()).filePath(m_taskName + QStringLiteral("/backbone_results"));
}

QString YoloeValForm::backboneResultPath(const QString &imagePath) const
{
    return QDir(backboneResultDirectory()).filePath(QFileInfo(imagePath).completeBaseName() + QStringLiteral(".txt"));
}

void YoloeValForm::setTemplateImage(const QString &imagePath)
{
    QImage image;
    if (!image.load(imagePath))
    {
        showError(QString(u8"设置VPE模板失败"), QString(u8"无法读取模板图像: %1").arg(imagePath));
        return;
    }

    m_templateImagePath = normalizedExistingPath(imagePath);
    m_templateImage = image;
    ui->ytRoiShowVPE->toSetImage(m_templateImage);
    ui->ytRoiShowVPE->toRemoveAllRoi();
    invalidateVpe(QString(u8"模板图已改变"));
    appendLog(QString(u8"已设置VPE模板图: %1").arg(m_templateImagePath));
}

QVector<QRectF> YoloeValForm::promptRects() const
{
    QVector<QRectF> rects;
    for (const BaseAnnoAnnotation &annotation : ui->ytRoiShowVPE->annotations())
    {
        if (annotation.shapeType != BaseAnnoShapeType::Rectangle || annotation.pointsImage.size() < 2)
        {
            continue;
        }

        const QRectF rect = annotation.pointsImage.boundingRect().normalized();
        if (rect.width() >= 1.0 && rect.height() >= 1.0)
        {
            rects.append(rect);
        }
    }
    return rects;
}

QString YoloeValForm::promptSignature() const
{
    QJsonArray rectangles;
    for (const QRectF &rect : promptRects())
    {
        QJsonArray rectangle;
        rectangle.append(rect.left());
        rectangle.append(rect.top());
        rectangle.append(rect.right());
        rectangle.append(rect.bottom());
        rectangles.append(rectangle);
    }
    return QString::fromUtf8(QJsonDocument(rectangles).toJson(QJsonDocument::Compact));
}

QString YoloeValForm::currentBackbonePath() const
{
    const QString modelPath = ui->LE_Backbone->text().trimmed();
    if (modelPath.isEmpty())
    {
        return {};
    }
    return normalizedExistingPath(modelPath);
}

bool YoloeValForm::hasCurrentVpe(QString *errorMessage) const
{
    if (!QFileInfo(vpePath()).isFile())
    {
        *errorMessage = QString(u8"VPE模板文件不存在, 请先生成VPE");
        return false;
    }
    if (m_generatedBackbonePath.isEmpty() || m_generatedTemplatePath.isEmpty() || m_generatedPromptSignature.isEmpty())
    {
        *errorMessage = QString(u8"当前页面未生成VPE, 请重新生成VPE");
        return false;
    }
    if (m_generatedBackbonePath != currentBackbonePath())
    {
        *errorMessage = QString(u8"backbone已改变, 请重新生成VPE");
        return false;
    }
    if (m_generatedTemplatePath != m_templateImagePath)
    {
        *errorMessage = QString(u8"模板图已改变, 请重新生成VPE");
        return false;
    }
    if (m_generatedPromptSignature != promptSignature())
    {
        *errorMessage = QString(u8"模板框已改变, 请重新生成VPE");
        return false;
    }
    return true;
}

void YoloeValForm::invalidateVpe(const QString &reason)
{
    if (m_generatedBackbonePath.isEmpty())
    {
        return;
    }
    m_generatedBackbonePath.clear();
    m_generatedTemplatePath.clear();
    m_generatedPromptSignature.clear();
    appendLog(QString(u8"VPE已失效: %1, 请重新生成VPE").arg(reason));
}

void YoloeValForm::refreshResultCanvas()
{
    if (m_currentImagePath.isEmpty())
    {
        return;
    }

    QImage image;
    if (!image.load(m_currentImagePath))
    {
        appendLog(QString(u8"无法读取待推理图像: %1").arg(m_currentImagePath));
        return;
    }

    ui->ytRoiShowInferRest->toSetImage(image);
    m_resultLabels.toLoadSegTxt(m_currentImagePath, image.size(), m_labelConfig.m_NameList);
    m_resultOverlay.toClearData();
    m_resultLabels.toGenSegOveplay(m_resultOverlay, m_labelConfig.m_NameList, m_labelConfig.m_ClorDefine);
    ui->ytRoiShowInferRest->toUpdateShow();
}

void YoloeValForm::refreshBackboneResultCanvas()
{
    if (m_currentImagePath.isEmpty())
    {
        return;
    }

    QImage image;
    if (!image.load(m_currentImagePath))
    {
        appendLog(QString(u8"无法读取Backbone待推理图像: %1").arg(m_currentImagePath));
        return;
    }

    ui->ytRoiShowBackboneRest->toSetImage(image);
    m_backboneResultLabels.toLoadSegTxtFile(backboneResultPath(m_currentImagePath),
                                            QFileInfo(m_currentImagePath).fileName(),
                                            image.size(),
                                            m_labelConfig.m_NameList);
    m_backboneResultOverlay.toClearData();
    m_backboneResultLabels.toGenSegOveplay(m_backboneResultOverlay,
                                           m_labelConfig.m_NameList,
                                           m_labelConfig.m_ClorDefine);
    ui->ytRoiShowBackboneRest->toUpdateShow();
}

void YoloeValForm::startPythonOperation(const RunningOperation operation, const QStringList &arguments)
{
    QString pythonPath;
    QString scriptPath;
    QString errorMessage;
    if (!validatePythonRuntime(&pythonPath, &scriptPath, &errorMessage))
    {
        showError(QString(u8"YOLOE推理失败"), errorMessage);
        return;
    }

    QStringList processArguments;
    processArguments.append(scriptPath);
    processArguments.append(arguments);
    m_runningOperation = operation;
    m_processOutputBuffer.clear();
    m_errorReported = false;
    setBusy(true);
    m_process->start(pythonPath, processArguments);
}

void YoloeValForm::processOutput()
{
    m_processOutputBuffer.append(QString::fromUtf8(m_process->readAllStandardOutput()));
    while (true)
    {
        const int lineEnd = m_processOutputBuffer.indexOf('\n');
        if (lineEnd < 0)
        {
            break;
        }
        const QString line = m_processOutputBuffer.left(lineEnd).trimmed();
        m_processOutputBuffer.remove(0, lineEnd + 1);
        processOutputLine(line);
    }
}

void YoloeValForm::processOutputLine(const QString &line)
{
    if (line.isEmpty())
    {
        return;
    }

    if (!line.startsWith(QString::fromLatin1(EventPrefix)))
    {
        appendLog(line);
        return;
    }

    const QByteArray eventJson = line.mid(static_cast<int>(strlen(EventPrefix))).toUtf8();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(eventJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        appendLog(QString(u8"YOLOE事件解析失败: %1").arg(parseError.errorString()));
        return;
    }

    const QJsonObject event = document.object();
    const QString eventType = event.value(QStringLiteral("type")).toString();
    const QString message = event.value(QStringLiteral("message")).toString();
    if (eventType == QStringLiteral("error"))
    {
        m_errorReported = true;
        showError(QString(u8"YOLOE推理失败"), QString(u8"YOLOE错误: %1").arg(message));
        return;
    }
    if (eventType == QStringLiteral("image_complete"))
    {
        const int maskCount = event.value(QStringLiteral("mask_count")).toInt();
        QString confidenceText = QString(u8"无目标");
        if (event.value(QStringLiteral("min_confidence")).isDouble() &&
            event.value(QStringLiteral("max_confidence")).isDouble())
        {
            confidenceText = QString(u8"置信度 %1 - %2")
                                 .arg(event.value(QStringLiteral("min_confidence")).toDouble(), 0, 'f', 3)
                                 .arg(event.value(QStringLiteral("max_confidence")).toDouble(), 0, 'f', 3);
        }
        appendLog(QString(u8"已完成: %1, Mask %2, %3")
                      .arg(event.value(QStringLiteral("image")).toString())
                      .arg(maskCount)
                      .arg(confidenceText));
        return;
    }
    if (!message.isEmpty())
    {
        appendLog(message);
    }
}

void YoloeValForm::processFinished(const int exitCode, const QProcess::ExitStatus exitStatus)
{
    processOutput();
    if (!m_processOutputBuffer.trimmed().isEmpty())
    {
        processOutputLine(m_processOutputBuffer.trimmed());
    }
    m_processOutputBuffer.clear();

    const RunningOperation operation = m_runningOperation;
    m_runningOperation = RunningOperation::None;
    setBusy(false);

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
        const QString errorMessage =
            QString(u8"YOLOE进程异常结束, 退出码 %1, %2").arg(exitCode).arg(m_process->errorString());
        if (!m_errorReported)
        {
            showError(QString(u8"YOLOE推理失败"), errorMessage);
        }
        else
        {
            appendLog(errorMessage);
        }
        return;
    }

    if (operation == RunningOperation::GenerateVpe)
    {
        const QFileInfo generatedVpe(vpePath());
        if (!generatedVpe.isFile() || generatedVpe.size() <= 0)
        {
            showError(QString(u8"生成VPE失败"), QString(u8"Python未生成有效VPE文件: %1").arg(vpePath()));
            return;
        }
        m_generatedBackbonePath = currentBackbonePath();
        m_generatedTemplatePath = m_templateImagePath;
        m_generatedPromptSignature = promptSignature();
        appendLog(QString(u8"VPE生成完成: %1").arg(vpePath()));
        return;
    }

    if (operation == RunningOperation::InferSingle || operation == RunningOperation::InferBatch)
    {
        refreshResultCanvas();
        appendLog(operation == RunningOperation::InferSingle ? QString(u8"YOLOE VPE单图推理完成")
                                                             : QString(u8"YOLOE VPE批量推理完成"));
        return;
    }
    if (operation == RunningOperation::InferTextSingle || operation == RunningOperation::InferTextBatch)
    {
        refreshBackboneResultCanvas();
        appendLog(operation == RunningOperation::InferTextSingle ? QString(u8"YOLOE Backbone单图推理完成")
                                                                 : QString(u8"YOLOE Backbone批量推理完成"));
    }
}

void YoloeValForm::selectBackbone()
{
    const QString modelPath = QFileDialog::getOpenFileName(this,
                                                           QString(u8"选择YOLOE Backbone"),
                                                           ui->LE_Backbone->text().trimmed(),
                                                           QString(u8"PyTorch模型 (*.pt)"));
    if (modelPath.isEmpty())
    {
        return;
    }
    ui->LE_Backbone->setText(modelPath);
    invalidateVpe(QString(u8"backbone已改变"));
}

void YoloeValForm::addImages()
{
    FileCopySetDlg dialog(this);
    dialog.setWindowFlags(Qt::SubWindow);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    ui->listWidget->addItems(dialog.toGetFileList());
    saveImageList();
    if (ui->listWidget->currentItem() == nullptr && ui->listWidget->count() > 0)
    {
        ui->listWidget->setCurrentRow(0);
    }
}

void YoloeValForm::clearImages()
{
    ui->listWidget->clear();
    m_currentImagePath.clear();
    m_resultLabels.toClearData();
    m_resultOverlay.toClearData();
    ui->ytRoiShowInferRest->clearImage();
    saveImageList();
}

void YoloeValForm::selectImage()
{
    if (ui->listWidget->currentItem() == nullptr)
    {
        return;
    }

    m_currentImagePath = ui->listWidget->currentItem()->text();
    refreshResultCanvas();
    refreshBackboneResultCanvas();
}

void YoloeValForm::selectTemplateFromCurrentImage()
{
    if (ui->listWidget->currentItem() == nullptr)
    {
        showError(QString(u8"设置VPE模板失败"), QString(u8"请先在图像列表中选择模板图"));
        return;
    }
    setTemplateImage(ui->listWidget->currentItem()->text());
}

void YoloeValForm::selectTemplateFromFile()
{
    const QString imagePath =
        QFileDialog::getOpenFileName(this,
                                     QString(u8"选择VPE模板图"),
                                     m_templateImagePath,
                                     QString(u8"图像文件 (*.bmp *.jpg *.jpeg *.png *.tif *.tiff)"));
    if (imagePath.isEmpty())
    {
        return;
    }
    setTemplateImage(imagePath);
}

void YoloeValForm::generateVpe()
{
    if (m_process->state() != QProcess::NotRunning)
    {
        showError(QString(u8"生成VPE失败"), QString(u8"YOLOE进程正在运行"));
        return;
    }

    QString modelPath;
    QString errorMessage;
    if (!validateBackbone(&modelPath, &errorMessage))
    {
        showError(QString(u8"生成VPE失败"), errorMessage);
        return;
    }
    if (m_templateImagePath.isEmpty() || !QFileInfo(m_templateImagePath).isFile())
    {
        showError(QString(u8"生成VPE失败"), QString(u8"请先选择有效的VPE模板图"));
        return;
    }

    const QVector<QRectF> rectangles = promptRects();
    if (rectangles.isEmpty())
    {
        showError(QString(u8"生成VPE失败"), QString(u8"请在模板图上至少绘制一个有效矩形框"));
        return;
    }

    QJsonArray rectangleArray;
    for (const QRectF &rect : rectangles)
    {
        QJsonArray rectangle;
        rectangle.append(rect.left());
        rectangle.append(rect.top());
        rectangle.append(rect.right());
        rectangle.append(rect.bottom());
        rectangleArray.append(rectangle);
    }

    QString pythonPath;
    QString scriptPath;
    if (!validatePythonRuntime(&pythonPath, &scriptPath, &errorMessage))
    {
        showError(QString(u8"生成VPE失败"), errorMessage);
        return;
    }

    const QStringList arguments{QStringLiteral("generate-vpe"),
                                QStringLiteral("--model"),
                                modelPath,
                                QStringLiteral("--template-image"),
                                m_templateImagePath,
                                QStringLiteral("--boxes-json"),
                                QString::fromUtf8(QJsonDocument(rectangleArray).toJson(QJsonDocument::Compact)),
                                QStringLiteral("--class-name"),
                                QStringLiteral("class_0"),
                                QStringLiteral("--output-vpe"),
                                vpePath(),
                                QStringLiteral("--imgsz"),
                                QStringLiteral("1024")};
    appendLog(QString(u8"开始生成YOLOE VPE"));
    startPythonOperation(RunningOperation::GenerateVpe, arguments);
}

void YoloeValForm::deleteVpe()
{
    const QString filePath = vpePath();
    if (QFileInfo::exists(filePath) && !QFile::remove(filePath))
    {
        showError(QString(u8"清除VPE失败"), QString(u8"无法删除VPE文件: %1").arg(filePath));
        return;
    }
    m_generatedBackbonePath.clear();
    m_generatedTemplatePath.clear();
    m_generatedPromptSignature.clear();
    appendLog(QString(u8"已清除VPE模板"));
}

void YoloeValForm::inferAllImages()
{
    runVpeInference(false);
}

void YoloeValForm::inferCurrentImage()
{
    runVpeInference(true);
}

void YoloeValForm::runVpeInference(const bool singleImage)
{
    if (m_process->state() != QProcess::NotRunning)
    {
        showError(QString(u8"YOLOE推理失败"), QString(u8"YOLOE进程正在运行"));
        return;
    }
    if (ui->listWidget->count() == 0)
    {
        showError(QString(u8"YOLOE推理失败"), QString(u8"请先添加待推理图像"));
        return;
    }

    QString modelPath;
    QString errorMessage;
    if (!validateBackbone(&modelPath, &errorMessage))
    {
        showError(QString(u8"YOLOE推理失败"), errorMessage);
        return;
    }
    if (!hasCurrentVpe(&errorMessage))
    {
        showError(QString(u8"YOLOE推理失败"), errorMessage);
        return;
    }

    QString pythonPath;
    QString scriptPath;
    if (!validatePythonRuntime(&pythonPath, &scriptPath, &errorMessage))
    {
        showError(QString(u8"YOLOE推理失败"), errorMessage);
        return;
    }

    QString sourceListPath;
    if (!prepareInferenceSourceList(singleImage, &sourceListPath, &errorMessage))
    {
        showError(QString(u8"YOLOE推理失败"), errorMessage);
        return;
    }
    const QStringList arguments{QStringLiteral("infer-batch"),
                                QStringLiteral("--model"),
                                modelPath,
                                QStringLiteral("--vpe"),
                                vpePath(),
                                QStringLiteral("--sources-file"),
                                sourceListPath,
                                QStringLiteral("--conf"),
                                QString::number(ui->DSB_Confidence->value(), 'f', 3),
                                QStringLiteral("--iou"),
                                QString::number(ui->DSB_Iou->value(), 'f', 3),
                                QStringLiteral("--max-det"),
                                QString::number(ui->spinBox_MaxDetNum->value()),
                                QStringLiteral("--imgsz"),
                                QStringLiteral("1024")};
    appendLog(singleImage ? QString(u8"启动YOLOE VPE单图推理: %1").arg(ui->listWidget->currentItem()->text())
                          : QString(u8"启动YOLOE VPE批量推理"));
    startPythonOperation(singleImage ? RunningOperation::InferSingle : RunningOperation::InferBatch, arguments);
}

void YoloeValForm::inferBackboneImages()
{
    runBackboneInference(false);
}

void YoloeValForm::inferBackboneCurrentImage()
{
    runBackboneInference(true);
}

void YoloeValForm::runBackboneInference(const bool singleImage)
{
    if (m_process->state() != QProcess::NotRunning)
    {
        showError(QString(u8"Backbone推理失败"), QString(u8"YOLOE进程正在运行"));
        return;
    }
    if (ui->listWidget->count() == 0)
    {
        showError(QString(u8"Backbone推理失败"), QString(u8"请先添加待推理图像"));
        return;
    }

    QString modelPath;
    QString errorMessage;
    if (!validateBackbone(&modelPath, &errorMessage))
    {
        showError(QString(u8"Backbone推理失败"), errorMessage);
        return;
    }
    const QString className = ui->LE_BackbonePrompt->text().trimmed();
    if (className.isEmpty())
    {
        showError(QString(u8"Backbone推理失败"), QString(u8"请输入非空的前景提示词"));
        return;
    }

    QString pythonPath;
    QString scriptPath;
    if (!validatePythonRuntime(&pythonPath, &scriptPath, &errorMessage))
    {
        showError(QString(u8"Backbone推理失败"), errorMessage);
        return;
    }

    QString sourceListPath;
    if (!prepareInferenceSourceList(singleImage, &sourceListPath, &errorMessage))
    {
        showError(QString(u8"Backbone推理失败"), errorMessage);
        return;
    }
    const QStringList arguments{QStringLiteral("infer-text-batch"),
                                QStringLiteral("--model"),
                                modelPath,
                                QStringLiteral("--sources-file"),
                                sourceListPath,
                                QStringLiteral("--output-dir"),
                                backboneResultDirectory(),
                                QStringLiteral("--class-name"),
                                className,
                                QStringLiteral("--conf"),
                                QString::number(ui->DSB_Confidence->value(), 'f', 3),
                                QStringLiteral("--iou"),
                                QString::number(ui->DSB_Iou->value(), 'f', 3),
                                QStringLiteral("--max-det"),
                                QString::number(ui->spinBox_MaxDetNum->value()),
                                QStringLiteral("--imgsz"),
                                QStringLiteral("1024")};
    appendLog(singleImage ? QString(u8"启动YOLOE Backbone文本提示单图推理: %1, %2")
                                .arg(className, ui->listWidget->currentItem()->text())
                          : QString(u8"启动YOLOE Backbone文本提示批量推理: %1").arg(className));
    startPythonOperation(singleImage ? RunningOperation::InferTextSingle : RunningOperation::InferTextBatch, arguments);
}
