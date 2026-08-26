#ifndef DATASETFORM_H
#define DATASETFORM_H

#include <QWidget>
#include "ytyolodefine.h"
#include <QTreeWidgetItem>
#include "showprocessform.h"
namespace Ui {
class DataSetForm;
}

class DataSetForm : public QWidget
{
    Q_OBJECT

public:
    explicit DataSetForm(QWidget *parent = nullptr);
    ~DataSetForm();

private:
    Ui::DataSetForm *ui;
public:
    QString m_ProcessName;//项目处理名称
public:
    YtYoloSetPro m_YtYoloSetPro;//读取标签
    YtRoiLabelSet m_YtRoiLabelSet;//标签数据
    QVector<QStringList> m_TrainLabelSetfilename;//按照类别进行文件存储
    QVector<QStringList> m_ValLabelSetfilename;//按照类别进行文件存储
    QVector<QStringList> m_BackGroundJsonfilename;//按照类别进行文件存储json
    QStringList          m_BackGroudImgname;  //存储全部BG_IMG的path
    QString m_DataGenErrorMessage;
    int m_DataGenRunIndex=0;
    YtSetShowtObj m_OverPlayShow;
    ShowProcessForm m_ShowProcessForm;
    QMap<QString,QString> m_GetKeys;
public:
    void toSetProcessName(QString Setname);
    void toInitShow();
    QFileInfoList toGetCheckedDataSheetDirList();

    void toProDataGet(bool isadd=false);//生成数据
    void toShowData();
    void toSaveData(int changele=3,bool datapp=false);//存储序列化文件
    void toLoadData();//读取序列化文件
    QString toGetRunType();//给出训练类别
    int toGetRunIndex();
private slots:
    void on_PB_RunDataGen_clicked();
    void on_PB_ViewPos_clicked();
    void on_PB_RunDataAppend_clicked();

public slots:
    void slot_ItemDoubleClicked(QTreeWidgetItem* item, int colum);
    void slotFinish();
signals:
    void Sigfinish();
    void SigProcess(int percent);
};

#endif // DATASETFORM_H
