#include "YoloeLabelForm.h"
#include "ui_YoloeLabelForm.h"
#include "setnamedialog.h"
#include <QDir>
#include "setlabelname.h"
#include "filecopysetdlg.h"
#include "renamedlg.h"
#include "nameselectdlg.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QTableWidgetItem>
#include <QApplication>
#include <QProgressDialog>
#include <QProgressBar>
#include <QListWidget>
#include <QList>
#include <QHeaderView>
#include <QPushButton>
#include <QSize>
#include <QShortcut>
#include <QSignalBlocker>
#include <exception>
#include <utility>

#include <opencv2/imgproc.hpp>

namespace
{
const QString kYoloeIgnoreCanvasLabel = QStringLiteral("__yoloe_training_ignore__");
const QString kYoloeIgnoreSubtractCanvasLabel = QStringLiteral("__yoloe_training_ignore_subtract__");
const QString kYoloeIgnoreDisplayLabel = QString(u8"训练忽略区");
const QString kYoloeIgnoreSubtractDisplayLabel = QString(u8"有效训练区");
const QColor kYoloeIgnoreColor(190, 70, 255);
const QColor kYoloeIgnoreSubtractColor(70, 220, 255);

std::vector<cv::Point> toCvPolygon(LabelSet label)
{
    std::vector<cv::Point> polygon;
    const QVector<QPointF> points = label.toGetPoints();
    polygon.reserve(points.size());
    for (const QPointF &point : points)
    {
        polygon.emplace_back(qRound(point.x()), qRound(point.y()));
    }
    return polygon;
}

void fillLabelMask(cv::Mat &mask, LabelSet label, uchar value)
{
    const std::vector<cv::Point> polygon = toCvPolygon(label);
    if (polygon.size() >= 3)
    {
        cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{polygon}, cv::Scalar(value));
    }
}

QVector<QVector<double>> contoursToRoiData(const cv::Mat &mask, bool *hasHole = nullptr)
{
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask.clone(), contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
    if (hasHole)
    {
        *hasHole = false;
    }
    QVector<QVector<double>> result;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
        if (!hierarchy.empty() && hierarchy.at(i)[3] >= 0)
        {
            if (hasHole)
            {
                *hasHole = true;
            }
            continue;
        }
        std::vector<cv::Point> approximated;
        cv::approxPolyDP(contours.at(i), approximated, 1.0, true);
        if (approximated.size() < 3 || std::abs(cv::contourArea(approximated)) <= 0.0)
        {
            continue;
        }
        QVector<double> roiData;
        roiData.reserve(static_cast<int>(approximated.size()) * 2);
        for (const cv::Point &point : approximated)
        {
            roiData << point.x << point.y;
        }
        result.append(roiData);
    }
    return result;
}

QVector<double> toDisplayPointData(LabelSet &label)
{
    QVector<double> displayData;
    const QVector<QPointF> points = label.toGetPoints();
    displayData.reserve(points.size() * 2);
    for (const QPointF &pt : points)
    {
        displayData.append(pt.x());
        displayData.append(pt.y());
    }
    return displayData;
}

QString toRoiText(const QVector<double> &roiData)
{
    QString text;
    for (int i = 0; i < roiData.size(); ++i)
    {
        if (i == 0)
        {
            text = QString::number(roiData[i], 'f', 4);
        }
        else
        {
            text += "#" + QString::number(roiData[i], 'f', 4);
        }
    }
    return text;
}

QString toSamTypeName()
{
    return QStringLiteral("SAM2");
}

bool toSamModelFilesExist(const QString &dirPath)
{
    return QFileInfo::exists(dirPath + "/sam2_image_encode.fp16.trt") &&
           QFileInfo::exists(dirPath + "/sam2_decode.fp16.trt");
}
bool toStringHasNonAscii(const QString &text)
{
    for (const QChar &ch : text)
    {
        if (ch.unicode() > 0x7f)
        {
            return true;
        }
    }
    return false;
}

QString toSamModelPathError(const QString &dirPath)
{
    QStringList paths;
    paths << dirPath << (dirPath + QStringLiteral("/sam2_image_encode.fp16.trt"))
          << (dirPath + QStringLiteral("/sam2_decode.fp16.trt"));

    for (const QString &path : paths)
    {
        if (toStringHasNonAscii(path))
        {
            return QString(u8"SAM2 模型路径不能包含非 ASCII 字符: %1").arg(path);
        }
    }
    return QString();
}

QStringList toSamCandidateDirs()
{
    QStringList candidateDirs;
    const QString appDir = QCoreApplication::applicationDirPath();
    candidateDirs << (appDir + QStringLiteral("/sam2"));
    candidateDirs.removeDuplicates();
    return candidateDirs;
}

void applySam2CommercialUi(Ui::YoloeLabelForm *ui, QWidget *root)
{
    if (!ui || !root)
    {
        return;
    }

    const QList<QPushButton *> dangerButtons = {ui->PB_RemoveLab,
                                                ui->PB_AddAnno,
                                                ui->PB_ClearLab,
                                                ui->PB_ReMoveCurentLab,
                                                ui->PB_SunDataSet};

    for (QPushButton *button : dangerButtons)
    {
        if (button)
        {
            button->setProperty("danger", true);
        }
    }

    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons)
    {
        button->setIconSize(QSize(18, 18));
    }

    const QList<QListWidget *> lists = root->findChildren<QListWidget *>();
    for (QListWidget *list : lists)
    {
        list->setAlternatingRowColors(true);
        list->setIconSize(QSize(18, 18));
    }

    ui->LW_SetLab->setAlternatingRowColors(true);
    ui->LW_SetLab->setShowGrid(false);
    ui->LW_SetLab->horizontalHeader()->setHighlightSections(false);
    ui->LW_SetLab->setIconSize(QSize(18, 18));
}
void toSyncHighlightedRoiFromTable(Ui::YoloeLabelForm *ui)
{
    if (!ui || !ui->YtLabelRoi || !ui->LW_SetLab)
    {
        return;
    }

    const int row = ui->LW_SetLab->currentRow();
    QTableWidgetItem *keyItem = row >= 0 ? ui->LW_SetLab->item(row, 0) : nullptr;
    ui->YtLabelRoi->toSetHighlightedRoiKey(keyItem ? keyItem->text() : QString());
}

QProgressDialog *toCreateSamInitDialog(QWidget *parent)
{
    QProgressDialog *dialog = new QProgressDialog(parent);
    dialog->setObjectName(QStringLiteral("samInitDialog"));
    dialog->setWindowTitle(QStringLiteral("SAM2"));
    dialog->setLabelText(QString(u8"正在初始化 SAM2 模型..."));
    dialog->setCancelButton(nullptr);
    dialog->setRange(0, 0);
    dialog->setMinimumDuration(0);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setMinimumWidth(380);
    if (QProgressBar *bar = dialog->findChild<QProgressBar *>())
    {
        bar->setTextVisible(false);
    }
    return dialog;
}
} // namespace

