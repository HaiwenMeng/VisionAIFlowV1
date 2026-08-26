#ifndef COMMONSETFORM_H
#define COMMONSETFORM_H

#include <QWidget>
#include "ytyolodefine.h"
#include <QMenu>
#include "datasetform.h"
#include "ytyolodefine.h"
namespace Ui {
class CommonSetForm;
}

class CommonSetForm : public QWidget
{
    Q_OBJECT

public:
    explicit CommonSetForm(QWidget *parent = nullptr);
    ~CommonSetForm();
    void setTaskName(const QString &taskName);
private:
    Ui::CommonSetForm *ui;
public:
    QFileInfoList m_GetProJectList;//项目列表
    YtYoloSetPro m_YtYoloSetPro;//读取标签
    YtRoiLabelSet m_YtRoiLabelSet;//标签数据
public:
    QString m_ProcessName;
    QString m_WorkPath;
    QString m_CurentFileName;//当前载入名称
    int m_CurentIndex=-1;
    QImage m_GetImage;
    bool m_IsaddState=false;
public:
    QProcess        *m_SetProcess=nullptr;    //单独用于训练的进程
public:
    QString m_Txtfilename;//承载val结果
    DataSetForm *m_DataSetForm=nullptr;//数据页的访问指针
    YtSetShowtObj m_TxtOverPlayShow;//val结果图层
public:
    //初始化标签显示
    void toInitProShowLabel();
    void toInitLabelSet();
    void toInitPath();
    void toAddDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState=Qt::Checked);
    void toSaveDataSheetCheckState();
public slots:
    void toGetDataUpdate(QString SetKey,QString LableName,int type);//只要返回Id和类型以及标签名称
    void Slot_ROIChange(QVector<double> tdata,QString &key,int type);   // 连接显示控件ROI信号
private slots:
    void on_PB_AddDataSet_clicked();
    void on_PB_SunDataSet_clicked();
    void on_LW_LabelSet_itemDoubleClicked(QListWidgetItem *item);
    void on_PB_AddLab_clicked();
    void on_PB_XLabe_clicked();
    void on_PB_RemoveLab_clicked();
    void on_LW_DataSheet_itemClicked(QListWidgetItem *item);
    void slotDataSheetItemChanged(QListWidgetItem *item);
    void on_LW_FileList_itemSelectionChanged();
    void on_PB_SaveCurent_clicked();
    void on_PB_Reload_clicked();
    void on_CB_CurrentLabel_activated(const QString &arg1);
    void on_CB_ROIType_activated(const QString &arg1);
    void on_CB_SetLabeMode_activated(int index);
    void on_PB_ReMoveCurentLab_clicked();
    void on_PB_ClearLab_clicked();
    void on_PB_StartIm_clicked();
    void on_PB_EndIm_clicked();
    void on_PB_LastIm_clicked();
    void on_PB_NextIm_clicked();

    void on_LW_SetLab_cellDoubleClicked(int row, int column);
    void on_PB_ViewDir_clicked();
    void on_LW_DataSheet_customContextMenuRequested(const QPoint &pos);
    void slot_menuTrigger(QAction *action);

    void on_ckB_ShowResu_stateChanged(int arg1);

    void on_ckB_ShowLable_stateChanged(int arg1);

    void on_PB_SetBackgroud_clicked();

    void on_combbox_SelectShow_currentIndexChanged(int index);

    void on_PB_DeletImg_clicked();

    void on_PB_Txt2Json_clicked();

    void on_PB_SaveTxt_clicked();

    void on_PB_TBConv_clicked();

    void on_PB_LRConv_clicked();

public:
    QMenu m_pRightMenu;
public:
    void toInitMenuData();
};

#endif // COMMONSETFORM_H
