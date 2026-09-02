#include "traindataform.h"

#include "datasetform.h"
#include "detecttrainingcontroller.h"
#include "release/ui_traindataform.h"
#include "taskrepository.h"
#include "ytyolodefine.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QUrl>

#include <array>

namespace
{
constexpr int kNativeInputSize = 640;
constexpr int kNativeChannels = 3;
constexpr double kNativeLearningRate = 0.001;
constexpr int kPluginPathRole = Qt::UserRole;
constexpr int kPluginNameRole = Qt::UserRole + 1;
constexpr int kPluginExportRole = Qt::UserRole + 2;
constexpr int kPluginIdRole = Qt::UserRole + 3;

QString ModelVariantForIndex(const QString &pluginId, const int index)
{
    if (pluginId == QStringLiteral("visionaiflow.detection.yolov11"))
    {
        return index == 0 ? QStringLiteral("yolo11n") : QString();
    }
    static const std::array<QString, 5> variants{QStringLiteral("yolov8n"),
                                                 QStringLiteral("yolov8s"),
                                                 QStringLiteral("yolov8m"),
                                                 QStringLiteral("yolov8l"),
                                                 QStringLiteral("yolov8x")};
    return index >= 0 && index < static_cast<int>(variants.size()) ? variants.at(index) : QString();
}
} // namespace

TrainDataForm::TrainDataForm(QWidget *parent)
    : QWidget(parent), ui(new Ui::TrainDataForm), m_trainingController(new DetectTrainingController(this))
{
    ui->setupUi(this);
    ui->plainTextEdit_Log->document()->setMaximumBlockCount(300);
    ui->TW_LabeSet->verticalHeader()->setVisible(false);
    ui->TW_LabeSet->horizontalHeader()->setStretchLastSection(true);
    ui->TW_LabeSet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->TW_LabeSet->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->TW_LabeSet->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->TW_LabeSet->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->SB_ImSize->setValue(kNativeInputSize);
    ui->SB_ImSize->setEnabled(false);
    ui->CB_ModeSize->setCurrentIndex(0);
    ui->CB_BachSize->setCurrentText(QStringLiteral("4"));
    ui->SB_epoch->setValue(100);
    ui->CB_MultiScal->setChecked(false);
    ui->CB_MultiScal->setEnabled(false);
    ui->CB_Int8->setChecked(false);
    ui->CB_Int8->setEnabled(false);
    ui->CB_Mocisk->setChecked(false);
    ui->CB_Mocisk->setEnabled(false);
    ui->CB_Works->setEnabled(false);
    ui->CB_Imagechange->setCurrentText(QString::number(kNativeChannels));
    ui->CB_Imagechange->setEnabled(false);
    ui->PB_StopRun->setEnabled(false);
    ui->PB_ModeCopy->setEnabled(false);
    ui->PB_OnnxOut->setEnabled(false);
    InitChart();

    connect(ui->LE_ModeList,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](const int)
            {
                UpdatePluginUiState();
            });

    connect(m_trainingController,
            &DetectTrainingController::EpochProgress,
            this,
            [this](int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double meanIou)
            {
                ui->ChartWidget->toInsertData(0, epoch, static_cast<float>(meanIou));
                ui->ChartWidget->toInsertData(1, epoch, static_cast<float>(loss));
                AppendLog(QString(u8"轮次: %1, 步数: %2, 损失: %3, 框损失: %4, 分类损失: %5, 正样本: %6, IoU: %7")
                              .arg(epoch)
                              .arg(step)
                              .arg(loss, 0, 'f', 6)
                              .arg(boxLoss, 0, 'f', 6)
                              .arg(classLoss, 0, 'f', 6)
                              .arg(positives)
                              .arg(meanIou, 0, 'f', 6));
            });
    connect(
        m_trainingController,
        &DetectTrainingController::Completed,
        this,
        [this](const QString &runDirectory, const QString &modelPath, const QString &bestCheckpointPath, const QString &durationMessage)
        {
            m_lastModelPath = modelPath;
            m_lastBestCheckpointPath = bestCheckpointPath;
            AppendLog(QString(u8"训练完成。输出目录: %1").arg(runDirectory));
            AppendLog(QString(u8"最新 checkpoint: %1").arg(modelPath));
            AppendLog(QString(u8"最佳检查点: %1").arg(bestCheckpointPath));
            AppendLog(durationMessage);
            UpdateTaskProgress(QStringLiteral("trained"));
            QMessageBox::information(this, QString(u8"训练完成"), QString(u8"%1 训练已完成").arg(SelectedPluginName()));
        });
    connect(m_trainingController,
            &DetectTrainingController::Failed,
            this,
            [this](const QString &errorMessage)
            {
                AppendLog(QString(u8"训练失败: %1").arg(errorMessage));
                UpdateTaskProgress(QStringLiteral("training_failed"));
                QMessageBox::critical(this, QString(u8"训练失败"), errorMessage);
            });
    connect(m_trainingController,
            &DetectTrainingController::Cancelled,
            this,
            [this]()
            {
                AppendLog(QString(u8"训练已取消"));
                UpdateTaskProgress(QStringLiteral("dataset_ready"));
            });
    connect(m_trainingController, &DetectTrainingController::StateChanged, this, &TrainDataForm::SetTrainingUiState);
}

