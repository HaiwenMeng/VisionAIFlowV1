#pragma once

#include "ytyolodefine.h"

#include <QImage>
#include <QProcess>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace Ui
{
class YoloeValForm;
}

class YoloeValForm final : public QWidget
{
    Q_OBJECT

public:
    explicit YoloeValForm(QWidget *parent = nullptr);
    ~YoloeValForm() override;

    void setTaskName(const QString &taskName);
    void initialize();

private:
    enum class RunningOperation
    {
        None,
        GenerateVpe,
        InferSingle,
        InferBatch,
        InferTextSingle,
        InferTextBatch
    };

    void appendLog(const QString &message);
    void showError(const QString &title, const QString &errorMessage);
    void setBusy(bool busy);
    bool validatePythonRuntime(QString *pythonPath, QString *scriptPath, QString *errorMessage) const;
    bool validateBackbone(QString *modelPath, QString *errorMessage) const;
    void saveImageList() const;
    bool prepareInferenceSourceList(bool singleImage, QString *sourceListPath, QString *errorMessage) const;
    void loadImageList();
    QString imageListPath() const;
    QString singleImageListPath() const;
    QString vpePath() const;
    QString backboneResultDirectory() const;
    QString backboneResultPath(const QString &imagePath) const;
    void setTemplateImage(const QString &imagePath);
    QVector<QRectF> promptRects() const;
    QString promptSignature() const;
    QString currentBackbonePath() const;
    bool hasCurrentVpe(QString *errorMessage) const;
    void invalidateVpe(const QString &reason);
    void refreshResultCanvas();
    void refreshBackboneResultCanvas();
    void startPythonOperation(RunningOperation operation, const QStringList &arguments);
    void processOutput();
    void processOutputLine(const QString &line);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void selectBackbone();
    void addImages();
    void clearImages();
    void selectImage();
    void selectTemplateFromCurrentImage();
    void selectTemplateFromFile();
    void generateVpe();
    void deleteVpe();
    void inferCurrentImage();
    void inferAllImages();
    void inferBackboneCurrentImage();
    void inferBackboneImages();
    void runVpeInference(bool singleImage);
    void runBackboneInference(bool singleImage);

    Ui::YoloeValForm *ui;
    QProcess *m_process;
    RunningOperation m_runningOperation = RunningOperation::None;
    QString m_processOutputBuffer;
    QString m_taskName;
    YtYoloSetPro m_labelConfig;
    YtRoiLabelSet m_resultLabels;
    YtSetShowtObj m_resultOverlay;
    YtRoiLabelSet m_backboneResultLabels;
    YtSetShowtObj m_backboneResultOverlay;
    QString m_currentImagePath;
    QString m_templateImagePath;
    QImage m_templateImage;
    QString m_generatedBackbonePath;
    QString m_generatedTemplatePath;
    QString m_generatedPromptSignature;
    bool m_errorReported = false;
};