YoloeLabelForm::YoloeLabelForm(QWidget *parent) : QWidget(parent), ui(new Ui::YoloeLabelForm)
{
    ui->setupUi(this);
    ui->PB_ManualPolygon->setVisible(false);
    connect(ui->CB_CurrentLabel,
            &QComboBox::currentTextChanged,
            this,
            [this](const QString &name)
            {
                ui->YtLabelRoi->toSetRoiDefaltName(name);
            });
    applySam2CommercialUi(ui, this);
    QShortcut *lastImageShortcut = new QShortcut(QKeySequence(Qt::Key_A), this);
    lastImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(lastImageShortcut, &QShortcut::activated, this, &YoloeLabelForm::on_PB_LastIm_clicked);

    QShortcut *nextImageShortcut = new QShortcut(QKeySequence(Qt::Key_D), this);
    nextImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextImageShortcut, &QShortcut::activated, this, &YoloeLabelForm::on_PB_NextIm_clicked);

    ui->stackedWidget->setCurrentIndex(0);

    // ���� CommonSetForm ���������ֱ��������Ŀ¼�����У��м仭��������չ���Ҳ��ǩ���̶���խ
    ui->gridLayout_7->setColumnStretch(0, 1);
    ui->gridLayout_7->setColumnStretch(1, 1);
    ui->gridLayout_7->setColumnStretch(2, 1);
    ui->gridLayout_7->setColumnStretch(3, 10);
    ui->gridLayout_7->setColumnStretch(4, 3);
    //    toUpdateSamStatusIndicator(ui, false);
    toInitSam2Inference();
    ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");

    //
    connect(ui->YtLabelRoi, &Sam2LabelCanvasCompat::SigGetDataUpdate, this, &YoloeLabelForm::toGetDataUpdate);
    connect(ui->YtLabelRoi, &Sam2LabelCanvasCompat::ROIChange, this, &YoloeLabelForm::Slot_ROIChange);
    connect(ui->YtLabelRoi, &Sam2LabelCanvasCompat::SamRuntimeError, this, &YoloeLabelForm::toHandleSam2RuntimeError);
    connect(ui->YtLabelRoi, &Sam2LabelCanvasCompat::EraserStrokeCommitted, this, &YoloeLabelForm::toApplyEraserStroke);
    connect(ui->YtLabelRoi,
            &Sam2LabelCanvasCompat::EraserExitRequested,
            this,
            [this]()
            {
                ui->PB_Eraser->setChecked(false);
            });
    connect(ui->YtLabelRoi,
            &Sam2LabelCanvasCompat::ManualPolygonError,
            this,
            [this](const QString &errorMessage)
            {
                QMessageBox::warning(this, QString(u8"警告"), errorMessage);
            });
    connect(ui->LW_SetLab,
            &QTableWidget::itemSelectionChanged,
            this,
            [this]()
            {
                toSyncHighlightedRoiFromTable(ui);
            });
    //
    ui->LW_SetLab->verticalHeader()->setVisible(false); // ���ش�ֱ��ͷ
    // ui->LW_SetLab->horizontalHeader()->setStretchLastSection(true);
    ui->LW_SetLab->setEditTriggers(QAbstractItemView::NoEditTriggers); // ���ɱ༭
    ui->LW_SetLab->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->LW_SetLab->setSelectionBehavior(QAbstractItemView::SelectRows); // ѡ��һ��
    ui->LW_SetLab->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    toInitMenuData();
    connect(ui->LW_DataSheet, &QListWidget::itemChanged, this, &YoloeLabelForm::slotDataSheetItemChanged);
}

YoloeLabelForm::~YoloeLabelForm()
{
    delete ui;

    if (m_SetProcess)
    {
        m_SetProcess->close();
        delete m_SetProcess;
        m_SetProcess = nullptr;
    }
}

void YoloeLabelForm::setTaskName(const QString &taskName)
{
    m_ProcessName = taskName;
    toInitProShowLabel();
}