TrainDataForm::~TrainDataForm()
{
    delete ui;
}

void TrainDataForm::toSetProcessName(const QString &setName)
{
    m_ProcessName = setName;
}

void TrainDataForm::toInitShow()
{
    RefreshPluginList();
    if (m_DataSetForm == nullptr)
    {
        AppendLog(QString(u8"训练页面缺少数据集页面实例"));
        return;
    }

    ui->TW_LabeSet->setRowCount(m_DataSetForm->m_YtYoloSetPro.m_NameList.size());
    for (int index = 0; index < m_DataSetForm->m_YtYoloSetPro.m_NameList.size(); ++index)
    {
        ui->TW_LabeSet->setItem(index, 0, new QTableWidgetItem(QString::number(index + 1)));
        ui->TW_LabeSet->setItem(index, 1, new QTableWidgetItem(m_DataSetForm->m_YtYoloSetPro.m_NameList.at(index)));
        const int trainCount = index < m_DataSetForm->m_TrainLabelSetfilename.size()
                                   ? m_DataSetForm->m_TrainLabelSetfilename.at(index).size()
                                   : 0;
        const int validationCount = index < m_DataSetForm->m_ValLabelSetfilename.size()
                                        ? m_DataSetForm->m_ValLabelSetfilename.at(index).size()
                                        : 0;
        const int backgroundCount = index < m_DataSetForm->m_BackGroundJsonfilename.size()
                                        ? m_DataSetForm->m_BackGroundJsonfilename.at(index).size()
                                        : 0;
        ui->TW_LabeSet->setItem(index, 2, new QTableWidgetItem(QString::number(trainCount)));
        ui->TW_LabeSet->setItem(index, 3, new QTableWidgetItem(QString::number(validationCount)));
        ui->TW_LabeSet->setItem(index, 4, new QTableWidgetItem(QString::number(backgroundCount)));
        for (int column = 0; column < ui->TW_LabeSet->columnCount(); ++column)
        {
            ui->TW_LabeSet->item(index, column)->setTextAlignment(Qt::AlignCenter);
        }
    }

    ui->LE_ModeType->setText(m_DataSetForm->toGetRunType());
    ui->SB_ImSize->setValue(kNativeInputSize);
    ui->CB_Imagechange->setCurrentText(QString::number(kNativeChannels));
    ui->CB_ModeSize->setCurrentIndex(0);
    AppendLog(QString(u8"检测训练输入固定为 640，通道固定为 3，学习率固定为 0.001。"));
}

bool TrainDataForm::isrunstate() const
{
    return !m_trainingController->IsRunning();
}

void TrainDataForm::on_PB_RunTrain_clicked()
{
    if (m_ProcessName.isEmpty())
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"未选择任务"));
        return;
    }
    if (m_DataSetForm == nullptr || m_DataSetForm->toGetRunIndex() != 0)
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"当前训练页面仅支持 Detect 数据集"));
        return;
    }
    const QString pluginPath = SelectedPluginPath();
    if (pluginPath.isEmpty())
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"请选择有效的检测训练插件"));
        return;
    }
    const QString modelVariant = ModelVariantForIndex(SelectedPluginId(), ui->CB_ModeSize->currentIndex());
    if (modelVariant.isEmpty())
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"当前模型规格不受所选训练插件支持"));
        return;
    }

    const QString csvPath = QDir(YtYoloDefine::toGetDataPath()).filePath(m_ProcessName + QStringLiteral("/train.csv"));
    if (!QFileInfo::exists(csvPath))
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"未找到已生成的数据集: %1").arg(csvPath));
        return;
    }

    ui->ChartWidget->removeAllPoints();
    ui->ChartWidget->toSetXAxis(1, ui->SB_epoch->value(), 1, 10, QString(u8"轮次"), 1);
    DetectTrainingRequest request;
    request.taskName = m_ProcessName;
    request.pluginPath = pluginPath;
    request.modelVariant = modelVariant;
    request.epochs = ui->SB_epoch->value();
    request.batchSize = ui->CB_BachSize->currentText().toInt();
    request.learningRate = kNativeLearningRate;
    request.horizontalFlip = false;
    AppendLog(QString(u8"开始 %1 %2 训练").arg(SelectedPluginName(), modelVariant));

    UpdateTaskProgress(QStringLiteral("training"));
    m_trainingController->Start(request);
}

