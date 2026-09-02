#pragma once

#include <QWidget>

class QPushButton;
class QEvent;
class QObject;

namespace Ui
{
class MainDlg;
}

class ProJectForm;
class DataSetForm;
class TrainDataForm;
class ValSetForm;
class CommonSetForm;
class MainWindow;
class SystemForm;

class MainDlg final : public QWidget
{
    Q_OBJECT

public:
    explicit MainDlg(QWidget *parent = nullptr);
    ~MainDlg() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void OnTaskSelected(const QString &taskName);
    void on_PB_ProJectMange_clicked();
    void on_PB_LabSet_clicked();
    void on_PB_SemiLabSet_clicked();
    void on_PB_DataSet_clicked();
    void on_PB_TrainPro_clicked();
    void on_PB_ValPro_clicked();
    void on_PB_SystemSet_clicked();

private:
    bool RequireDetectionTask();
    bool RequireGeneratedDataset();
    void SetCurrentNavButton(QPushButton *button);
    void ShowUnavailable(const QString &featureName);
    void ReportError(const QString &errorMessage);
    void UpdateWindowControlButtons();

    Ui::MainDlg *ui;
    ProJectForm *m_projectForm;
    DataSetForm *m_dataSetForm;
    TrainDataForm *m_trainDataForm;
    ValSetForm *m_valSetForm;
    CommonSetForm *m_commonSetForm;
    MainWindow *m_semiAutoAnnoForm = nullptr;
    SystemForm *m_systemForm;
    QString m_taskName;
};
