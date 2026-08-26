#pragma once

#include "visionaiflow/app/InferenceController.h"
#include "visionaiflow/app/ProjectSession.h"
#include "visionaiflow/app/TrainingController.h"

#include <QMainWindow>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace Ui
{
class InferencePage;
}
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

namespace visionaiflow::app
{
class AnnotationCanvas;

class WorkspaceWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit WorkspaceWindow(QWidget *parent = nullptr);
    ~WorkspaceWindow() override;

private slots:
    void SelectWorkspace();
    void ScanWorkspace();
    void CreateProject();
    void OpenSelectedProject();
    void ImportImages();
    void AddLabel();
    void SelectDatasetImage();
    void SaveAnnotations();
    void UndoAnnotation();
    void RedoAnnotation();
    void StartTraining();
    void CancelTraining();
    void SelectInferenceModel();
    void StartInference();

private:
    void BuildPages();
    void SetPage(int pageIndex);
    void RefreshProjectPage();
    void RefreshDatasetPage();
    void RefreshAnnotationPage();
    void RefreshAvailability();
    void ReportError(const QString &title, const QString &message);
    void OpenProject(const QString &projectRoot);
    void LoadImageForAnnotation(int row);
    int AnnotationCount(const QString &imageId) const;
    void RefreshInferencePage();
    void RenderInferenceResult(const QVector<InferenceDetection> &detections);

    ProjectSession m_session;
    TrainingController m_trainingController;
    InferenceController m_inferenceController;
    QString m_workspaceRoot;
    QListWidget *m_projectList{nullptr};
    QTableWidget *m_datasetTable{nullptr};
    QListWidget *m_annotationImages{nullptr};
    AnnotationCanvas *m_annotationCanvas{nullptr};
    QLabel *m_projectStatus{nullptr};
    QLabel *m_annotationStatus{nullptr};
    QLabel *m_trainingStatus{nullptr};
    QLabel *m_inferenceStatus{nullptr};
    QStackedWidget *m_pages{nullptr};
    QPushButton *m_annotationNavigation{nullptr};
    QPushButton *m_datasetNavigation{nullptr};
    QPushButton *m_trainingNavigation{nullptr};
    QPushButton *m_inferenceNavigation{nullptr};
    QSpinBox *m_epochSpinBox{nullptr};
    QSpinBox *m_batchSpinBox{nullptr};
    QDoubleSpinBox *m_learningRateSpinBox{nullptr};
    QCheckBox *m_horizontalFlipCheckBox{nullptr};
    QLineEdit *m_resumeCheckpointEdit{nullptr};
    QPushButton *m_startTrainingButton{nullptr};
    QPushButton *m_cancelTrainingButton{nullptr};
    QLineEdit *m_inferenceModelEdit{nullptr};
    QComboBox *m_inferenceImageComboBox{nullptr};
    QPushButton *m_startInferenceButton{nullptr};
    QLabel *m_inferencePreview{nullptr};
    QTableWidget *m_inferenceTable{nullptr};
    Ui::InferencePage *m_inferenceUi{nullptr};
};
}
