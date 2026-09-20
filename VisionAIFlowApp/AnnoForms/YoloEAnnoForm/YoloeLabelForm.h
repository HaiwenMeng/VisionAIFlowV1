#ifndef YoloeLabelForm_H
#define YoloeLabelForm_H

#include <QWidget>
#include "ytyolodefine.h"
#include <QMenu>
#include "datasetform.h"
#include "sam2assist/Sam2LabelCanvasCompat.h"

namespace Ui
{
class YoloeLabelForm;
}

class YoloeLabelForm : public QWidget
{
    Q_OBJECT

public:
    explicit YoloeLabelForm(QWidget *parent = nullptr);
    ~YoloeLabelForm();
    void setTaskName(const QString &taskName);
    bool initialize();
    void releaseSam2();
    bool isReady() const;

private:
    Ui::YoloeLabelForm *ui;
    bool m_Sam2Available = false;
    bool m_Sam2UnavailableMessageShown = false;

public:
    QFileInfoList m_GetProJectList; //��Ŀ�б�
    YtYoloSetPro m_YtYoloSetPro;    //��ȡ��ǩ
    YtRoiLabelSet m_YtRoiLabelSet;  //��ǩ����
public:
    QString m_ProcessName;
    QString m_WorkPath;
    QString m_CurentFileName; //��ǰ��������
    int m_CurentIndex = -1;
    QImage m_GetImage;
    bool m_IsaddState = false;

public:
    QProcess *m_SetProcess = nullptr; //��������ѵ���Ľ���
public:
    QString m_Txtfilename;                //����val���
    DataSetForm *m_DataSetForm = nullptr; //����ҳ�ķ���ָ��
    YtSetShowtObj m_TxtOverPlayShow;      // val���ͼ��
public:
    //��ʼ����ǩ��ʾ
    void toInitProShowLabel();
    void toInitLabelSet();
    void toInitPath();
    void toAddDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState = Qt::Checked);
    void toSaveDataSheetCheckState();
    void toInitSam2Inference();

private:
    void toSyncSam2PromptMode();
    void toApplyEraserStroke(const QVector<QPointF> &points, qreal diameter);
    void toHandleSam2Unavailable(const QString &errorMessage, bool forceMessage = false);
    void toHandleSam2RuntimeError(const QString &errorMessage);
public slots:
    void toGetDataUpdate(QString SetKey, QString LableName, int type); //ֻҪ����Id�������Լ���ǩ����
    void Slot_ROIChange(QVector<double> tdata, QString &key, int type); // ������ʾ�ؼ�ROI�ź�
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
    void on_PB_TBConv_clicked();
    void on_PB_LRConv_clicked();
    void on_comboBox_currentIndexChanged(int index);
    void on_PB_AddAnno_clicked();
    void on_PB_DrawIgnore_clicked();
    void on_PB_Eraser_toggled(bool checked);
    void on_spinBox_EraserSize_valueChanged(int value);
    void on_ckB_ShowMask_stateChanged(int state);

public:
    QMenu m_pRightMenu;

public:
    void toInitMenuData();
};

#endif // YoloeLabelForm_H
