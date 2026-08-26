#include "CommonSetForm.h"
#include "ui_CommonSetForm.h"
#include "baseannolegacycanvas.h"
#include "setnamedialog.h"
#include <QDir>
#include "setlabelname.h"
#include "filecopysetdlg.h"
#include "renamedlg.h"
#include "nameselectdlg.h"
#include <QShortcut>
#include <QSignalBlocker>

CommonSetForm::CommonSetForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CommonSetForm)
{
    ui->setupUi(this);
    ui->stackedWidget->setCurrentIndex(0);

    QShortcut *lastImageShortcut = new QShortcut(QKeySequence(Qt::Key_A), this);
    lastImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(lastImageShortcut, &QShortcut::activated, this, &CommonSetForm::on_PB_LastIm_clicked);

    QShortcut *nextImageShortcut = new QShortcut(QKeySequence(Qt::Key_D), this);
    nextImageShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextImageShortcut, &QShortcut::activated, this, &CommonSetForm::on_PB_NextIm_clicked);
    //
    connect(ui->YtLabelRoi,
            &BaseAnnoLegacyCanvas::SigGetDataUpdate,
            this,
            &CommonSetForm::toGetDataUpdate);
    connect(ui->YtLabelRoi, &BaseAnnoLegacyCanvas::ROIChange, this, &CommonSetForm::Slot_ROIChange);
    //
    ui->YtLabelRoi->toSetBGColor(QColor(33,34,35));
    ui->LW_SetLab->verticalHeader()->setVisible(false);   //隐藏垂直表头
    //ui->LW_SetLab->horizontalHeader()->setStretchLastSection(true);
    ui->LW_SetLab->setEditTriggers(QAbstractItemView::NoEditTriggers);  //不可编辑
    ui->LW_SetLab->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->LW_SetLab->setSelectionBehavior(QAbstractItemView::SelectRows);   //选择一行
    ui->LW_SetLab->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    toInitMenuData();
    connect(ui->LW_DataSheet,&QListWidget::itemChanged,this,&CommonSetForm::slotDataSheetItemChanged);


}

CommonSetForm::~CommonSetForm()
{
    delete ui;

    if(m_SetProcess)
    {
        m_SetProcess->close();
        delete m_SetProcess;
        m_SetProcess=nullptr;
    }
}

void CommonSetForm::toInitProShowLabel()
{

    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
    toInitLabelSet();
    toInitPath();
    //
    m_CurentIndex=-1;
    m_CurentFileName="";
    ui->LW_FileList->clear();
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toSetImage(QImage());
    ui->YtLabelRoi->toRemoveAllRoi();

}




void CommonSetForm::toInitLabelSet()
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
    QVector<int> SetRoiType;
    SetRoiType << LabelSet::LrectangleROI << LabelSet::LrotaterectangleROI << LabelSet::LcircleROI
               << LabelSet::LpolygonROI << LabelSet::LpointROI;
    ui->YtLabelRoi->toSetRoiDefaltType(SetRoiType.at(ui->CB_ROIType->currentIndex()));
    ui->YtLabelRoi->toSetRoiDefaltName(ui->CB_CurrentLabel->currentText());
    if(ui->CB_SetLabeMode->currentIndex()<0)
    {
        ui->YtLabelRoi->toSetRoiDefaltName("");
    }
}

void CommonSetForm::toAddDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState)
{
    QListWidgetItem *item=new QListWidgetItem(dataSheetName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checkState);
    ui->LW_DataSheet->addItem(item);
}

void CommonSetForm::setTaskName(const QString &taskName)
{
    if (taskName.trimmed().isEmpty())
    {
        qCritical().noquote() << QString(u8"标注页面未接收到任务名称");
        return;
    }

    m_ProcessName = taskName;
    toInitProShowLabel();
}

void CommonSetForm::toSaveDataSheetCheckState()
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

void CommonSetForm::toInitPath()
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

