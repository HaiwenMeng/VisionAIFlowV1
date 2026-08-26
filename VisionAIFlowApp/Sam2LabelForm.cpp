#include "Sam2LabelForm.h"
#include "ui_Sam2LabelForm.h"
#include "setnamedialog.h"
#include <QDir>
#include "setlabelname.h"
#include "filecopysetdlg.h"
#include "renamedlg.h"
#include "nameselectdlg.h"
#include "ytframelesswidgetlib.h"
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

namespace
{
QVector<double> toRectStorageData(const QVector<double> &roiData)
{
    if (roiData.size() >= 8)
    {
        qreal minX = roiData[0];
        qreal minY = roiData[1];
        qreal maxX = roiData[0];
        qreal maxY = roiData[1];
        for (int i = 0; i + 1 < roiData.size(); i += 2)
        {
            minX = qMin(minX, roiData[i]);
            minY = qMin(minY, roiData[i + 1]);
            maxX = qMax(maxX, roiData[i]);
            maxY = qMax(maxY, roiData[i + 1]);
        }

        CMvRect rectData(minX, minY, qMax<qreal>(0.0, maxX - minX), qMax<qreal>(0.0, maxY - minY));
        return rectData.Data();
    }

    return roiData;
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
    return QFileInfo::exists(dirPath + "/sam2_image_encode.fp16.trt")
            && QFileInfo::exists(dirPath + "/sam2_decode.fp16.trt");
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
    paths << dirPath
          << (dirPath + QStringLiteral("/sam2_image_encode.fp16.trt"))
          << (dirPath + QStringLiteral("/sam2_decode.fp16.trt"));

    for (const QString &path : paths)
    {
        if (toStringHasNonAscii(path))
        {
            return QString(u8"路径可能含有中文: %1. ").arg(path);
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


void applySam2CommercialUi(Ui::Sam2LabelForm *ui, QWidget *root)
{
    if (!ui || !root)
    {
        return;
    }

    const QList<QPushButton *> dangerButtons = {
        ui->PB_RemoveLab,
        ui->PB_AddAnno,
        ui->PB_ClearLab,
        ui->PB_ReMoveCurentLab,
        ui->PB_SunDataSet
    };

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
//void toUpdateSamStatusIndicator(Ui::Sam2LabelForm *ui, bool ready)
//{
//    if (!ui || !ui->label_10)
//    {
//        return;
//    }

//    ui->label_10->setText(QString());
//    ui->label_10->setFixedSize(14, 14);
//    ui->label_10->setToolTip(ready ? QString(u8"模型初始化成功") : QString(u8"模型初始化失败"));
//    ui->label_10->setStyleSheet(QStringLiteral("QLabel { background-color: %1; border-radius: 7px; border: 1px solid rgba(0, 0, 0, 90); }")
//                                 .arg(ready ? QStringLiteral("#19a84a") : QStringLiteral("#d93025")));
//}

void toSyncHighlightedRoiFromTable(Ui::Sam2LabelForm *ui)
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
    dialog->setLabelText(QStringLiteral("Initializing SAM2 model..."));
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
}

Sam2LabelForm::Sam2LabelForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Sam2LabelForm)
{
    ui->setupUi(this);
    applySam2CommercialUi(ui, this);
    QShortcut *lastImageShortcut = new QShortcut(QKeySequence(Qt::Key_A), this);
    lastImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(lastImageShortcut, &QShortcut::activated, this, &Sam2LabelForm::on_PB_LastIm_clicked);

    QShortcut *nextImageShortcut = new QShortcut(QKeySequence(Qt::Key_D), this);
    nextImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextImageShortcut, &QShortcut::activated, this, &Sam2LabelForm::on_PB_NextIm_clicked);

    ui->stackedWidget->setCurrentIndex(0);

    // 对齐 CommonSetForm 的三栏布局比例：左侧目录区适中，中间画布优先扩展，右侧标签区固定较窄
    ui->gridLayout_7->setColumnStretch(0, 1);
    ui->gridLayout_7->setColumnStretch(1, 1);
    ui->gridLayout_7->setColumnStretch(2, 1);
    ui->gridLayout_7->setColumnStretch(3, 10);
    ui->gridLayout_7->setColumnStretch(4, 3);
//    toUpdateSamStatusIndicator(ui, false);
    toInitSam2Inference();
    ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");

    //
    connect(ui->YtLabelRoi,&Sam2LabelCanvasCompat::SigGetDataUpdate,this,&Sam2LabelForm::toGetDataUpdate);
    connect(ui->YtLabelRoi,&Sam2LabelCanvasCompat::ROIChange,this,&Sam2LabelForm::Slot_ROIChange);
    connect(ui->YtLabelRoi,&Sam2LabelCanvasCompat::SamRuntimeError,this,&Sam2LabelForm::toHandleSam2RuntimeError);
    connect(ui->LW_SetLab, &QTableWidget::itemSelectionChanged, this, [this]() {
        toSyncHighlightedRoiFromTable(ui);
    });
    //
    ui->LW_SetLab->verticalHeader()->setVisible(false);   //隐藏垂直表头
    //ui->LW_SetLab->horizontalHeader()->setStretchLastSection(true);
    ui->LW_SetLab->setEditTriggers(QAbstractItemView::NoEditTriggers);  //不可编辑
    ui->LW_SetLab->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->LW_SetLab->setSelectionBehavior(QAbstractItemView::SelectRows);   //选择一行
    ui->LW_SetLab->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    toInitMenuData();
    connect(ui->LW_DataSheet,&QListWidget::itemChanged,this,&Sam2LabelForm::slotDataSheetItemChanged);


}

Sam2LabelForm::~Sam2LabelForm()
{
    delete ui;

    if(m_SetProcess)
    {
        m_SetProcess->close();
        delete m_SetProcess;
        m_SetProcess=nullptr;
    }
}

void Sam2LabelForm::toInitSam2Inference()
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
            failureMessage = QStringLiteral("SAM2 model init exception from %1: %2").arg(dirPath, QString::fromLocal8Bit(ex.what()));
            qWarning() << failureMessage;
        }
        catch (...)
        {
            failureMessage = QStringLiteral("SAM2 model init failed with unknown exception from: %1.").arg(dirPath);
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
            failureMessage = QStringLiteral("SAM2 model init failed from: %1.").arg(dirPath);
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
                failureMessage = QStringLiteral("SAM2 model init failed. Please check TensorRT runtime, CUDA, and engine compatibility.");
            }
            else
            {
                failureMessage = QStringLiteral("SAM2 model files are missing. Please generate sam2_image_encode.fp16.trt and sam2_decode.fp16.trt first.");
            }
        }
        qWarning() << toSamTypeName() << "model unavailable, candidates:" << candidateDirs << "error:" << failureMessage;
        toHandleSam2Unavailable(failureMessage, false);