void TrainDataForm::on_PB_StopRun_clicked()
{
    if (!m_trainingController->IsRunning())
    {
        AppendLog(QString(u8"没有正在运行的训练任务"));
        return;
    }
    AppendLog(QString(u8"已请求停止训练，当前批次结束后停止"));
    m_trainingController->Cancel();
}

void TrainDataForm::on_PB_ViewPos_clicked()
{
    const QString directory = WeightsDirectory();
    if (!QFileInfo(directory).isDir())
    {
        QMessageBox::critical(this, QString(u8"打开失败"), QString(u8"训练权重目录不存在: %1").arg(directory));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void TrainDataForm::on_PB_ModeCopy_clicked()
{
    const QString sourcePath = m_lastBestCheckpointPath.isEmpty()
                                   ? QDir(WeightsDirectory()).filePath(QStringLiteral("best.pt"))
                                   : m_lastBestCheckpointPath;
    if (!QFileInfo::exists(sourcePath))
    {
        QMessageBox::critical(this, QString(u8"复制失败"), QString(u8"未找到 YOLOv8 训练导出的 best.pt"));
        return;
    }
    const QString destinationDirectory = QFileDialog::getExistingDirectory(this, QString(u8"选择模型导出目录"));
    if (destinationDirectory.isEmpty())
    {
        return;
    }
    const QString destinationPath = QDir(destinationDirectory).filePath(QFileInfo(sourcePath).fileName());
    if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath))
    {
        QMessageBox::critical(this, QString(u8"复制失败"), QString(u8"无法覆盖目标文件: %1").arg(destinationPath));
        return;
    }
    if (!QFile::copy(sourcePath, destinationPath))
    {
        QMessageBox::critical(this,
                              QString(u8"复制失败"),
                              QString(u8"无法复制 checkpoint 到: %1").arg(destinationPath));
        return;
    }
    QMessageBox::information(this, QString(u8"复制完成"), QString(u8"模型已复制到: %1").arg(destinationPath));
}

void TrainDataForm::on_PB_OnnxOut_clicked()
{
    ShowUnsupported(QString(u8"ONNX 导出"));
}

void TrainDataForm::on_PB_EigenCamTest_clicked()
{
    ShowUnsupported(QString(u8"EigenCAM"));
}

void TrainDataForm::on_PB_OnnxMatch_clicked()
{
    ShowUnsupported(QString(u8"ONNX 匹配测试"));
}

void TrainDataForm::on_PB_Batch0_clicked()
{
    ShowUnsupported(QString(u8"训练批次预览"));
}

void TrainDataForm::on_PB_Batch1_clicked()
{
    ShowUnsupported(QString(u8"训练批次预览"));
}

void TrainDataForm::on_PB_Batch2_clicked()
{
    ShowUnsupported(QString(u8"训练批次预览"));
}

void TrainDataForm::on_PB_Val0_clicked()
{
    ShowUnsupported(QString(u8"验证标签预览"));
}

void TrainDataForm::on_PB_Val1_clicked()
{
    ShowUnsupported(QString(u8"验证预测预览"));
}

void TrainDataForm::InitChart()
{
    ui->ChartWidget->toAddLineSerices(Qt::green, QStringLiteral("IoU"), 0);
    ui->ChartWidget->toAddLineSerices(Qt::red, QStringLiteral("Loss"), 1);
    ui->ChartWidget->toInitChart(QString());
    ui->ChartWidget->toSetCrosshairVisible(true);
    ui->ChartWidget->toSetXAxis(1, ui->SB_epoch->value(), 1, 10, QString(u8"轮次"));
    ui->ChartWidget->toSetYAxis(0, 1, 4, QStringLiteral("IoU"));
    ui->ChartWidget->toSetOtherAxis(0, 1, 4, QStringLiteral("Loss"));
}