void CommonSetForm::slotDataSheetItemChanged(QListWidgetItem *item)
{
    Q_UNUSED(item);
    toSaveDataSheetCheckState();
}
void CommonSetForm::toGetDataUpdate(QString SetKey, QString LableName, int type)
{
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
            ui->LW_SetLab->item(i,1)->setBackground(m_YtYoloSetPro.toGetColorDefine().at(getindex));

            QVector<double> getData=ui->YtLabelRoi->getROI(SetKey);
            temppset.toInitProData(LableName,type,getData);
            ui->LW_SetLab->setItem(i,2,new QTableWidgetItem(temppset.shape_type));


            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(temppset.RoiData));
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

void CommonSetForm::Slot_ROIChange(QVector<double> tdata, QString &key, int type)
{
    if(m_IsaddState)
    {
        return;
    }
    int rowcount=ui->LW_SetLab->rowCount();
    LabelSet temppset;
    int getindex = -1;
    for(int i=0;i<rowcount;i++)
    {
        if(key.toInt()==ui->LW_SetLab->item(i,0)->text().toInt())
        {
            getindex=i;
            temppset.toInitProData(ui->LW_SetLab->item(i,1)->text(),type,tdata);
            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(temppset.RoiData));
            //
            break;
        }

    }

    if(getindex >= 0 && ui->CB_SaveROIType->currentIndex()==0)
    {
        m_YtRoiLabelSet.toModify(temppset,getindex,false);
        on_PB_SaveCurent_clicked();
    }

}

void CommonSetForm::on_PB_AddDataSet_clicked()
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

void CommonSetForm::on_PB_SunDataSet_clicked()
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

void CommonSetForm::on_LW_LabelSet_itemDoubleClicked(QListWidgetItem *item)
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
            QMessageBox::warning(0,u8"警告",u8"名称已存在或为空",u8"确定");
            return;
        }
        m_YtYoloSetPro.toModifyInfo(getLable,getColor,getindex);
        m_YtYoloSetPro.toSaveData(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName);
        toInitLabelSet();
    }
}

void CommonSetForm::on_PB_AddLab_clicked()
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

        //QMessageBox::about(0,u8"提示",tDlg.toGetTitleName()+QString::number(tDlg.toGetTitleName().size()));
        //执行拷贝
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



void CommonSetForm::on_PB_RemoveLab_clicked()
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
            QMessageBox::warning(0,u8"提示",QString(u8"删除目录%11失败").arg(ui->LW_DataSheet->item(index)->text()),u8"取消",u8"确认");
        }
    }

}




void CommonSetForm::on_LW_DataSheet_itemClicked(QListWidgetItem *item)
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
    int currentSelect = ui->combbox_SelectShow->currentIndex();

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

void CommonSetForm::on_LW_FileList_itemSelectionChanged()
{

    if(ui->CB_OcrP->isChecked() && ui->CB_AutoSave->isChecked())
    {
        //切换的时候保存上一张
        if(!m_CurentFileName.isEmpty() && !ui->LE_OcrPP->text().isEmpty())
        {
            //保存上一张
           on_PB_SaveTxt_clicked();

        }
    }

    m_CurentIndex=ui->LW_FileList->currentRow();
    m_CurentFileName=ui->LW_FileList->currentItem()->text();
    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    //显示图象
    m_GetImage.load(filename);

    m_YtRoiLabelSet.toSetFileName(m_CurentFileName);
    m_YtRoiLabelSet.toSetImSize(m_GetImage.size());
    ui->YtLabelRoi->toSetImage(m_GetImage);
    //重载 标注

    on_PB_Reload_clicked();


}

void CommonSetForm::on_PB_SaveCurent_clicked()
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