//        toUpdateSamStatusIndicator(ui, false);
    }
}

void Sam2LabelForm::toHandleSam2Unavailable(const QString &errorMessage, bool forceMessage)
{
    QString message = errorMessage;
    if (message.isEmpty())
    {
        message = QStringLiteral("SAM2 is unavailable. Automatic prompts are disabled and manual rectangle mode is enabled.");
    }

    m_Sam2Available = false;
    qWarning() << "SAM2 unavailable:" << message;

    if (ui)
    {
        if (ui->CB_SetLabeMode)
        {
            ui->CB_SetLabeMode->setCurrentIndex(1);
            ui->CB_SetLabeMode->setEnabled(false);
        }
        if (ui->CB_ROIType)
        {
            ui->CB_ROIType->setCurrentIndex(0);
        }
        toSyncSam2PromptMode();
    }

    if ((forceMessage || !m_Sam2UnavailableMessageShown) && ui)
    {
        QMessageBox::warning(this, QStringLiteral("Warning"), message);
        m_Sam2UnavailableMessageShown = true;
    }
}

void Sam2LabelForm::toHandleSam2RuntimeError(const QString &errorMessage)
{
    QString message = errorMessage;
    if (message.isEmpty())
    {
        message = QStringLiteral("SAM2 inference failed.");
    }

    qWarning() << "SAM2 runtime error:" << message;
    if (ui)
    {
        QMessageBox::warning(this, QStringLiteral("Warning"), message);
    }
}
void Sam2LabelForm::toInitProShowLabel()
{
    toInitSam2Inference();

    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
    toInitLabelSet();
    toInitPath();
    //
    m_CurentIndex=-1;
    m_CurentFileName="";
    ui->LW_FileList->clear();
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toSetImage(QImage());
    ui->YtLabelRoi->toRemoveAllRoi();

}




