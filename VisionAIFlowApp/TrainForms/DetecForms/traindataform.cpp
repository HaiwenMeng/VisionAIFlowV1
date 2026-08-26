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

namespace
{
constexpr int kNativeInputSize = 640;
constexpr int kNativeChannels = 3;
constexpr double kNativeLearningRate = 0.001;
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
    InitChart();

    connect(m_trainingController,
            &DetectTrainingController::Progress,
            this,
            [this](int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double meanIou)
            {
                ui->ChartWidget->toInsertData(0, step, static_cast<float>(meanIou));
                ui->ChartWidget->toInsertData(1, step, static_cast<float>(loss));
                AppendLog(QString(u8"轮次: %1, 步数: %2, 损失: %3, 框损失: %4, 分类损失: %5, 正样本: %6, IoU: %7")
                              .arg(epoch)
                              .arg(step)
                              .arg(loss, 0, 'f', 6)
                              .arg(boxLoss, 0, 'f', 6)
                              .arg(classLoss, 0, 'f', 6)
                              .arg(positives)
                              .arg(meanIou, 0, 'f', 6));
            });
    connect(m_trainingController,
            &DetectTrainingController::Completed,
            this,
            [this](const QString &runDirectory, const QString &modelPath, const QString &bestCheckpointPath)
            {
                AppendLog(QString(u8"训练完成。输出目录: %1").arg(runDirectory));
                AppendLog(QString(u8"ONNX: %1").arg(modelPath));
                AppendLog(QString(u8"最佳检查点: %1").arg(bestCheckpointPath));
                UpdateTaskProgress(QStringLiteral("trained"));
                QMessageBox::information(this, QString(u8"训练完成"), QString(u8"Yolo11 原生训练已完成"));
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
    ui->LE_CurentProName->setText(setName);
}

void TrainDataForm::toInitShow()
{
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
    AppendLog(QString(u8"检测训练使用 C++ Yolo11 n 插件。输入固定为 640，通道固定为 3，学习率固定为 0.001。"));
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
    if (ui->CB_ModeSize->currentIndex() != 0)
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"当前原生插件仅支持 Yolo11 n 模型"));
        return;
    }

    const QString csvPath = QDir(YtYoloDefine::toGetDataPath()).filePath(m_ProcessName + QStringLiteral("/train.csv"));
    if (!QFileInfo::exists(csvPath))
    {
        QMessageBox::critical(this, QString(u8"训练失败"), QString(u8"未找到已生成的数据集: %1").arg(csvPath));
        return;
    }

    ui->ChartWidget->removeAllPoints();
    ui->ChartWidget->toSetMaxVal(0, ui->SB_epoch->value());
    DetectTrainingRequest request;
    request.taskName = m_ProcessName;
    request.epochs = ui->SB_epoch->value();
    request.batchSize = ui->CB_BachSize->currentText().toInt();
    request.learningRate = kNativeLearningRate;
    request.horizontalFlip = false;
    const QString checkpointPath =
        QDir(YtYoloDefine::toGetTrainPath()).filePath(m_ProcessName + QStringLiteral("/YtPretrained.pt"));
    if (QFileInfo::exists(checkpointPath))
    {
        request.resumeCheckpointPath = checkpointPath;
        AppendLog(QString(u8"使用原生检查点继续训练: %1").arg(checkpointPath));
    }
    else
    {
        AppendLog(QString(u8"开始新的 Yolo11 n 原生训练"));
    }

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
    const QString sourcePath = QDir(WeightsDirectory()).filePath(QStringLiteral("best.onnx"));
    if (!QFileInfo::exists(sourcePath))
    {
        QMessageBox::critical(this, QString(u8"复制失败"), QString(u8"未找到原生训练导出的 best.onnx"));
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
        QMessageBox::critical(this, QString(u8"复制失败"), QString(u8"无法复制 ONNX 文件到: %1").arg(destinationPath));
        return;
    }
    QMessageBox::information(this, QString(u8"复制完成"), QString(u8"模型已复制到: %1").arg(destinationPath));
}

void TrainDataForm::on_PB_OnnxOut_clicked()
{
    const QString modelPath = QDir(WeightsDirectory()).filePath(QStringLiteral("best.onnx"));
    if (!QFileInfo::exists(modelPath))
    {
        QMessageBox::critical(this, QString(u8"导出失败"), QString(u8"未找到 best.onnx。请先完成原生训练。"));
        return;
    }
    QMessageBox::information(this, QString(u8"ONNX 已生成"), modelPath);
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
    ui->ChartWidget->toSetXAxis(0, ui->SB_epoch->value(), 1, 10);
    ui->ChartWidget->toSetYAxis(0, 1, 4, QStringLiteral("IoU"));
    ui->ChartWidget->toSetOtherAxis(0, 1, 4, QStringLiteral("Loss"));
}

void TrainDataForm::SetTrainingUiState(bool running)
{
    ui->PB_RunTrain->setEnabled(!running);
    ui->PB_StopRun->setEnabled(running);
}

void TrainDataForm::AppendLog(const QString &message)
{
    ui->plainTextEdit_Log->append(message);
    qInfo().noquote() << message;
}

void TrainDataForm::ShowUnsupported(const QString &featureName)
{
    const QString errorMessage = QString(u8"原生 Yolo11 训练暂未支持 %1").arg(featureName);
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