void CommonSetForm::on_PB_Reload_clicked()
{
    m_YtRoiLabelSet.toLoadJson(m_WorkPath);
    m_IsaddState=true;

    ui->YtLabelRoi->toRemoveAllRoi();
    ui->YtLabelRoi->ClearAllOverPlayPtr();
    ui->YtLabelRoi->ClearAllStdOverPlayPtr();

    m_TxtOverPlayShow.toClearData();
    ui->LW_SetLab->setRowCount(m_YtRoiLabelSet.m_SetLabeset.size());




    if(ui->ckB_ShowLable->isChecked())
    {
        for(int i=0;i<m_YtRoiLabelSet.m_SetLabeset.size();i++)
        {

            QVector<double> getdata=m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiData();
            ui->YtLabelRoi->toAppItemLabel(m_YtRoiLabelSet.m_SetLabeset[i].toGetName(),m_YtRoiLabelSet.m_SetLabeset[i].toGetRoiType(),
                                           getdata);
            //
            ui->LW_SetLab->setItem(i,0,new QTableWidgetItem(QString::number(i+1)));

            ui->LW_SetLab->setItem(i,1,new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].toGetName()));
            int getinde=qMax(0,m_YtYoloSetPro.toGetLabelName().indexOf(m_YtRoiLabelSet.m_SetLabeset[i].toGetName()));
            ui->LW_SetLab->item(i,1)->setBackground(m_YtYoloSetPro.toGetColorDefine().at(getinde));

            ui->LW_SetLab->setItem(i,2,new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].shape_type));


            ui->LW_SetLab->setItem(i,3,new QTableWidgetItem(m_YtRoiLabelSet.m_SetLabeset[i].RoiData));
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
            switch (ui->CB_ROIType->currentIndex())
            {
            case 0:
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
                break;
            }
            case 1:
            {
                //OBB识别
                m_YtRoiLabelSet.toLoadObbTxt(m_Txtfilename,m_GetImage.size(),m_YtYoloSetPro.m_NameList);
                m_YtRoiLabelSet.toGenObbOveplay(m_TxtOverPlayShow,m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);
                ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");
                ui->YtLabelRoi->toUpdateShow();
                break;
            }
            case 2:
            {
                //语义分割
                m_YtRoiLabelSet.toLoadSegTxt(m_Txtfilename,m_GetImage.size(),m_YtYoloSetPro.m_NameList);
                m_YtRoiLabelSet.toGenSegOveplay(m_TxtOverPlayShow,m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);
                ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");
                ui->YtLabelRoi->toUpdateShow();
                break;
            }
            case 3:
            {
                //分类识别
                m_YtRoiLabelSet.toLoadClassTxt(m_Txtfilename,m_GetImage.size(),m_YtYoloSetPro.m_NameList);
                m_YtRoiLabelSet.toGenClassOveplay(m_TxtOverPlayShow,m_YtYoloSetPro.m_NameList,m_YtYoloSetPro.m_ClorDefine);
                ui->YtLabelRoi->addOverPlayPtr(&m_TxtOverPlayShow, "valRest");
                ui->YtLabelRoi->toUpdateShow();
                break;
            }
            default:
                break;
            }

        }
    }

    ui->YtLabelRoi->toUpdateShow();
    m_IsaddState=false;


}

void CommonSetForm::on_CB_CurrentLabel_activated(const QString &arg1)
{
    ui->YtLabelRoi->toSetRoiDefaltName(arg1);
    ui->CB_SetLabeMode->setCurrentIndex(0);
}

void CommonSetForm::on_CB_ROIType_activated(const QString &arg1)
{
    QVector<int> SetRoiType;
    SetRoiType << LabelSet::LrectangleROI << LabelSet::LrotaterectangleROI << LabelSet::LcircleROI
               << LabelSet::LpolygonROI << LabelSet::LpointROI;
    ui->YtLabelRoi->toSetRoiDefaltType(SetRoiType.at(ui->CB_ROIType->currentIndex()));
}

void CommonSetForm::on_CB_SetLabeMode_activated(int index)
{
    if(index>0)
    {
        ui->YtLabelRoi->toSetRoiDefaltName("");
    }
}