void Sam2LabelForm::toInitLabelSet()
{
    ui->LW_LabelSet->clear();
    ui->CB_CurrentLabel->clear();
    QStringList temNames;
    for(int i=0;i<m_YtYoloSetPro.toGetLabelName().size();i++)
    {
        QListWidgetItem *item = new QListWidgetItem(m_YtYoloSetPro.toGetLabelName().at(i));
        //
        QImage temim=QImage(40,40,QImage::Format_RGB888);
        temim.fill(m_YtYoloSetPro.toGetColorDefine().at(i));
        item->setIcon(QPixmap::fromImage(temim));

        ui->LW_LabelSet->addItem(item);

        ui->CB_CurrentLabel->addItem(QPixmap::fromImage(temim),m_YtYoloSetPro.toGetLabelName().at(i));
        temNames<<m_YtYoloSetPro.toGetLabelName().at(i);
    }
    ui->CB_CurrentLabel->setCurrentIndex(0);
    ui->CB_ROIType->setCurrentIndex(0);
    //
    ui->YtLabelRoi->toSetLabelNames(temNames,m_YtYoloSetPro.toGetColorDefine());
    toSyncSam2PromptMode();
}

void Sam2LabelForm::toSyncSam2PromptMode()
{
    if(!ui || !ui->YtLabelRoi || !ui->CB_ROIType || !ui->CB_SetLabeMode || !ui->CB_CurrentLabel)
    {
        return;
    }

    const bool samReady = m_Sam2Available && ui->YtLabelRoi->isSamReady();
    if (!samReady)
    {
        if(ui->CB_SetLabeMode->currentIndex() != 1)
        {
            ui->CB_SetLabeMode->setCurrentIndex(1);
        }
        ui->CB_SetLabeMode->setEnabled(false);
    }
    else
    {
        ui->CB_SetLabeMode->setEnabled(true);
    }

    const bool manualBoxMode = !samReady || ui->CB_SetLabeMode->currentIndex() == 1;
    int roiType = LrectangleROI;

    if(manualBoxMode)
    {
        if(ui->CB_ROIType->currentIndex() != 0)
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
    if(ui->CB_SetLabeMode->currentIndex()<0)
    {
        ui->YtLabelRoi->toSetRoiDefaltName("");
    }
}

void Sam2LabelForm::toAddDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState)
{
    QListWidgetItem *item=new QListWidgetItem(dataSheetName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checkState);
    ui->LW_DataSheet->addItem(item);
}

void Sam2LabelForm::toSaveDataSheetCheckState()
{
    if(m_ProcessName.isEmpty())
    {
        return;
    }
    QStringList uncheckedDataSheetList;
    for(int i=0;i<ui->LW_DataSheet->count();i++)
    {
        QListWidgetItem *item=ui->LW_DataSheet->item(i);
        if(item && item->checkState()!=Qt::Checked && !uncheckedDataSheetList.contains(item->text()))
        {
            uncheckedDataSheetList.append(item->text());
        }
    }
    if(m_YtYoloSetPro.m_UncheckedDataSheetList==uncheckedDataSheetList)
    {
        return;
    }
    m_YtYoloSetPro.m_UncheckedDataSheetList=uncheckedDataSheetList;
    m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
}

void Sam2LabelForm::toInitPath()
{
    QSignalBlocker blocker(ui->LW_DataSheet);
    ui->LW_DataSheet->clear();
    QDir temdir(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
    QFileInfoList temlis=temdir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(int i=0;i<temlis.size();i++)
    {
        Qt::CheckState checkState=m_YtYoloSetPro.m_UncheckedDataSheetList.contains(temlis.at(i).fileName()) ? Qt::Unchecked : Qt::Checked;
        toAddDataSheetItem(temlis.at(i).fileName(),checkState);

    }
    toSaveDataSheetCheckState();
}

void Sam2LabelForm::slotDataSheetItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    toSaveDataSheetCheckState();
}
void Sam2LabelForm::toGetDataUpdate(QString SetKey, QString LableName, int type)
{
    Q_UNUSED(type);
    if(m_IsaddState)
    {
        return;
    }
    int rowcount=ui->LW_SetLab->rowCount();
    int getindex=m_YtYoloSetPro.toGetLabelName().lastIndexOf(LableName);
    if(getindex<0)
    {
        return;
    }
    LabelSet temppset;
    for(int i=0;i<(rowcount+1);i++)
    {
        if(SetKey.toInt()==(i+1))
        {
            ui->LW_SetLab->insertRow(i);
            ui->LW_SetLab->setItem(i,0,new QTableWidgetItem(SetKey));

            ui->LW_SetLab->setItem(i,1,new QTableWidgetItem(LableName));
            ui->LW_SetLab->item(i,1)->setBackgroundColor(m_YtYoloSetPro.toGetColorDefine().at(getindex));

            QVector<double> getData=ui->YtLabelRoi->getROI(SetKey);
            QVector<double> saveData = toRectStorageData(getData);
            temppset.toInitProData(LableName,LrectangleROI,saveData);
            ui->LW_SetLab->setItem(i,2,new QTableWidgetItem(temppset.shape_type));


            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(toRoiText(getData)));
            //
            m_YtRoiLabelSet.toInserData(temppset,i);
            for(int m=0;m<4;m++)
            {
                ui->LW_SetLab->item(i,m)->setTextAlignment(Qt::AlignCenter);
            }
        }

    }
    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        on_PB_SaveCurent_clicked();
    }



}

