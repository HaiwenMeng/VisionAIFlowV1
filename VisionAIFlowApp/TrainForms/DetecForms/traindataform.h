#pragma once

#include <QWidget>

class DataSetForm;
class DetectTrainingController;

namespace Ui
{
class TrainDataForm;
}

class TrainDataForm final : public QWidget
{
    Q_OBJECT

public:
    explicit TrainDataForm(QWidget *parent = nullptr);
    ~TrainDataForm() override;

    void toSetProcessName(const QString &setName);
    void toInitShow();
    bool isrunstate() const;

    QString m_ProcessName;
    DataSetForm *m_DataSetForm{nullptr};

private slots:
    void on_PB_RunTrain_clicked();
    void on_PB_StopRun_clicked();
    void on_PB_ViewPos_clicked();
    void on_PB_EigenCamTest_clicked();
    void on_PB_OnnxOut_clicked();
    void on_PB_OnnxMatch_clicked();
    void on_PB_ModeCopy_clicked();
    void on_PB_Batch0_clicked();
    void on_PB_Batch1_clicked();
    void on_PB_Batch2_clicked();
    void on_PB_Val0_clicked();
    void on_PB_Val1_clicked();

private:
    void InitChart();
    void RefreshPluginList();
    void UpdatePluginUiState();
    void SetTrainingUiState(bool running);
    void AppendLog(const QString &message);
    void ShowUnsupported(const QString &featureName);
    void UpdateTaskProgress(const QString &progress);
    QString WeightsDirectory() const;
    QString SelectedPluginPath() const;
    QString SelectedPluginName() const;

    Ui::TrainDataForm *ui;
    DetectTrainingController *m_trainingController;
    QString m_lastModelPath;
    QString m_lastBestCheckpointPath;
};