void CommonSetForm::on_PB_ReMoveCurentLab_clicked()
{
    int index=ui->LW_SetLab->currentRow();
    if(index<0)
    {
        return;
    }
    //
    ui->YtLabelRoi->toRemoveRoiByKey(ui->LW_SetLab->item(index,0)->text());
    ui->LW_SetLab->removeRow(index);
    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        m_YtRoiLabelSet.toReMoveIndex(index);
        on_PB_SaveCurent_clicked();
    }
}

void CommonSetForm::on_PB_ClearLab_clicked()
{
    m_IsaddState=true;
    ui->LW_SetLab->setRowCount(0);
    ui->YtLabelRoi->toRemoveAllRoi();
    m_YtRoiLabelSet.toClearData();
    if(ui->CB_SaveROIType->currentIndex()==0)
    {
        on_PB_SaveCurent_clicked();
    }
    m_IsaddState=false;

}

void CommonSetForm::on_PB_StartIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(0);
}

void CommonSetForm::on_PB_EndIm_clicked()
{
    if(ui->LW_FileList->count()<1)
    {
        return;
    }
    ui->LW_FileList->setCurrentRow(ui->LW_FileList->count()-1);
}

void CommonSetForm::on_PB_LastIm_clicked()
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

void CommonSetForm::on_PB_NextIm_clicked()
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

void CommonSetForm::on_PB_XLabe_clicked()
{
    if(!QFile::exists(YtYoloDefine::toGetPythonPath()+"/python.exe"))
    {
        QMessageBox::warning(0,u8"警告",u8"运行路径异常",u8"确定");
        return;
    }
    qDebug()<< "111111";
    if(m_SetProcess==nullptr)
    {
        m_SetProcess = new QProcess(this);                 //读命令行标准数据（兼容）
        m_SetProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_SetProcess->start("cmd.exe");
    }
    qDebug()<< "222222";
    //
    QString cmdDir = YtYoloDefine::toGetPythonPath().left(1)+":"+ "\r\n";
    qDebug()<< "333333";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    qDebug()<< "444444";
    cmdDir=QString("cd %1/X-AnyLabeling-main\r\n").arg(YtYoloDefine::toGetPythonPath());
    qDebug()<< "555555";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.length());
    qDebug()<< "666666";
    QString SelfDefine=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName;
    qDebug()<< "777777";
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
    qDebug()<< "888888";

    cmdDir=QString("%1/python.exe %2/X-AnyLabeling-main/anylabeling/app.py %3\r\n").
            arg(YtYoloDefine::toGetPythonPath()).arg(YtYoloDefine::toGetPythonPath()).arg(SelfDefine);
    qDebug()<< "999999";
    m_SetProcess->write(cmdDir.toLocal8Bit().data(), cmdDir.toLocal8Bit().length());

    qDebug()<< "000000";
}

void CommonSetForm::on_LW_SetLab_cellDoubleClicked(int row, int column)
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





void CommonSetForm::on_PB_ViewDir_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName));
}

void CommonSetForm::toInitMenuData()
{

    QAction *pAc1=new QAction(u8"批量设置标签");
    QAction *pAc2=new QAction(u8"批量移除标签");
    m_pRightMenu.addAction(pAc1);
    m_pRightMenu.addAction(pAc2);
    connect(&m_pRightMenu,&QMenu::triggered,this,&CommonSetForm::slot_menuTrigger);
    ui->LW_DataSheet->setContextMenuPolicy(Qt::CustomContextMenu);

}

void CommonSetForm::on_LW_DataSheet_customContextMenuRequested(const QPoint &pos)
{
    if(ui->LW_DataSheet->currentRow()<0)
    {
        return;
    }
    m_pRightMenu.exec(QCursor::pos());
}