void Sam2LabelForm::Slot_ROIChange(QVector<double> tdata, QString &key, int type)
{
    Q_UNUSED(type);
    if(m_IsaddState)
    {
        return;
    }
    int rowcount=ui->LW_SetLab->rowCount();
    LabelSet temppset;
    int getindex;
    for(int i=0;i<rowcount;i++)
    {
        if(key.toInt()==ui->LW_SetLab->item(i,0)->text().toInt())
        {
            getindex=i;
            QVector<double> saveData = toRectStorageData(tdata);
            temppset.toInitProData(ui->LW_SetLab->item(i,1)->text(),LrectangleROI,saveData);
            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(toRoiText(tdata)));
            //
            break;
        }

    }

    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        m_YtRoiLabelSet.toModify(temppset,getindex,false);
        on_PB_SaveCurent_clicked();
    }

}

void Sam2LabelForm::on_PB_AddDataSet_clicked()
{

    SetLabelName tDlg;
    tDlg.setWindowFlags(Qt::SubWindow);
    if(QDialog::Accepted==tDlg.exec())
    {
        QColor getColor;
        QString getLable;
        tDlg.toGetData(getLable,getColor);
        if(getLable.isEmpty() || m_YtYoloSetPro.toGetLabelName().contains(getLable))
        {
            QMessageBox::warning(0,u8"警告",u8"名称已存在或为空",u8"确定");

            return;
        }
        m_YtYoloSetPro.toAppendLabelInfo(getLable,getColor);

        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        //qDebug()<<"label data save path:" <<m_WorkPath;
        toInitLabelSet();
    }


}

void Sam2LabelForm::on_PB_SunDataSet_clicked()
{
    int index=ui->LW_LabelSet->currentRow();

    if(index>=0)
    {
        QListWidgetItem *temite=ui->LW_LabelSet->takeItem(index);
        delete temite;
        m_YtYoloSetPro.m_ClorDefine.removeAt(index);
        m_YtYoloSetPro.m_NameList.removeAt(index);
        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        toInitLabelSet();

    }
}

void Sam2LabelForm::on_LW_LabelSet_itemDoubleClicked(QListWidgetItem *item)
{
    int getindex=ui->LW_LabelSet->currentRow();
    if(getindex<0)
    {
        return;
    }
    SetLabelName tDlg;
    tDlg.toSetData( m_YtYoloSetPro.toGetLabelName().at(getindex), m_YtYoloSetPro.toGetColorDefine().at(getindex));
    if(QDialog::Accepted==tDlg.exec())
    {
        QColor getColor;
        QString getLable;
        tDlg.toGetData(getLable,getColor);
        if(getLable.isEmpty())
        {
            QMessageBox::warning(0, u8"警告",u8"名称已存在或为空",u8"确定");
            return;
        }
        m_YtYoloSetPro.toModifyInfo(getLable,getColor,getindex);
        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        toInitLabelSet();
    }
}

void Sam2LabelForm::on_PB_AddLab_clicked()
{
    int getindex=ui->LW_DataSheet->currentRow();


    FileCopySetDlg tDlg;
    tDlg.setWindowFlags(Qt::SubWindow);
    if(getindex>=0)
    {
        tDlg.toSetTitleName(ui->LW_DataSheet->item(getindex)->text());
    }
    if(QDialog::Accepted==tDlg.exec())
    {


        QDir temdir(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        QFileInfoList temlis=temdir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,QDir::Time);
        ////////////////
        QDir destPath;
        if(!tDlg.toGetTitleName().isEmpty())
        {
            destPath.setPath(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+tDlg.toGetTitleName());
            if(!destPath.exists())
            {
                destPath.mkpath(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+tDlg.toGetTitleName());
                toAddDataSheetItem(tDlg.toGetTitleName(),Qt::Checked);
            }

        }
        else
        {

            for(int i=1;i<=(temlis.size()+1);i++)
            {
                QDir temss(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+QString::number(i));
                if(!temss.exists())
                {
                    destPath.setPath(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+QString::number(i));

                    destPath.mkpath(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+QString::number(i));
                    toAddDataSheetItem(QString::number(i),Qt::Checked);

                }

            }
        }
        tDlg.toGetCopyImage(destPath.path());
        toSaveDataSheetCheckState();
        QMessageBox::about(0,u8"提示",u8"拷贝完成!");
    }
}



