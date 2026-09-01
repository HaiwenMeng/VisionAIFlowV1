#ifndef VALSETFORM_H
#define VALSETFORM_H

#include <QWidget>

#include "ytyolodefine.h"
#include "datasetform.h"

#include <memory>

namespace Ui
{
class ValSetForm;
}

class DetectionInferenceController;

class ValSetForm : public QWidget
{
    Q_OBJECT

public:
    explicit ValSetForm(QWidget *parent = nullptr);
    ~ValSetForm();

private:
    Ui::ValSetForm *ui;
    std::unique_ptr<DetectionInferenceController> m_detectionInferenceController;

public:
    QString m_ProcessName; //项目处理名称
public:
    QProcess *m_SetProcess = nullptr;     //单独用于训练的进程
    DataSetForm *m_DataSetForm = nullptr; //数据页的访问指针
    YtYoloSetPro m_YtYoloSetPro;          //读取标签
    YtRoiLabelSet m_YtRoiLabelSet;        //标签数据
    YtSetShowtObj m_OverPlayShow;
    YtSetShowtObj m_JsonOverPlayShow;
    YtRoiLabelSet m_YtRoiLabelSetJson; //标签数据

public:
    void toSetProcessName(QString Setname);
    void toInitShow();

private:
    void RefreshDetectionPlugins();
    bool RunDetectionInference();
    void AppendLog(const QString &message);

private slots:
    void on_PB_AddFiles_clicked();
    void on_PB_Clear_clicked();

    void on_listWidget_itemSelectionChanged();
    void on_PB_ProVal_clicked();

    void on_PB_SelectDetectionModel_clicked();

    void on_PB_RunReplace_clicked();

    void on_PB_OverJson_clicked();

    void on_PB_RunTset_clicked();

    void on_PB_RunOCR_clicked();

public:
    QString m_Getfilename;
    QImage m_GetImage;
    QString m_GetSavePath;

public:
    void toSaveCsv();
    void toLoadCsv();
    void toDoRunPP();
    void toDoRunOCR(QString savepth); //切割OCR图象

public slots:
    void read_data();
    void finished_process();
    void error_process();
};

#endif // VALSETFORM_H