void CommonSetForm::slot_menuTrigger(QAction *action)
{
    QString SetNewType;

    if(action->text() == u8"批量设置标签")
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
    else if(action->text() == u8"批量移除标签")
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
            temlabeset.toInitProData(SetNewType,LabelSet::LrectangleROI,temsetdata.Data());
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

void CommonSetForm::on_ckB_ShowResu_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void CommonSetForm::on_ckB_ShowLable_stateChanged(int arg1)
{
    on_PB_Reload_clicked();
}

void CommonSetForm::on_PB_SetBackgroud_clicked()
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
    temlabeset.toInitProData(SetNewType,LabelSet::LrectangleROI,temsetdata.Data());
    m_YtRoiLabelSet.m_SetLabeset.append(temlabeset);

    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
    ui->YtLabelRoi->toSetImage(temim);
    ui->LW_FileList->currentItem()->setIcon(QIcon(u8":/ResourYolo/Selfbutton/已标注.png"));


    //重载 标注
    on_PB_Reload_clicked();
}

void CommonSetForm::on_PB_SaveTxt_clicked()
{


    CMvRect temsetdata=CMvRect(0,0,m_GetImage.width()-1,m_GetImage.height()-1);
    LabelSet temlabeset;
    temlabeset.toInitProData(ui->LE_OcrPP->text(),LabelSet::LrectangleROI,temsetdata.Data());
    m_YtRoiLabelSet.toSetFileName(m_CurentFileName);
    m_YtRoiLabelSet.toSetImSize(m_GetImage.size());
    m_YtRoiLabelSet.m_SetLabeset.clear();
    m_YtRoiLabelSet.m_SetLabeset.append(temlabeset);
    m_YtRoiLabelSet.toSaveJson(m_WorkPath);
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

void CommonSetForm::on_PB_TBConv_clicked()
{

    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    //显示图象
    QImage temim=m_GetImage.mirrored(false,true);
    m_GetImage=temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}

void CommonSetForm::on_PB_LRConv_clicked()
{
    QString filename=m_WorkPath+"/"+m_CurentFileName;
    //
    //显示图象
    QImage temim=m_GetImage.mirrored(true,false);
    m_GetImage=temim;
    ui->YtLabelRoi->toSetImage(m_GetImage);
    m_GetImage.save(filename);
}



void CommonSetForm::on_PB_DeletImg_clicked()
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

void CommonSetForm::on_PB_Txt2Json_clicked()
{
    if(ui->LW_DataSheet->currentItem() == nullptr || ui->LW_FileList->currentItem() == nullptr)
    {
        return;
    }


    m_WorkPath=YtYoloDefine::toGetLabelPath()+"/"+m_ProcessName+"/"+ui->LW_DataSheet->currentItem()->text();
    QString t_Getfilename = m_WorkPath +"/" + ui->LW_FileList->currentItem()->text();
    QString t_ImgBaseName = t_Getfilename.left(t_Getfilename.lastIndexOf("."));
    if(QFile().exists(t_ImgBaseName +".json"))
    {
        if(QDialog::Rejected==QMessageBox::question(0,u8"警告",u8"标准文件已存在,是否覆盖？",u8"取消",u8"确认"))
         {
            return;
        }
    }
    if(ui->CB_ROIType->currentIndex() == 0)
    {
        QImage tImg = QImage(t_Getfilename);

        m_YtRoiLabelSet.toLoadDetcTxt(t_ImgBaseName +".txt", tImg.size(), m_YtYoloSetPro.m_NameList);
        qDebug()<<"ui->LW_FileList->currentItem()->text():"<<ui->LW_FileList->currentItem()->text();
        qDebug()<<"QFileInfo(t_Getfilename).path(): "<<t_Getfilename<<QFileInfo(t_Getfilename).path();
        m_YtRoiLabelSet.m_imagePath = t_Getfilename;
        m_YtRoiLabelSet.toSaveJson(QFileInfo(t_Getfilename).path());
        on_PB_Reload_clicked();
    }

}

void CommonSetForm::on_combbox_SelectShow_currentIndexChanged(int index)
{
    QListWidgetItem *item = new QListWidgetItem;
    on_LW_DataSheet_itemClicked(item);
}