void Sam2LabelForm::on_PB_RemoveLab_clicked()
{
    int index=ui->LW_DataSheet->currentRow();
    if(index<0)
    {
        return;
    }
    //
    QDir temdir;
    temdir.setPath(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+ui->LW_DataSheet->item(index)->text());
    if(1==QMessageBox::question(0,u8"警告",u8"确认删除，将不可恢复",u8"取消",u8"确认"))
    {

        if(true==temdir.removeRecursively())
        {
            QMessageBox::about(0,u8"提示",QString(u8"删除目录%1成功").arg(ui->LW_DataSheet->item(index)->text()));
            delete ui->LW_DataSheet->takeItem(index);
            toSaveDataSheetCheckState();

        }
        else
        {
            QMessageBox::warning(0,u8"提示",QString(u8"删除目录%1失败").arg(ui->LW_DataSheet->item(index)->text()),u8"取消",u8"确认");
        }
    }

}




void Sam2LabelForm::on_LW_DataSheet_itemClicked(QListWidgetItem *item)
{
    if(ui->LW_DataSheet->currentItem() == nullptr)
    {
        return;
    }


    m_WorkPath=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+ui->LW_DataSheet->currentItem()->text();
    //
    QStringList filtelis;
    filtelis<<"*.bmp"<<"*.jpg"<<"*.jpeg"<<"*.png"<<"*.tiff";
    QFileInfoList CurentFiles=YtYoloDefine::toGetPathFileInfo(m_WorkPath,filtelis);
    //
    ui->LW_FileList->clear();
    int currentSelect = ui->comboBox->currentIndex();

    if(currentSelect == 4)
    {
        ui->CB_CurrentLabel->setEnabled(false);
    }
    //
    for(int i=0;i<CurentFiles.size();i++)
    {

        const QFileInfo& fileInfo = CurentFiles.at(i);
        QString baseName = fileInfo.completeBaseName(); // 获取不带扩展名的文件名
        QString jsonPath = m_WorkPath + "/" + baseName + ".json";
        QString txtPath = m_WorkPath + "/" + baseName + ".txt";

        bool isAnnotated = QFile::exists(jsonPath);
        bool isDected = QFile::exists(txtPath);

        // 根据筛选条件决定是否显示该文件
        if (currentSelect == 1 && !isAnnotated) continue;
        if (currentSelect == 2 && isAnnotated) continue;
        if (currentSelect == 3 && !isDected) continue;
        if (currentSelect == 4 && ! ProTrainData::IsJsonHasLabel(jsonPath, ui->CB_CurrentLabel->currentText())) continue;

        // 添加文件到列表
        QListWidgetItem* listItem = new QListWidgetItem(fileInfo.fileName());

        // 设置图标
        listItem->setIcon(QIcon(isAnnotated
            ? u8":/ResourYolo/Selfbutton/已标注.png"
            : u8":/ResourYolo/Selfbutton/未标注.png"));

        // 可选：设置不同状态的不同文本颜色
//        listItem->setForeground(isAnnotated ? Qt::darkGreen : Qt::darkRed);

        ui->LW_FileList->addItem(listItem);
    }
    ui->CB_CurrentLabel->setEnabled(true);
}

void Sam2LabelForm::on_LW_FileList_itemSelectionChanged()
{


    m_CurentIndex=ui->LW_FileList->currentRow();
    m_CurentFileName=ui->LW_FileList->currentItem()->text();
    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    m_GetImage.load(filename);

    m_YtRoiLabelSet.toSetFileName(m_CurentFileName);
    m_YtRoiLabelSet.toSetImSize(m_GetImage.size());
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toSetImage(m_GetImage);

    on_PB_Reload_clicked();

}

void Sam2LabelForm::on_PB_SaveCurent_clicked()
{
    //
    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    //
    if(m_CurentIndex>=0 && m_CurentIndex<ui->LW_FileList->count())
    {
        if(m_YtRoiLabelSet.m_SetLabeset.size()>0)
        {
            ui->LW_FileList->item(m_CurentIndex)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));
        }
        else
        {
            ui->LW_FileList->item(m_CurentIndex)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/未标注.png"));
        }
    }
}