void YoloeLabelForm::setIgnoreEnabled(bool enabled)
{
    ui->PB_Eraser->setChecked(false);
    ui->PB_ManualPolygon->setChecked(false);
    ui->PB_DrawIgnore->setVisible(enabled);
    ui->PB_Eraser->setVisible(enabled);
    ui->spinBox_EraserSize->setVisible(enabled);
    ui->ckB_ShowMask->setVisible(enabled);
    ui->PB_ManualPolygon->setVisible(!enabled);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_PB_ManualPolygon_toggled(bool checked)
{
    Q_UNUSED(checked);
    toSyncSam2PromptMode();
}

bool YoloeLabelForm::initialize()
{
    toInitSam2Inference();
    return isReady();
}

void YoloeLabelForm::releaseSam2()
{
    ui->YtLabelRoi->releaseSam2();
    m_Sam2Available = false;
}

bool YoloeLabelForm::isReady() const
{
    return m_Sam2Available && ui->YtLabelRoi->isSamReady();
}

void YoloeLabelForm::toInitSam2Inference()
{
    if (!ui || !ui->YtLabelRoi)
    {
        return;
    }

    if (ui->YtLabelRoi->isSamReady())
    {
        m_Sam2Available = true;
        m_Sam2UnavailableMessageShown = false;
        if (ui->CB_SetLabeMode)
        {
            ui->CB_SetLabeMode->setEnabled(true);
        }
        return;
    }

    QProgressDialog *initDialog = toCreateSamInitDialog(this);
    initDialog->show();
    qApp->processEvents();

    const QStringList candidateDirs = toSamCandidateDirs();
    bool inited = false;
    bool foundModelFiles = false;
    QString usedDir;
    QString failureMessage;
    for (const QString &dirPath : candidateDirs)
    {
        if (!toSamModelFilesExist(dirPath))
        {
            continue;
        }

        foundModelFiles = true;
        const QString pathError = toSamModelPathError(dirPath);
        if (!pathError.isEmpty())
        {
            failureMessage = pathError;
            qWarning() << toSamTypeName() << "model path rejected:" << pathError;
            break;
        }

        bool initOk = false;
        try
        {
            initOk = ui->YtLabelRoi->toInitSam2Model(dirPath);
        }
        catch (const std::exception &ex)
        {
            failureMessage =
                QString(u8"SAM2 模型初始化异常，目录: %1，错误: %2").arg(dirPath, QString::fromLocal8Bit(ex.what()));
            qWarning() << failureMessage;
        }
        catch (...)
        {
            failureMessage = QString(u8"SAM2 模型初始化发生未知异常，目录: %1").arg(dirPath);
            qWarning() << failureMessage;
        }

        if (initOk)
        {
            inited = true;
            usedDir = dirPath;
            break;
        }

        if (failureMessage.isEmpty())
        {
            failureMessage = QString(u8"SAM2 模型初始化失败，目录: %1").arg(dirPath);
        }
    }

    initDialog->close();
    initDialog->deleteLater();
    qApp->processEvents();

    if (inited)
    {
        m_Sam2Available = true;
        m_Sam2UnavailableMessageShown = false;
        if (ui->CB_SetLabeMode)
        {
            ui->CB_SetLabeMode->setEnabled(true);
        }
        qDebug() << toSamTypeName() << "model initialized from:" << usedDir;
        //        toUpdateSamStatusIndicator(ui, true);
        toSyncSam2PromptMode();
    }
    else
    {
        if (failureMessage.isEmpty())
        {
            if (foundModelFiles)
            {
                failureMessage = QString(u8"SAM2 模型初始化失败，请检查 TensorRT、CUDA 和引擎兼容性");
            }
            else
            {
                failureMessage =
                    QString(u8"缺少 SAM2 模型文件，请先生成 sam2_image_encode.fp16.trt 和 sam2_decode.fp16.trt");
            }
        }
        qWarning() << toSamTypeName() << "model unavailable, candidates:" << candidateDirs
                   << "error:" << failureMessage;
        toHandleSam2Unavailable(failureMessage, false);
        //        toUpdateSamStatusIndicator(ui, false);
    }
}

void YoloeLabelForm::toHandleSam2Unavailable(const QString &errorMessage, bool forceMessage)
{
    QString message = errorMessage;
    if (message.isEmpty())
    {
        message = QString(u8"SAM2 不可用，无法继续 YOLOE mask 标注");
    }

    m_Sam2Available = false;
    qWarning() << "SAM2 unavailable:" << message;

    if (ui)
    {
        if (ui->CB_SetLabeMode)
        {
            ui->CB_SetLabeMode->setEnabled(false);
        }
        if (ui->CB_ROIType)
        {
            ui->CB_ROIType->setEnabled(false);
        }
        toSyncSam2PromptMode();
    }

    if ((forceMessage || !m_Sam2UnavailableMessageShown) && ui)
    {
        QMessageBox::warning(this, QString(u8"警告"), message);
        m_Sam2UnavailableMessageShown = true;
    }
}

void YoloeLabelForm::toHandleSam2RuntimeError(const QString &errorMessage)
{
    QString message = errorMessage;
    if (message.isEmpty())
    {
        message = QString(u8"SAM2 推理失败");
    }

    qWarning() << "SAM2 runtime error:" << message;
    if (ui)
    {
        QMessageBox::warning(this, QString(u8"警告"), message);
    }
}
void YoloeLabelForm::toInitProShowLabel()
{
    toInitSam2Inference();

    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    toInitLabelSet();
    toInitPath();
    //
    m_CurentIndex = -1;
    m_CurentFileName = "";
    ui->LW_FileList->clear();
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toSetImage(QImage());
    ui->YtLabelRoi->toRemoveAllRoi();
}

void YoloeLabelForm::toInitLabelSet()
{
    ui->LW_LabelSet->clear();
    ui->CB_CurrentLabel->clear();
    QStringList temNames;
    for (int i = 0; i < m_YtYoloSetPro.toGetLabelName().size(); i++)
    {
        QListWidgetItem *item = new QListWidgetItem(m_YtYoloSetPro.toGetLabelName().at(i));
        //
        QImage temim = QImage(40, 40, QImage::Format_RGB888);
        temim.fill(m_YtYoloSetPro.toGetColorDefine().at(i));
        item->setIcon(QPixmap::fromImage(temim));

        ui->LW_LabelSet->addItem(item);

        ui->CB_CurrentLabel->addItem(QPixmap::fromImage(temim), m_YtYoloSetPro.toGetLabelName().at(i));
        temNames << m_YtYoloSetPro.toGetLabelName().at(i);
    }
    temNames << kYoloeIgnoreCanvasLabel;
    temNames << kYoloeIgnoreSubtractCanvasLabel;
    QVector<QColor> canvasColors = m_YtYoloSetPro.toGetColorDefine();
    canvasColors << kYoloeIgnoreColor;
    canvasColors << kYoloeIgnoreSubtractColor;
    ui->CB_CurrentLabel->setCurrentIndex(0);
    ui->CB_ROIType->setCurrentIndex(0);
    //
    ui->YtLabelRoi->toSetLabelNames(temNames, canvasColors);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::toSyncSam2PromptMode()
{
    if (!ui || !ui->YtLabelRoi || !ui->CB_ROIType || !ui->CB_SetLabeMode || !ui->CB_CurrentLabel)
    {
        return;
    }

    const bool eraserMode = ui->PB_Eraser && ui->PB_Eraser->isChecked();
    const bool samReady = m_Sam2Available && ui->YtLabelRoi->isSamReady();
    const bool manualPolygon = !ui->PB_ManualPolygon->isHidden() && ui->PB_ManualPolygon->isChecked();
    ui->YtLabelRoi->toSetManualPolygonMode(manualPolygon);
    ui->YtLabelRoi->toSetEraserMode(eraserMode);
    ui->YtLabelRoi->toSetEraserDiameter(ui->spinBox_EraserSize->value());
    ui->YtLabelRoi->toSetIgnoreMaskVisible(ui->ckB_ShowMask->isChecked());
    if (manualPolygon)
    {
        ui->CB_SetLabeMode->setEnabled(false);
        ui->CB_ROIType->setEnabled(false);
        ui->CB_CurrentLabel->setEnabled(true);
        ui->YtLabelRoi->toSetRoiDefaltName(ui->CB_CurrentLabel->currentText());
        ui->YtLabelRoi->setEnabled(true);
        return;
    }
    if (eraserMode)
    {
        ui->CB_SetLabeMode->setEnabled(false);
        ui->CB_ROIType->setEnabled(false);
        ui->CB_CurrentLabel->setEnabled(false);
        ui->YtLabelRoi->setEnabled(true);
        return;
    }

    ui->CB_CurrentLabel->setEnabled(true);
    if (!samReady)
    {
        ui->CB_SetLabeMode->setEnabled(false);
        ui->CB_ROIType->setEnabled(false);
        ui->YtLabelRoi->setEnabled(false);
    }
    else
    {
        ui->CB_SetLabeMode->setCurrentIndex(0);
        ui->CB_SetLabeMode->setEnabled(false);
        ui->CB_ROIType->setEnabled(samReady);
        ui->YtLabelRoi->setEnabled(true);
    }

    const bool manualBoxMode = false;
    int roiType = LrectangleROI;

    if (manualBoxMode)
    {
        if (ui->CB_ROIType->currentIndex() != 0)
        {
            ui->CB_ROIType->setCurrentIndex(0);
        }
        ui->CB_ROIType->setEnabled(false);
    }
    else
    {
        ui->CB_ROIType->setEnabled(true);
        switch (ui->CB_ROIType->currentIndex())
        {
        case 0:
            roiType = LrectangleROI;
            break;
        case 1:
            roiType = LpointROI;
            break;
        default:
            ui->CB_ROIType->setCurrentIndex(0);
            roiType = LrectangleROI;
            break;
        }
    }

    ui->YtLabelRoi->toSetManualBoxMode(manualBoxMode);
    ui->YtLabelRoi->toSetRoiDefaltType(roiType);
    ui->YtLabelRoi->toSetRoiDefaltName(ui->CB_CurrentLabel->currentText());
    if (ui->CB_SetLabeMode->currentIndex() < 0)
    {
        ui->YtLabelRoi->toSetRoiDefaltName("");
    }
}

void YoloeLabelForm::toAddDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState)
{
    QListWidgetItem *item = new QListWidgetItem(dataSheetName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checkState);
    ui->LW_DataSheet->addItem(item);
}

void YoloeLabelForm::toSaveDataSheetCheckState()
{
    if (m_ProcessName.isEmpty())
    {
        return;
    }
    QStringList uncheckedDataSheetList;
    for (int i = 0; i < ui->LW_DataSheet->count(); i++)
    {
        QListWidgetItem *item = ui->LW_DataSheet->item(i);
        if (item && item->checkState() != Qt::Checked && !uncheckedDataSheetList.contains(item->text()))
        {
            uncheckedDataSheetList.append(item->text());
        }
    }
    if (m_YtYoloSetPro.m_UncheckedDataSheetList == uncheckedDataSheetList)
    {
        return;
    }
    m_YtYoloSetPro.m_UncheckedDataSheetList = uncheckedDataSheetList;
    m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
}

void YoloeLabelForm::toInitPath()
{
    QSignalBlocker blocker(ui->LW_DataSheet);
    ui->LW_DataSheet->clear();
    QDir temdir(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    QFileInfoList temlis = temdir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (int i = 0; i < temlis.size(); i++)
    {
        Qt::CheckState checkState =
            m_YtYoloSetPro.m_UncheckedDataSheetList.contains(temlis.at(i).fileName()) ? Qt::Unchecked : Qt::Checked;
        toAddDataSheetItem(temlis.at(i).fileName(), checkState);
    }
    toSaveDataSheetCheckState();
}

void YoloeLabelForm::slotDataSheetItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    toSaveDataSheetCheckState();
}
void YoloeLabelForm::toGetDataUpdate(QString SetKey, QString LableName, int type)
{
    Q_UNUSED(type);
    if (m_IsaddState)
    {
        return;
    }
    int rowcount = ui->LW_SetLab->rowCount();
    const bool isIgnore = LableName == kYoloeIgnoreCanvasLabel;
    int getindex = m_YtYoloSetPro.toGetLabelName().lastIndexOf(LableName);
    if (!isIgnore && getindex < 0)
    {
        return;
    }
    LabelSet temppset;
    for (int i = 0; i < (rowcount + 1); i++)
    {
        if (SetKey.toInt() == (i + 1))
        {
            ui->LW_SetLab->insertRow(i);
            ui->LW_SetLab->setItem(i, 0, new QTableWidgetItem(SetKey));

            ui->LW_SetLab->setItem(i, 1, new QTableWidgetItem(isIgnore ? kYoloeIgnoreDisplayLabel : LableName));
            ui->LW_SetLab->item(i, 1)->setBackground(isIgnore ? kYoloeIgnoreColor
                                                              : m_YtYoloSetPro.toGetColorDefine().at(getindex));

            QVector<double> getData = ui->YtLabelRoi->getROI(SetKey);
            temppset.toInitProData(isIgnore ? QString() : LableName, LpolygonROI, getData);
            temppset.trainingRole = isIgnore ? LabelSet::TrainingRole::Ignore : LabelSet::TrainingRole::Target;
            if (!isIgnore)
            {
                m_YtRoiLabelSet.m_SampleRole = YtRoiLabelSet::SampleRole::Labeled;
            }
            ui->LW_SetLab->setItem(i, 2, new QTableWidgetItem(temppset.shape_type));

            ui->LW_SetLab->setItem(i, 3, new QTableWidgetItem(toRoiText(getData)));
            //
            m_YtRoiLabelSet.toInserData(temppset, i);
            for (int m = 0; m < 4; m++)
            {
                ui->LW_SetLab->item(i, m)->setTextAlignment(Qt::AlignCenter);
            }
        }
    }
    if (ui->CB_SaveROIType->currentIndex() == 0)
    {
        on_PB_SaveCurent_clicked();
    }
}

void YoloeLabelForm::Slot_ROIChange(QVector<double> tdata, QString &key, int type)
{
    Q_UNUSED(type);
    if (m_IsaddState)
    {
        return;
    }
    int rowcount = ui->LW_SetLab->rowCount();
    LabelSet temppset;
    int getindex = -1;
    for (int i = 0; i < rowcount; i++)
    {
        if (key.toInt() == ui->LW_SetLab->item(i, 0)->text().toInt())
        {
            getindex = i;
            const LabelSet &existingLabel = m_YtRoiLabelSet.m_SetLabeset.at(i);
            temppset.toInitProData(existingLabel.Name, LpolygonROI, tdata);
            temppset.trainingRole = existingLabel.trainingRole;
            temppset.ignoreOperation = existingLabel.ignoreOperation;
            ui->LW_SetLab->setItem(i, 3, new QTableWidgetItem(toRoiText(tdata)));
            //
            break;
        }
    }

    if (getindex < 0)
    {
        qWarning() << "YOLOE polygon update failed: ROI key not found" << key;
        return;
    }

    if (ui->CB_SaveROIType->currentIndex() == 0)
    {
        m_YtRoiLabelSet.toModify(temppset, getindex, false);
        on_PB_SaveCurent_clicked();
    }
}

void YoloeLabelForm::on_PB_AddDataSet_clicked()
{

    SetLabelName tDlg;
    tDlg.setWindowFlags(Qt::SubWindow);
    if (QDialog::Accepted == tDlg.exec())
    {
        QColor getColor;
        QString getLable;
        tDlg.toGetData(getLable, getColor);
        if (getLable.isEmpty() || m_YtYoloSetPro.toGetLabelName().contains(getLable))
        {
            QMessageBox::warning(0, u8"警告", u8"标签已存在或为空", u8"确定");

            return;
        }
        m_YtYoloSetPro.toAppendLabelInfo(getLable, getColor);

        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
        // qDebug()<<"label data save path:" <<m_WorkPath;
        toInitLabelSet();
    }
}

void YoloeLabelForm::on_PB_SunDataSet_clicked()
{
    int index = ui->LW_LabelSet->currentRow();

    if (index >= 0)
    {
        QListWidgetItem *temite = ui->LW_LabelSet->takeItem(index);
        delete temite;
        m_YtYoloSetPro.m_ClorDefine.removeAt(index);
        m_YtYoloSetPro.m_NameList.removeAt(index);
        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
        toInitLabelSet();
    }
}

void YoloeLabelForm::on_LW_LabelSet_itemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    int getindex = ui->LW_LabelSet->currentRow();
    if (getindex < 0)
    {
        return;
    }
    SetLabelName tDlg;
    const QString projectPath = YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName;
    tDlg.toSetModifyData(m_YtYoloSetPro.toGetLabelName().at(getindex),
                         m_YtYoloSetPro.toGetColorDefine().at(getindex),
                         getindex,
                         projectPath);
    if (QDialog::Accepted == tDlg.exec())
    {
        m_YtYoloSetPro.toLoadData(projectPath);
        toInitLabelSet();
        if (m_CurentIndex >= 0 && !m_WorkPath.isEmpty() && !m_CurentFileName.isEmpty())
        {
            on_PB_Reload_clicked();
        }
    }
}

