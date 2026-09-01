#ifndef AUTOLABELPROJECT_APP_MAINWINDOW_H
#define AUTOLABELPROJECT_APP_MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

#include "app/AppTypes.h"
#include "inference/SamTypes.h"

class QLabel;
class QComboBox;
class QListWidgetItem;
class QResizeEvent;
class QShowEvent;
class QThread;
class SamInferenceWorker;
class StartupOverlay;

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setTaskName(const QString &taskName);
    void ensureSamInitialized();
    void releaseSam3();

private slots:
    void onInitializeBridgeClicked();
    void onReleaseModelClicked();
    void onAddDataSheetClicked();
    void onRemoveDataSheetClicked();
    void onDataSheetSelectionChanged(int row);
    void onDataSheetItemChanged(QListWidgetItem *item);
    void onImageSelectionChanged(int row);

    void onDeleteAnnotationClicked();
    void onFixAnnotationClicked();
    void onAddLabelClicked();
    void onDeleteLabelClicked();
    void onClearAllAnnotationsClicked();
    void onFirstImageClicked();
    void onPreviousImageClicked();
    void onDeleteCurrentImageClicked();
    void onNextImageClicked();
    void onFinalImageClicked();
    void onInferByRectsClicked();
    void onClearRectsClicked();
    void onAnnotationModeChanged(int index);
    void onAnnotationShapeTypeChanged(int index);
    void onImageFilterChanged(int index);
    void onCurrentLabelFilterChanged(int index);

    void onPointPromptRequested(const QPointF &imagePoint);
    void onRectPromptRequested(const QRectF &imageRect);
    void onPolygonPromptRequested(const QPolygonF &imagePolygon);
    void onPolygonDraftRejected(const QString &message);
    void onAnnotationSelectionChanged(int annotationIndex);
    void onImageViewportChanged(const QString &statusText);
    void onSamInitializeFinished(bool success, const QString &errorMessage);
    void onSamCurrentImageFinished(const QString &imagePath, bool success, const QString &errorMessage);
    void
    onSamPointInferenceFinished(const TrtSam3InferResult &result, const QString &labelName, const QString &imagePath);
    void
    onSamRectInferenceFinished(const TrtSam3InferResult &result, const QString &labelName, const QString &imagePath);
    void
    onSamRectsInferenceFinished(const TrtSam3InferResult &result, const QString &labelName, const QString &imagePath);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void setupStatusBarWidgets();
    void setupStartupOverlay();
    void setupInferenceWorker();
    void applyStaticTextAndIcons();
    void setupConnections();
    void appendLog(const QString &message);
    void applyNativeTitleBarTheme();
    void startSamInitialization(bool automatic);
    void requestSetCurrentImageForWorker();
    bool ensureModelReadyForInference();
    void syncImageStatusText();

    void updateWindowTitle();
    void setWorkingDirectory(const QString &folderPath);
    void refreshDataSheetList();
    void addDataSheetItem(const QString &dataSheetName, Qt::CheckState checkState);
    void saveDataSheetCheckState();
    QString taskDataSheetPath(const QString &dataSheetName) const;
    void refreshImageList();
    bool loadImageByPath(const QString &imagePath);
    bool cleanupEmptyAnnotationFile(const QString &imagePath, const QString &reason);
    void ensureImageFilterItems();
    void ensureAnnotationShapeTypeItems();

    bool loadLabelConfig();
    bool saveLabelConfig();
    void refreshLabelList();
    bool reloadAnnotationsForCurrentImage();
    void refreshAnnotationList();
    void updateAnnotationColors();
    void updateStatusSummary(const QString &message = QString());
    void setModelStatusText(const QString &text);
    void setImageStatusText(const QString &text);
    int colorForLabel(const QString &label) const;
    enum class AnnotationMode
    {
        Auto,
        Manual,
        SmallTarget,
        MultiTarget
    };
    AnnotationMode currentAnnotationMode() const;
    AnnotationShapeType currentAnnotationShapeType() const;
    enum class ImageFilterMode
    {
        All,
        Annotated,
        Unannotated,
        ContainsCurrentLabel
    };
    ImageFilterMode currentImageFilterMode() const;
    bool isAutoAnnotationMode() const;
    bool isManualPolygonMode() const;
    void updateAnnotationShapeControls();
    void updateModeControls();
    void clearPendingMultiRects();
    void clearCurrentImageState();
    bool saveManualRectAnnotation(const QRectF &imageRect, const QString &labelName);
    bool saveManualPolygonAnnotation(const QPolygonF &imagePolygon, const QString &labelName);
    QString currentSelectedLabel(QString *errorMessage = nullptr) const;
    QList<AnnotationObject> nmsAnnotations(const QList<AnnotationObject> &annotations) const;
    bool normalizeAnnotationsForImage(const QString &imagePath);
    bool normalizeAnnotationsForCurrentImage();
    QList<AnnotationObject> annotationsFromSamResult(const TrtSam3InferResult &result,
                                                     const QString &labelName,
                                                     QString *errorMessage = nullptr);
    bool saveSamResultAnnotations(const TrtSam3InferResult &result,
                                  const QString &labelName,
                                  const QString &imagePath = QString());

    Ui::MainWindow *ui = nullptr;
    QThread *m_inferenceThread = nullptr;
    SamInferenceWorker *m_inferenceWorker = nullptr;
    StartupOverlay *m_startupOverlay = nullptr;

    QString m_taskName;
    QString m_workingDir;
    QStringList m_allImageFilePaths;
    QStringList m_imageFilePaths;
    QString m_currentImagePath;
    QString m_workerCurrentImagePath;
    QString m_pendingWorkerImagePath;
    QVector<QRectF> m_pendingMultiRects;

    QList<AnnotationObject> m_annotations;
    LabelConfig m_labelConfig;
    bool m_modelInitialized = false;
    bool m_modelInitializing = false;
    bool m_inferenceBusy = false;
    bool m_automaticInitialization = false;
    bool m_ignoreNextInitializeFinished = false;

    QLabel *m_modelStatusLabel = nullptr;
    QLabel *m_imageStatusLabel = nullptr;
    QLabel *m_folderStatusLabel = nullptr;
    QLabel *m_annoShapeTypeLabel = nullptr;
    QComboBox *m_annoShapeTypeCombo = nullptr;
};

#endif // AUTOLABELPROJECT_APP_MAINWINDOW_H