void Sam2LabelForm::on_PB_Reload_clicked()
{
    m_YtRoiLabelSet.toLoadJson(m_WorkPath);

    m_IsaddState=true;

    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toRemoveAllRoi();
    m_TxtOverPlayShow.toClearData();
    ui->LW_SetLab->setRowCount(m_YtRoiLabelSet.m_SetLabeset.size());

    if(ui->ckB_ShowLable->isChecked())
    {
        for(int i=0;i<m_YtRoiLabelSet.m_SetLabeset.size();i++)
        {

            QVector<double> getdata=m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiData();
            QVector<double> displayData = toDisplayPointData(m_YtRoiLabelSet.m_SetLabeset[i]);
            if(displayData.isEmpty())
            {
                displayData = getdata;
            }
            ui->YtLabelRoi->toAppItemLabel(m_YtRoiLabelSet.m_SetLabeset[i].toGetName(),m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiType(),
                                           displayData);
            //
            ui->LW_SetLab->setItem(i,0,new QTableWidgetItem(QString::number(i+1)));

            ui->LW_SetLab->setItem(i,1,new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].toGetName()));
            int getinde=qMax(0,m_YtYoloSetPro.toGetLabelName().indexOf(m_YtRoiLabelSet.m_SetLabeset[i].toGetName()));
            ui->LW_SetLab->item(i,1)->setBackgroundColor(m_YtYoloSetPro.toGetColorDefine().at(getinde));

            ui->LW_SetLab->setItem(i,2,new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].shape_type));


            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(toRoiText(displayData)));
            //
            for(int m=0;m<4;m++)
            {
                ui->LW_SetLab->item(i,m)->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    //显示测试推理的结果
    if(ui->ckB_ShowResu->isChecked())
    {
        QString tImgPath = m_WorkPath+"/"+m_YtRoiLabelSet.m_imagePath;
        int getindex=tImgPath.lastIndexOf(".");
        if(getindex<0)
        {
            return;
        }

        QFile tsetfile;
        m_Txtfilename = tImgPath.left(getindex)+".txt";
        tsetfile.setFileName(m_Txtfilename);
        if(tsetfile.exists())
        {
            for(QString tstr:m_YtYoloSetPro.m_NameList)
            {
                qDebug()<<tstr;
            }

            //矩形目标识别
            m_YtRoiLabelSet.toLoadDetcTxt(m_Txtfilename,m_GetImage.size(),m_YtYoloSetPro.m_NameList);
            m_YtRoiLabelSet.toGenDetcOveplay(m_TxtOverPlayShow,m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);
            ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");
            ui->YtLabelRoi->toUpdateShow();

        }
    }

    m_IsaddState=false;
    toSyncHighlightedRoiFromTable(ui);


}
void Sam2LabelForm::on_CB_CurrentLabel_activated(const QString &arg1)
{
    Q_UNUSED(arg1);
    ui->CB_SetLabeMode->setCurrentIndex(0);
    toSyncSam2PromptMode();
}

void Sam2LabelForm::on_CB_ROIType_activated(const QString &arg1)
{
    Q_UNUSED(arg1);
    toSyncSam2PromptMode();
}

void Sam2LabelForm::on_CB_SetLabeMode_activated(int index)
{
    Q_UNUSED(index);
    toSyncSam2PromptMode();
}

void Sam2LabelForm::on_PB_ReMoveCurentLab_clicked()
{
    int index=ui->LW_SetLab->currentRow();
    if(index<0)
    {
        return;
    }
    //
    ui->YtLabelRoi->toRemoveRoiByKey(ui->LW_SetLab->item(index,0)->text());
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
    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        m_YtRoiLabelSet.toReMoveIndex(index);
        on_PB_SaveCurent_clicked();
    }
}

void Sam2LabelForm::on_PB_ClearLab_clicked()
{
    m_IsaddState=true;
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toSetHighlightedRoiKey(QString());
    ui->YtLabelRoi->toRemoveAllRoi();
    m_YtRoiLabelSet.toClearData();
    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        on_PB_SaveCurent_clicked();
    }
    m_IsaddState=false;

}

void Sam2LabelForm::on_PB_StartIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(0);
}

void Sam2LabelForm::on_PB_EndIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(ui->LW_FileList->count()-1);
}

void Sam2LabelForm::on_PB_LastIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    int getlis= ui->LW_FileList->currentRow();
    if(getlis<0)
    {
        getlis=0;
    }
    else
    {
        if(getlis>=1)
        {
            getlis-=1;
        }
    }
    ui->LW_FileList->setCurrentRow(getlis);
}

void Sam2LabelForm::on_PB_NextIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    int getlis= ui->LW_FileList->currentRow();
    if(getlis<0)
    {
        getlis=0;
    }
    else
    {
        if(getlis<(ui->LW_FileList->count()-1))
        {
            getlis+=1;
        }
    }
    ui->LW_FileList->setCurrentRow(getlis);
}