void YoloeLabelForm::on_PB_AddLab_clicked()
{
    int getindex = ui->LW_DataSheet->currentRow();

    FileCopySetDlg tDlg;
    tDlg.setWindowFlags(Qt::SubWindow);
    if (getindex >= 0)
    {
        tDlg.toSetTitleName(ui->LW_DataSheet->item(getindex)->text());
    }
    if (QDialog::Accepted == tDlg.exec())
    {

        QDir temdir(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
        QFileInfoList temlis = temdir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        ////////////////
        QDir destPath;
        if (!tDlg.toGetTitleName().isEmpty())
        {
            destPath.setPath(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + tDlg.toGetTitleName());
            if (!destPath.exists())
            {
                destPath.mkpath(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + tDlg.toGetTitleName());
                toAddDataSheetItem(tDlg.toGetTitleName(), Qt::Checked);
            }
        }
        else
        {

            for (int i = 1; i <= (temlis.size() + 1); i++)
            {
                QDir temss(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + QString::number(i));
                if (!temss.exists())
                {
                    destPath.setPath(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + QString::number(i));

                    destPath.mkpath(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + QString::number(i));
                    toAddDataSheetItem(QString::number(i), Qt::Checked);
                }
            }
        }
        tDlg.toGetCopyImage(destPath.path());
        toSaveDataSheetCheckState();
        QMessageBox::about(0, u8"提示", u8"数据添加完成");
    }
}

void YoloeLabelForm::on_PB_RemoveLab_clicked()
{
    int index = ui->LW_DataSheet->currentRow();
    if (index < 0)
    {
        return;
    }
    //
    QDir temdir;
    temdir.setPath(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + ui->LW_DataSheet->item(index)->text());
    if (1 == QMessageBox::question(0, u8"警告", u8"确定删除数据集？删除后不可恢复", u8"取消", u8"确定"))
    {

        if (true == temdir.removeRecursively())
        {
            QMessageBox::about(0, u8"提示", QString(u8"删除目录 %1 成功").arg(ui->LW_DataSheet->item(index)->text()));
            delete ui->LW_DataSheet->takeItem(index);
            toSaveDataSheetCheckState();
        }
        else
        {
            QMessageBox::warning(0,
                                 u8"提示",
                                 QString(u8"删除目录 %1 失败").arg(ui->LW_DataSheet->item(index)->text()),
                                 u8"取消",
                                 u8"确定");
        }
    }
}

void YoloeLabelForm::on_LW_DataSheet_itemClicked(QListWidgetItem *item)
{
    if (ui->LW_DataSheet->currentItem() == nullptr)
    {
        return;
    }

    m_WorkPath = YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + ui->LW_DataSheet->currentItem()->text();
    //
    QStringList filtelis;
    filtelis << "*.bmp"
             << "*.jpg"
             << "*.jpeg"
             << "*.png"
             << "*.tiff";
    QFileInfoList CurentFiles = YtYoloDefine::toGetPathFileInfo(m_WorkPath, filtelis);
    //
    ui->LW_FileList->clear();
    int currentSelect = ui->comboBox->currentIndex();

    if (currentSelect == 4)
    {
        ui->CB_CurrentLabel->setEnabled(false);
    }
    //
    for (int i = 0; i < CurentFiles.size(); i++)
    {

        const QFileInfo &fileInfo = CurentFiles.at(i);
        QString baseName = fileInfo.completeBaseName(); // ��ȡ������չ�����ļ���
        QString jsonPath = m_WorkPath + "/" + baseName + ".json";
        QString txtPath = m_WorkPath + "/" + baseName + ".txt";

        bool isAnnotated = QFile::exists(jsonPath);
        bool isDected = QFile::exists(txtPath);

        // ����ɸѡ���������Ƿ���ʾ���ļ�
        if (currentSelect == 1 && !isAnnotated)
            continue;
        if (currentSelect == 2 && isAnnotated)
            continue;
        if (currentSelect == 3 && !isDected)
            continue;
        if (currentSelect == 4 && !ProTrainData::IsJsonHasLabel(jsonPath, ui->CB_CurrentLabel->currentText()))
            continue;

        // �����ļ����б�
        QListWidgetItem *listItem = new QListWidgetItem(fileInfo.fileName());

        // ����ͼ��
        listItem->setIcon(
            QIcon(isAnnotated ? u8":/ResourYolo/Selfbutton/已标注.png" : u8":/ResourYolo/Selfbutton/未标注.png"));

        // ��ѡ�����ò�ͬ״̬�Ĳ�ͬ�ı���ɫ
        //        listItem->setForeground(isAnnotated ? Qt::darkGreen : Qt::darkRed);

        ui->LW_FileList->addItem(listItem);
    }
    ui->CB_CurrentLabel->setEnabled(true);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_LW_FileList_itemSelectionChanged()
{

    m_CurentIndex = ui->LW_FileList->currentRow();
    m_CurentFileName = ui->LW_FileList->currentItem()->text();
    QString filename = m_WorkPath + "/" + m_CurentFileName;
    //
    m_GetImage.load(filename);

    m_YtRoiLabelSet.toSetFileName(m_CurentFileName);
    m_YtRoiLabelSet.toSetImSize(m_GetImage.size());
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toSetImage(m_GetImage);

    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_PB_SaveCurent_clicked()
{
    //
    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    //
    if (m_CurentIndex >= 0 && m_CurentIndex < ui->LW_FileList->count())
    {
        if (!m_YtRoiLabelSet.m_SetLabeset.isEmpty() || m_YtRoiLabelSet.isBackgroundSample())
        {
            ui->LW_FileList->item(m_CurentIndex)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));
        }
        else
        {
            ui->LW_FileList->item(m_CurentIndex)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/未标注.png"));
        }
    }
}

void YoloeLabelForm::on_PB_Reload_clicked()
{
    m_YtRoiLabelSet.toLoadJson(m_WorkPath);
    for (int i = m_YtRoiLabelSet.m_SetLabeset.size() - 1; i >= 0; --i)
    {
        const LabelSet &label = m_YtRoiLabelSet.m_SetLabeset.at(i);
        if (!label.isTrainingIgnored() && label.Name.compare(QStringLiteral("background"), Qt::CaseInsensitive) == 0)
        {
            m_YtRoiLabelSet.m_SampleRole = YtRoiLabelSet::SampleRole::Background;
            m_YtRoiLabelSet.toReMoveIndex(i);
        }
    }

    m_IsaddState = true;

    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toRemoveAllRoi();
    ui->YtLabelRoi->toSetIgnoreMaskVisible(ui->ckB_ShowMask->isChecked());
    m_TxtOverPlayShow.toClearData();
    ui->LW_SetLab->setRowCount(m_YtRoiLabelSet.m_SetLabeset.size());

    if (ui->ckB_ShowLable->isChecked())
    {
        for (int i = 0; i < m_YtRoiLabelSet.m_SetLabeset.size(); i++)
        {

            QVector<double> getdata = m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiData();
            QVector<double> displayData = toDisplayPointData(m_YtRoiLabelSet.m_SetLabeset[i]);
            if (displayData.isEmpty())
            {
                displayData = getdata;
            }
            const bool isIgnore = m_YtRoiLabelSet.m_SetLabeset[i].isTrainingIgnored();
            const bool isIgnoreSubtract = m_YtRoiLabelSet.m_SetLabeset[i].isIgnoreSubtract();
            const QString canvasLabel =
                isIgnore ? (isIgnoreSubtract ? kYoloeIgnoreSubtractCanvasLabel : kYoloeIgnoreCanvasLabel)
                         : m_YtRoiLabelSet.m_SetLabeset[i].toGetName();
            ui->YtLabelRoi->toAppItemLabel(canvasLabel, m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiType(), displayData);
            //
            ui->LW_SetLab->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));

            ui->LW_SetLab->setItem(i,
                                   1,
                                   new QTableWidgetItem(isIgnore ? (isIgnoreSubtract ? kYoloeIgnoreSubtractDisplayLabel
                                                                                     : kYoloeIgnoreDisplayLabel)
                                                                 : m_YtRoiLabelSet.m_SetLabeset[i].toGetName()));
            if (isIgnore)
            {
                ui->LW_SetLab->item(i, 1)->setBackground(isIgnoreSubtract ? kYoloeIgnoreSubtractColor
                                                                          : kYoloeIgnoreColor);
            }
            else
            {
                const int colorIndex =
                    m_YtYoloSetPro.toGetLabelName().indexOf(m_YtRoiLabelSet.m_SetLabeset[i].toGetName());
                if (colorIndex >= 0 && colorIndex < m_YtYoloSetPro.toGetColorDefine().size())
                {
                    ui->LW_SetLab->item(i, 1)->setBackground(m_YtYoloSetPro.toGetColorDefine().at(colorIndex));
                }
            }

            ui->LW_SetLab->setItem(i, 2, new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].shape_type));

            ui->LW_SetLab->setItem(i, 3, new QTableWidgetItem(toRoiText(displayData)));
            //
            for (int m = 0; m < 4; m++)
            {
                ui->LW_SetLab->item(i, m)->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // ��ʾ���������Ľ��
    if (ui->ckB_ShowResu->isChecked())
    {
        QString tImgPath = m_WorkPath + "/" + m_YtRoiLabelSet.m_imagePath;
        int getindex = tImgPath.lastIndexOf(".");
        if (getindex < 0)
        {
            return;
        }

        QFile tsetfile;
        m_Txtfilename = tImgPath.left(getindex) + ".txt";
        tsetfile.setFileName(m_Txtfilename);
        if (tsetfile.exists())
        {
            for (QString tstr : m_YtYoloSetPro.m_NameList)
            {
                qDebug() << tstr;
            }

            // ����Ŀ��ʶ��
            m_YtRoiLabelSet.toLoadDetcTxt(m_Txtfilename, m_GetImage.size(), m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenDetcOveplay(m_TxtOverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
            ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");
            ui->YtLabelRoi->toUpdateShow();
        }
    }

    m_IsaddState = false;
    toSyncHighlightedRoiFromTable(ui);
}
void YoloeLabelForm::on_CB_CurrentLabel_activated(int index)
{
    Q_UNUSED(index);
    ui->CB_SetLabeMode->setCurrentIndex(0);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_CB_ROIType_activated(int index)
{
    Q_UNUSED(index);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_CB_SetLabeMode_activated(int index)
{
    Q_UNUSED(index);
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_PB_DrawIgnore_clicked()
{
    if (m_GetImage.isNull() || m_GetImage.width() <= 0 || m_GetImage.height() <= 0)
    {
        QMessageBox::warning(this, QString(u8"警告"), QString(u8"请先选择需要标注的图像"));
        return;
    }

    if (ui->PB_Eraser->isChecked())
    {
        ui->PB_Eraser->setChecked(false);
    }

    QVector<LabelSet> labels;
    labels.reserve(m_YtRoiLabelSet.m_SetLabeset.size() + 1);

    LabelSet fullImageIgnore;
    const QVector<double> fullImagePolygon = {
        0.0,
        0.0,
        m_GetImage.width() - 0.5,
        0.0,
        m_GetImage.width() - 0.5,
        m_GetImage.height() - 0.5,
        0.0,
        m_GetImage.height() - 0.5,
    };
    fullImageIgnore.toInitProData(QString(), LpolygonROI, fullImagePolygon);
    fullImageIgnore.trainingRole = LabelSet::TrainingRole::Ignore;
    fullImageIgnore.ignoreOperation = LabelSet::IgnoreOperation::Add;
    labels.append(fullImageIgnore);

    for (const LabelSet &label : std::as_const(m_YtRoiLabelSet.m_SetLabeset))
    {
        if (label.isTrainingIgnored() && !label.isIgnoreSubtract())
        {
            continue;
        }
        labels.append(label);
    }

    m_YtRoiLabelSet.m_SetLabeset = labels;
    ui->ckB_ShowMask->setChecked(true);
    on_PB_SaveCurent_clicked();
    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_PB_Eraser_toggled(bool checked)
{
    if (checked)
    {
        ui->ckB_ShowMask->setChecked(true);
    }
    toSyncSam2PromptMode();
}

void YoloeLabelForm::on_spinBox_EraserSize_valueChanged(int value)
{
    ui->YtLabelRoi->toSetEraserDiameter(value);
}

void YoloeLabelForm::on_ckB_ShowMask_stateChanged(int state)
{
    ui->YtLabelRoi->toSetIgnoreMaskVisible(state == Qt::Checked);
}

void YoloeLabelForm::toApplyEraserStroke(const QVector<QPointF> &points, qreal diameter)
{
    if (points.isEmpty() || m_GetImage.isNull())
    {
        return;
    }

    const cv::Size imageSize(m_GetImage.width(), m_GetImage.height());
    cv::Mat strokeMask(imageSize, CV_8UC1, cv::Scalar(0));
    const int brushDiameter = qMax(2, qRound(diameter));
    const int brushRadius = qMax(1, brushDiameter / 2);
    for (int i = 0; i < points.size(); ++i)
    {
        const cv::Point current(qRound(points.at(i).x()), qRound(points.at(i).y()));
        cv::circle(strokeMask, current, brushRadius, cv::Scalar(255), cv::FILLED, cv::LINE_8);
        if (i > 0)
        {
            const cv::Point previous(qRound(points.at(i - 1).x()), qRound(points.at(i - 1).y()));
            cv::line(strokeMask, previous, current, cv::Scalar(255), brushDiameter, cv::LINE_8);
        }
    }

    cv::Mat ignoreAddMask(imageSize, CV_8UC1, cv::Scalar(0));
    cv::Mat existingSubtractMask(imageSize, CV_8UC1, cv::Scalar(0));
    int existingSubtractCount = 0;
    for (LabelSet label : std::as_const(m_YtRoiLabelSet.m_SetLabeset))
    {
        if (label.isTrainingIgnored() && !label.isIgnoreSubtract())
        {
            fillLabelMask(ignoreAddMask, label, 255);
        }
        else if (label.isIgnoreSubtract())
        {
            fillLabelMask(existingSubtractMask, label, 255);
            ++existingSubtractCount;
        }
    }
    cv::Mat effectiveIgnore;
    cv::Mat inverseSubtractMask;
    cv::bitwise_not(existingSubtractMask, inverseSubtractMask);
    cv::bitwise_and(ignoreAddMask, inverseSubtractMask, effectiveIgnore);

    QVector<LabelSet> nextLabels;
    bool changed = false;
    bool targetHoleRejected = false;
    for (LabelSet label : std::as_const(m_YtRoiLabelSet.m_SetLabeset))
    {
        if (label.isTrainingIgnored())
        {
            if (!label.isIgnoreSubtract())
            {
                nextLabels.append(label);
            }
            continue;
        }

        cv::Mat targetMask(imageSize, CV_8UC1, cv::Scalar(0));
        fillLabelMask(targetMask, label, 255);
        cv::Mat intersection;
        cv::bitwise_and(targetMask, strokeMask, intersection);
        if (cv::countNonZero(intersection) == 0)
        {
            nextLabels.append(label);
            continue;
        }

        cv::Mat remaining;
        cv::bitwise_not(strokeMask, remaining);
        cv::bitwise_and(targetMask, remaining, remaining);
        bool hasHole = false;
        const QVector<QVector<double>> contours = contoursToRoiData(remaining, &hasHole);
        if (hasHole)
        {
            targetHoleRejected = true;
            nextLabels.append(label);
            continue;
        }
        for (const QVector<double> &roiData : contours)
        {
            LabelSet replacement;
            replacement.toInitProData(label.Name, LpolygonROI, roiData);
            replacement.trainingRole = LabelSet::TrainingRole::Target;
            nextLabels.append(replacement);
        }
        changed = true;
    }

    cv::Mat erasedIgnore;
    cv::bitwise_and(strokeMask, effectiveIgnore, erasedIgnore);
    cv::Mat mergedSubtractMask;
    cv::bitwise_or(existingSubtractMask, erasedIgnore, mergedSubtractMask);
    cv::bitwise_and(mergedSubtractMask, ignoreAddMask, mergedSubtractMask);
    const bool ignoreChanged = cv::countNonZero(erasedIgnore) > 0;
    const QVector<QVector<double>> subtractContours = contoursToRoiData(mergedSubtractMask);
    const bool subtractRepresentationChanged = existingSubtractCount != subtractContours.size();
    for (const QVector<double> &roiData : subtractContours)
    {
        LabelSet subtractLabel;
        subtractLabel.toInitProData(QString(), LpolygonROI, roiData);
        subtractLabel.trainingRole = LabelSet::TrainingRole::Ignore;
        subtractLabel.ignoreOperation = LabelSet::IgnoreOperation::Subtract;
        nextLabels.append(subtractLabel);
    }
    changed = changed || ignoreChanged || subtractRepresentationChanged;

    if (targetHoleRejected)
    {
        QMessageBox::information(this, QString(u8"提示"), QString(u8"普通目标不支持内部孔洞，相关目标未被修改"));
    }
    if (!changed)
    {
        return;
    }

    m_YtRoiLabelSet.m_SetLabeset = nextLabels;
    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_PB_ReMoveCurentLab_clicked()
{
    int index = ui->LW_SetLab->currentRow();
    if (index < 0)
    {
        return;
    }
    //
    ui->YtLabelRoi->toRemoveRoiByKey(ui->LW_SetLab->item(index, 0)->text());
    ui->LW_SetLab->removeRow(index);
    for (int i = 0; i < ui->LW_SetLab->rowCount(); ++i)
    {
        QTableWidgetItem *keyItem = ui->LW_SetLab->item(i, 0);
        if (keyItem)
        {
            keyItem->setText(QString::number(i + 1));
        }
    }
    toSyncHighlightedRoiFromTable(ui);
    if (ui->CB_SaveROIType->currentIndex() == 0)
    {
        m_YtRoiLabelSet.toReMoveIndex(index);
        on_PB_SaveCurent_clicked();
    }
}

void YoloeLabelForm::on_PB_ClearLab_clicked()
{
    m_IsaddState = true;
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toRemoveAllRoi();
    m_YtRoiLabelSet.toClearData();
    if (ui->CB_SaveROIType->currentIndex() == 0)
    {
        on_PB_SaveCurent_clicked();
    }
    m_IsaddState = false;
}

void YoloeLabelForm::on_PB_StartIm_clicked()
{
    if (ui->LW_FileList->count() < 1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(0);
}

void YoloeLabelForm::on_PB_EndIm_clicked()
{
    if (ui->LW_FileList->count() < 1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(ui->LW_FileList->count() - 1);
}

void YoloeLabelForm::on_PB_LastIm_clicked()
{
    if (ui->LW_FileList->count() < 1)
    {
        return;
    }
    int getlis = ui->LW_FileList->currentRow();
    if (getlis < 0)
    {
        getlis = 0;
    }
    else
    {
        if (getlis >= 1)
        {
            getlis -= 1;
        }
    }
    ui->LW_FileList->setCurrentRow(getlis);
}

void YoloeLabelForm::on_PB_NextIm_clicked()
{
    if (ui->LW_FileList->count() < 1)
    {
        return;
    }
    int getlis = ui->LW_FileList->currentRow();
    if (getlis < 0)
    {
        getlis = 0;
    }
    else
    {
        if (getlis < (ui->LW_FileList->count() - 1))
        {
            getlis += 1;
        }
    }
    ui->LW_FileList->setCurrentRow(getlis);
}

void YoloeLabelForm::on_PB_XLabe_clicked()
{
    if (!QFile::exists(YtYoloDefine::toGetPythonPath() + "/python.exe"))
    {
        QMessageBox::warning(0, u8"警告", u8"Python 路径异常", u8"确定");
        return;
    }
    if (m_SetProcess == nullptr)
    {
        m_SetProcess = new QProcess(this); // �������б�׼���ݣ����ݣ�
        m_SetProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_SetProcess->start("cmd.exe");
    }
    //
    QString cmdDir = YtYoloDefine::toGetPythonPath().left(1) + ":\r\n";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    cmdDir = QString("cd %1/X-AnyLabeling-main\r\n").arg(YtYoloDefine::toGetPythonPath());
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    QString SelfDefine = YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName;
    for (int i = 0; i < m_YtYoloSetPro.toGetLabelName().size(); i++)
    {
        if (i == 0)
        {
            SelfDefine.append(QString(" --labels %1").arg(m_YtYoloSetPro.toGetLabelName().at(i)));
        }
        else
        {
            SelfDefine.append(QString(",%1").arg(m_YtYoloSetPro.toGetLabelName().at(i)));
        }
    }

    cmdDir = QString("%1/python.exe %2/X-AnyLabeling-main/anylabeling/app.py %3\r\n")
                 .arg(YtYoloDefine::toGetPythonPath())
                 .arg(YtYoloDefine::toGetPythonPath())
                 .arg(SelfDefine);
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.toLocal8Bit().length());
}

void YoloeLabelForm::on_LW_SetLab_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= m_YtRoiLabelSet.m_SetLabeset.size())
    {
        return;
    }
    if (m_YtRoiLabelSet.m_SetLabeset.at(row).isTrainingIgnored())
    {
        QMessageBox::information(this, QString(u8"提示"), QString(u8"训练忽略区不能重命名"));
        return;
    }
    // �����޸ı�ǩ
    ReNameDlg tDlg;

    tDlg.setWindowFlags(Qt::SubWindow);
    tDlg.toInitData(ui->LW_SetLab->item(row, 1)->text(), m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);

    if (QDialog::Accepted == tDlg.exec())
    {

        LabelSet LabelName = m_YtRoiLabelSet.m_SetLabeset.at(row);

        if (LabelName.Name == tDlg.toGetCurentName())
        {
            return;
        }
        LabelName.Name = tDlg.toGetCurentName();

        m_YtRoiLabelSet.toModify(LabelName, row, true);

        on_PB_SaveCurent_clicked();
        on_PB_Reload_clicked();
    }
}

void YoloeLabelForm::on_PB_ViewDir_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName));
}

void YoloeLabelForm::toInitMenuData()
{

    QAction *pAc1 = new QAction(QString(u8"批量设置标签"));
    QAction *pAc2 = new QAction(QString(u8"批量移除标签"));
    m_pRightMenu.addAction(pAc1);
    m_pRightMenu.addAction(pAc2);
    connect(&m_pRightMenu, &QMenu::triggered, this, &YoloeLabelForm::slot_menuTrigger);
    ui->LW_DataSheet->setContextMenuPolicy(Qt::CustomContextMenu);
}

void YoloeLabelForm::on_LW_DataSheet_customContextMenuRequested(const QPoint &pos)
{
    if (ui->LW_DataSheet->currentRow() < 0)
    {
        return;
    }
    m_pRightMenu.exec(QCursor::pos());
}

void YoloeLabelForm::slot_menuTrigger(QAction *action)
{
    QString SetNewType;

    if (action->text() == QString(u8"批量设置标签"))
    {
        NameSelectDlg tDlg;
        tDlg.toInitLabeLis(m_YtYoloSetPro.m_NameList);
        tDlg.setWindowFlags(Qt::SubWindow);
        if (QDialog::Accepted == tDlg.exec())
        {
            SetNewType = tDlg.toGetSetName();
        }
        else
        {
            return;
        }
    }
    else if (action->text() == QString(u8"批量移除标签"))
    {
        SetNewType.clear();
    }
    m_WorkPath = YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName + "/" + ui->LW_DataSheet->currentItem()->text();
    //
    QStringList filtelis;
    filtelis << "*.bmp"
             << "*.jpg"
             << "*.jpeg"
             << "*.png"
             << "*.tiff";
    QFileInfoList CurentFiles = YtYoloDefine::toGetPathFileInfo(m_WorkPath, filtelis);
    //
    //
    QImage temim;
    QString Destpfilename;
    LabelSet temlabeset;
    CMvRect temsetdata;
    for (int i = 0; i < CurentFiles.size(); i++)
    {

        int getindex = CurentFiles.at(i).fileName().lastIndexOf(".");
        Destpfilename = m_WorkPath + "/" + CurentFiles.at(i).fileName().left(getindex) + ".json";
        //

        if (SetNewType.isEmpty())
        {
            QFile::remove(Destpfilename);
            if (i < ui->LW_FileList->count())
            {
                ui->LW_FileList->item(i)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/未标注.png"));
            }
        }
        else
        {
            temim.load(CurentFiles.at(i).absoluteFilePath());
            qDebug() << "PPPP" << CurentFiles.at(i).absoluteFilePath() << CurentFiles.at(i).path();
            m_YtRoiLabelSet.toSetFileName(CurentFiles.at(i).fileName());
            m_YtRoiLabelSet.toSetImSize(temim.size());
            m_YtRoiLabelSet.m_SetLabeset.clear();
            temsetdata = CMvRect(0, 0, temim.width() - 1, temim.height() - 1);
            temlabeset.toInitProData(SetNewType, LrectangleROI, temsetdata.Data());
            m_YtRoiLabelSet.m_SetLabeset.append(temlabeset);
            m_YtRoiLabelSet.toSaveJson(CurentFiles.at(i).path());
            if (i < ui->LW_FileList->count())
            {
                ui->LW_FileList->item(i)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));
            }
        }
    }
    QMessageBox::about(0, u8"提示", u8"批量标注完成");
}

void YoloeLabelForm::on_ckB_ShowResu_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_ckB_ShowLable_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_PB_SetBackgroud_clicked()
{
    if (ui->LW_DataSheet->currentItem() == nullptr || ui->LW_FileList->currentItem() == nullptr)
    {
        return;
    }

    QVector<LabelSet> ignoreLabels;
    for (const LabelSet &label : std::as_const(m_YtRoiLabelSet.m_SetLabeset))
    {
        if (label.isTrainingIgnored())
        {
            ignoreLabels.append(label);
        }
    }
    m_YtRoiLabelSet.toClearData();
    for (const LabelSet &label : std::as_const(ignoreLabels))
    {
        m_YtRoiLabelSet.toAppendData(label);
    }
    m_YtRoiLabelSet.m_SampleRole = YtRoiLabelSet::SampleRole::Background;
    m_YtRoiLabelSet.toSetFileName(ui->LW_FileList->currentItem()->text());
    m_YtRoiLabelSet.toSetImSize(m_GetImage.size());
    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    ui->LW_FileList->currentItem()->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));
    on_PB_Reload_clicked();
}