void TrainDataForm::SetTrainingUiState(bool running)
{
    ui->LE_ModeList->setEnabled(!running);
    ui->CB_ModeSize->setEnabled(!running);
    ui->PB_RunTrain->setEnabled(!running && !SelectedPluginPath().isEmpty());
    ui->PB_StopRun->setEnabled(running);
    ui->PB_ModeCopy->setEnabled(!running && QFileInfo::exists(m_lastBestCheckpointPath));
    ui->PB_OnnxOut->setEnabled(false);
}

void TrainDataForm::AppendLog(const QString &message)
{
    ui->plainTextEdit_Log->append(message);
    qInfo().noquote() << message;
}

void TrainDataForm::ShowUnsupported(const QString &featureName)
{
    const QString errorMessage = QString(u8"当前训练插件暂未支持 %1").arg(featureName);
    AppendLog(errorMessage);
    QMessageBox::critical(this, QString(u8"功能未支持"), errorMessage);
}

void TrainDataForm::UpdateTaskProgress(const QString &progress)
{
    QString errorMessage;
    if (!TaskRepository::UpdateProgress(m_ProcessName, progress, &errorMessage))
    {
        AppendLog(QString(u8"更新任务进度失败: %1").arg(errorMessage));
    }
}

QString TrainDataForm::WeightsDirectory() const
{
    return QDir(YtYoloDefine::toGetTrainPath()).filePath(m_ProcessName + QStringLiteral("/detect/train/weights"));
}

void TrainDataForm::RefreshPluginList()
{
    if (m_trainingController->IsRunning())
    {
        return;
    }

    const QString selectedPath = SelectedPluginPath();
    QStringList errorMessages;
    const QVector<DetectionPluginDescriptor> plugins = m_trainingController->DiscoverPlugins(&errorMessages);
    ui->LE_ModeList->clear();
    for (const DetectionPluginDescriptor &plugin : plugins)
    {
        const QString displayName = plugin.version.isEmpty()
                                        ? plugin.displayName
                                        : QStringLiteral("%1 %2").arg(plugin.displayName, plugin.version);
        ui->LE_ModeList->addItem(displayName, plugin.filePath);
        const int itemIndex = ui->LE_ModeList->count() - 1;
        ui->LE_ModeList->setItemData(itemIndex, plugin.filePath, kPluginPathRole);
        ui->LE_ModeList->setItemData(itemIndex, plugin.displayName, kPluginNameRole);
        ui->LE_ModeList->setItemData(itemIndex, plugin.supportsExport, kPluginExportRole);
        ui->LE_ModeList->setItemData(itemIndex, plugin.id, kPluginIdRole);
        if (plugin.filePath == selectedPath)
        {
            ui->LE_ModeList->setCurrentIndex(itemIndex);
        }
    }
    for (const QString &errorMessage : errorMessages)
    {
        AppendLog(errorMessage);
    }
    if (plugins.isEmpty())
    {
        AppendLog(QString(u8"未发现可用的检测训练插件，训练按钮已禁用"));
    }
    UpdatePluginUiState();
}

void TrainDataForm::UpdatePluginUiState()
{
    const bool hasPlugin = !SelectedPluginPath().isEmpty();
    const bool isYolo11 = SelectedPluginId() == QStringLiteral("visionaiflow.detection.yolov11");
    if (isYolo11)
    {
        ui->CB_ModeSize->setCurrentIndex(0);
    }
    ui->CB_ModeSize->setEnabled(!isYolo11 && !m_trainingController->IsRunning());
    ui->PB_RunTrain->setEnabled(hasPlugin && !m_trainingController->IsRunning());
    ui->PB_OnnxOut->setEnabled(false);
    ui->PB_OnnxOut->setToolTip(hasPlugin ? QString(u8"当前 YOLOv8 插件不支持 ONNX 导出")
                                         : QString(u8"请先选择训练插件"));
}

QString TrainDataForm::SelectedPluginPath() const
{
    return ui->LE_ModeList->currentData(kPluginPathRole).toString();
}

QString TrainDataForm::SelectedPluginId() const
{
    return ui->LE_ModeList->currentData(kPluginIdRole).toString();
}

QString TrainDataForm::SelectedPluginName() const
{
    return ui->LE_ModeList->currentData(kPluginNameRole).toString();
}