void Sam2LabelForm::on_PB_XLabe_clicked()
{
    if(!QFile::exists(YtYoloDefine::toGetPythonPath()+"/python.exe"))
    {
        QMessageBox::warning(0,u8"警告",u8"运行路径异常",u8"确定");
        return;
    }
    if(m_SetProcess==nullptr)
    {
        m_SetProcess = new QProcess(this);                 //读命令行标准数据（兼容）
        m_SetProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_SetProcess->start("cmd.exe");
    }
    //
    QString cmdDir = YtYoloDefine::toGetPythonPath().left(1)+":\r\n";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    cmdDir=QString("cd %1/X-AnyLabeling-main\r\n").arg(YtYoloDefine::toGetPythonPath());
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    QString SelfDefine=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName;
    for(int i=0;i<m_YtYoloSetPro.toGetLabelName().size();i++)
    {
        if(i==0)
        {
            SelfDefine.append(QString(" --labels %1").arg(m_YtYoloSetPro.toGetLabelName().at(i)));
        }
        else
        {
            SelfDefine.append(QString(",%1").arg(m_YtYoloSetPro.toGetLabelName().at(i)));

        }
    }

    cmdDir=QString("%1/python.exe %2/X-AnyLabeling-main/anylabeling/app.py %3\r\n").
            arg(YtYoloDefine::toGetPythonPath()).arg(YtYoloDefine::toGetPythonPath()).arg(SelfDefine);
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.toLocal8Bit().length());

}

void Sam2LabelForm::on_LW_SetLab_cellDoubleClicked(int row, int column)
{
    //可以修改标签
    ReNameDlg tDlg;

    tDlg.setWindowFlags(Qt::SubWindow);
    tDlg.toInitData(ui->LW_SetLab->item(row,1)->text(),m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);

    if(QDialog::Accepted==tDlg.exec())
    {

        LabelSet LabelName=m_YtRoiLabelSet.m_SetLabeset.at(row);

        if(LabelName.Name==tDlg.toGetCurentName())
        {
            return;
        }
        LabelName.Name=tDlg.toGetCurentName();

        m_YtRoiLabelSet.toModify(LabelName,row,true);

        on_PB_SaveCurent_clicked();
        on_PB_Reload_clicked();
    }


}





void Sam2LabelForm::on_PB_ViewDir_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName));
}

void Sam2LabelForm::toInitMenuData()
{

    QAction *pAc1=new QAction(toGetTranTip(u8"批量设置标签"));
    QAction *pAc2=new QAction(toGetTranTip(u8"批量移除标签"));
    m_pRightMenu.addAction(pAc1);
    m_pRightMenu.addAction(pAc2);
    connect(&m_pRightMenu,&QMenu::triggered,this,&Sam2LabelForm::slot_menuTrigger);
    ui->LW_DataSheet->setContextMenuPolicy(Qt::CustomContextMenu);

}

void Sam2LabelForm::on_LW_DataSheet_customContextMenuRequested(const QPoint &pos)
{
    if(ui->LW_DataSheet->currentRow()<0)
    {
        return;
    }
    m_pRightMenu.exec(QCursor::pos());
}

void Sam2LabelForm::slot_menuTrigger(QAction *action)
{
    QString SetNewType;

    if(action->text() == toGetTranTip(u8"批量设置标签"))
    {
        NameSelectDlg tDlg;
        tDlg.toInitLabeLis(m_YtYoloSetPro.m_NameList);
        tDlg.setWindowFlags(Qt::SubWindow);
        if(QDialog::Accepted==tDlg.exec())
        {
            SetNewType=tDlg.toGetSetName();
        }
        else
        {
            return;
        }

    }
    else if(action->text() == toGetTranTip(u8"批量移除标签"))
    {
        SetNewType.clear();
    }
    m_WorkPath=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+ui->LW_DataSheet->currentItem()->text();
    //
    QStringList filtelis;
    filtelis<<"*.bmp"<<"*.jpg"<<"*.jpeg"<<"*.png"<<"*.tiff";
    QFileInfoList CurentFiles=YtYoloDefine::toGetPathFileInfo(m_WorkPath,filtelis);
    //
    //
    QImage temim;
    QString Destpfilename;
    LabelSet temlabeset;
    CMvRect temsetdata;
    for(int i=0;i<CurentFiles.size();i++)
    {

        int getindex=CurentFiles.at(i).fileName().lastIndexOf(".");
        Destpfilename=m_WorkPath+"/"+CurentFiles.at(i).fileName().left(getindex)+".json";
        //

        if(SetNewType.isEmpty())
        {
            QFile::remove(Destpfilename);
            if(i<ui->LW_FileList->count())
            {
                ui->LW_FileList->item(i)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/未标注.png"));
            }
        }
        else
        {
            temim.load(CurentFiles.at(i).absoluteFilePath());
            qDebug()<<"PPPP"<<CurentFiles.at(i).absoluteFilePath()<<CurentFiles.at(i).path();
            m_YtRoiLabelSet.toSetFileName(CurentFiles.at(i).fileName());
            m_YtRoiLabelSet.toSetImSize(temim.size());
            m_YtRoiLabelSet.m_SetLabeset.clear();
            temsetdata=CMvRect(0,0,temim.width()-1,temim.height()-1);
            temlabeset.toInitProData(SetNewType,LrectangleROI,temsetdata.Data());
            m_YtRoiLabelSet.m_SetLabeset.append(temlabeset);
            m_YtRoiLabelSet.toSaveJson(CurentFiles.at(i).path());
            if(i<ui->LW_FileList->count())
            {
                ui->LW_FileList->item(i)->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));
            }

        }


    }
    QMessageBox::about(0,u8"提示",u8"设置完成");
}