void YoloeLabelForm::on_PB_TBConv_clicked()
{

    QString filename = m_WorkPath + "/" + m_CurentFileName;
    //
    // ��ʾͼ��
    QImage temim = m_GetImage.mirrored(false, true);
    m_GetImage = temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}

void YoloeLabelForm::on_PB_LRConv_clicked()
{
    QString filename = m_WorkPath + "/" + m_CurentFileName;
    //
    // ��ʾͼ��
    QImage temim = m_GetImage.mirrored(true, false);
    m_GetImage = temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}

void YoloeLabelForm::on_comboBox_currentIndexChanged(int index)
{
    QListWidgetItem *item = new QListWidgetItem;
    on_LW_DataSheet_itemClicked(item);
}

void YoloeLabelForm::on_PB_AddAnno_clicked()
{
    if (ui->LW_FileList->currentItem() == nullptr)
    {
        return;
    }
    if (QDialog::Rejected == QMessageBox::question(0, u8"提示", u8"确定删除当前图片？", u8"是", u8"否"))
    {
        // int currentListIndex = ui->LW_FileList->row(ui->LW_FileList->currentItem());
        QModelIndex tShowIndex = ui->LW_FileList->currentIndex();
        QString deletFileName = m_WorkPath + "/" + ui->LW_FileList->currentItem()->text();
        QString BaseName = deletFileName.split(".").first();

        QFile().remove(deletFileName);
        if (QFile().exists(BaseName + ".txt"))
        {
            QFile().remove(BaseName + ".txt");
            qDebug() << "removed: " << BaseName + ".txt";
        }
        if (QFile().exists(BaseName + ".json"))
        {
            QFile().remove(BaseName + ".json");
            qDebug() << "removed: " << BaseName + ".json";
        }

        // ˢ����ʾ
        if (ui->LW_DataSheet->currentItem() == nullptr)
        {
            return;
        }
        on_LW_DataSheet_itemClicked(ui->LW_DataSheet->currentItem());
        ui->LW_FileList->setCurrentIndex(tShowIndex);
    }
}