void Sam2LabelForm::on_ckB_ShowResu_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void Sam2LabelForm::on_ckB_ShowLable_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void Sam2LabelForm::on_PB_SetBackgroud_clicked()
{

    QString SetNewType = "background";

    //如果没有，则添加“background”类别
    if(!m_YtYoloSetPro.toGetLabelName().contains(SetNewType))
    {
        m_YtYoloSetPro.toAppendLabelInfo(SetNewType, Qt::green);

        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        //qDebug()<<"label data save path:" <<m_WorkPath;
        toInitLabelSet();
    }


    //标注当前图片为“background”类别
    QImage temim;
    LabelSet temlabeset;
    CMvRect temsetdata;

    m_WorkPath=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+ui->LW_DataSheet->currentItem()->text();

    temim.load(m_WorkPath+"/"+ui->LW_FileList->currentItem()->text());
    qDebug()<<"PPPP"<<m_WorkPath;
    m_YtRoiLabelSet.toSetFileName(ui->LW_FileList->currentItem()->text());
    m_YtRoiLabelSet.toSetImSize(temim.size());
    m_YtRoiLabelSet.m_SetLabeset.clear();
    temsetdata=CMvRect(2,2,temim.width()-2,temim.height()-2);
    temlabeset.toInitProData(SetNewType,LrectangleROI,temsetdata.Data());
    m_YtRoiLabelSet.m_SetLabeset.append(temlabeset);

    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    ui->YtLabelRoi->toSetImage(temim);
    ui->LW_FileList->currentItem()->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));


    //重载 标注
    on_PB_Reload_clicked();
}




void Sam2LabelForm::on_PB_TBConv_clicked()
{

    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    //显示图像
    QImage temim=m_GetImage.mirrored(false,true);
    m_GetImage=temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}

void Sam2LabelForm::on_PB_LRConv_clicked()
{
    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    //显示图像
    QImage temim=m_GetImage.mirrored(true,false);
    m_GetImage=temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}

void Sam2LabelForm::on_comboBox_currentIndexChanged(int index)
{
    QListWidgetItem *item = new QListWidgetItem;
    on_LW_DataSheet_itemClicked(item);
}

void Sam2LabelForm::on_PB_AddAnno_clicked()
{
    if(ui->LW_FileList->currentItem() ==nullptr)
    {
        return;
    }
    if(QDialog::Rejected==QMessageBox::question(0,u8"提示",u8"确认删除？",u8"是",u8"否"))
    {
        //int currentListIndex = ui->LW_FileList->row(ui->LW_FileList->currentItem());
        QModelIndex tShowIndex = ui->LW_FileList->currentIndex();
        QString deletFileName = m_WorkPath+"/"+ui->LW_FileList->currentItem()->text();
        QString BaseName = deletFileName.split(".").first();

        QFile().remove(deletFileName);
        if(QFile().exists(BaseName+".txt"))
        {
            QFile().remove(BaseName+".txt");
            qDebug()<<"removed: "<<BaseName+".txt";
        }
        if(QFile().exists(BaseName+".json"))
        {
            QFile().remove(BaseName+".json");
            qDebug()<<"removed: "<<BaseName+".json";
        }

        //刷新显示
        if(ui->LW_DataSheet->currentItem() == nullptr)
        {
            return;
        }
        on_LW_DataSheet_itemClicked(ui->LW_DataSheet->currentItem());
        ui->LW_FileList->setCurrentIndex(tShowIndex);

    }
}
