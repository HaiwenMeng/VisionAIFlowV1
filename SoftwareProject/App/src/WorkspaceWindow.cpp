#include "visionaiflow/app/WorkspaceWindow.h"

#include "visionaiflow/app/AnnotationCanvas.h"
#include "visionaiflow/app/CreateProjectDialog.h"
#include "visionaiflow/annotation/AnnotationStore.h"
#include "visionaiflow/project_store/DatasetIndex.h"
#include "visionaiflow/project_store/LabelStore.h"
#include "ui_InferencePage.h"

#include <QDir>
#include <QDirIterator>
#include <QAbstractItemView>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QHeaderView>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

namespace visionaiflow::app
{
namespace
{
QWidget *CreatePage(QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setLayout(new QVBoxLayout());
    return page;
}

QLabel *CreateTitle(const QString &text, QWidget *parent)
{
    auto *title = new QLabel(text, parent);
    QFont font = title->font();
    font.setPointSize(16);
    font.setBold(true);
    title->setFont(font);
    return title;
}
}

WorkspaceWindow::WorkspaceWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_session(this)
    , m_trainingController(this)
    , m_inferenceController(this)
{
    setWindowTitle(QStringLiteral("VisionAIFlow"));
    resize(1280, 820);
    BuildPages();
    connect(&m_session, &ProjectSession::ProjectChanged, this, [this]()
    {
        RefreshProjectPage();
        RefreshDatasetPage();
        RefreshAnnotationPage();
        RefreshAvailability();
        RefreshInferencePage();
    });
    connect(&m_session, &ProjectSession::RefreshFailed, this, [this](const QString &message)
    {
        ReportError(QStringLiteral("Project refresh failed"), message);
    });
    connect(&m_trainingController, &TrainingController::Progress, this, [this](int epoch, int step, double loss, double boxLoss, double classLoss, int positives, double iou)
    {
        m_trainingStatus->setText(QString(u8"训练中: epoch %1/%2, step %3, loss %4, box %5, class %6, positives %7, IoU %8")
                                      .arg(epoch)
                                      .arg(m_epochSpinBox->value())
                                      .arg(step)
                                      .arg(loss, 0, 'g', 8)
                                      .arg(boxLoss, 0, 'g', 8)
                                      .arg(classLoss, 0, 'g', 8)
                                      .arg(positives)
                                      .arg(iou, 0, 'g', 6));
    });
    connect(&m_trainingController, &TrainingController::Completed, this, [this](const QString &modelPath, const QString &bestCheckpointPath)
    {
        m_trainingStatus->setText(QString(u8"训练完成。ONNX: %1\n最佳断点: %2").arg(modelPath, bestCheckpointPath));
        m_inferenceModelEdit->setText(modelPath);
    });
    connect(&m_trainingController, &TrainingController::Failed, this, [this](const QString &message)
    {
        m_trainingStatus->setText(QString(u8"训练失败: %1").arg(message));
    });
    connect(&m_trainingController, &TrainingController::Cancelled, this, [this]()
    {
        m_trainingStatus->setText(QString(u8"训练已取消。"));
    });
    connect(&m_trainingController, &TrainingController::StateChanged, this, [this](bool running)
    {
        m_startTrainingButton->setEnabled(!running);
        m_cancelTrainingButton->setEnabled(running);
    });
    connect(&m_inferenceController, &InferenceController::Completed, this, &WorkspaceWindow::RenderInferenceResult);
    connect(&m_inferenceController, &InferenceController::Failed, this, [this](const QString &message)
    {
        m_inferenceStatus->setText(QString(u8"推理失败: %1").arg(message));
    });
    connect(&m_inferenceController, &InferenceController::StateChanged, this, [this](bool running)
    {
        m_startInferenceButton->setEnabled(!running);
    });
    RefreshAvailability();
}

WorkspaceWindow::~WorkspaceWindow()
{
    delete m_inferenceUi;
}

void WorkspaceWindow::BuildPages()
{
    auto *root = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(root);
    auto *navigation = new QWidget(root);
    auto *navigationLayout = new QVBoxLayout(navigation);
    auto *projectButton = new QPushButton(QStringLiteral("Project management"), navigation);
    m_annotationNavigation = new QPushButton(QStringLiteral("Annotation"), navigation);
    m_datasetNavigation = new QPushButton(QStringLiteral("Dataset"), navigation);
    m_trainingNavigation = new QPushButton(QStringLiteral("Training"), navigation);
    m_inferenceNavigation = new QPushButton(QStringLiteral("Inference"), navigation);
    navigationLayout->addWidget(projectButton);
    navigationLayout->addWidget(m_annotationNavigation);
    navigationLayout->addWidget(m_datasetNavigation);
    navigationLayout->addWidget(m_trainingNavigation);
    navigationLayout->addWidget(m_inferenceNavigation);
    navigationLayout->addStretch();
    m_pages = new QStackedWidget(root);
    rootLayout->addWidget(navigation);
    rootLayout->addWidget(m_pages, 1);
    setCentralWidget(root);

    auto *projectPage = CreatePage(m_pages);
    projectPage->layout()->addWidget(CreateTitle(QStringLiteral("Project management"), projectPage));
    m_projectStatus = new QLabel(projectPage);
    projectPage->layout()->addWidget(m_projectStatus);
    auto *projectActions = new QHBoxLayout();
    auto *selectWorkspaceButton = new QPushButton(QStringLiteral("Select workspace"), projectPage);
    auto *scanButton = new QPushButton(QStringLiteral("Scan projects"), projectPage);
    auto *createButton = new QPushButton(QStringLiteral("Create detection project"), projectPage);
    auto *openButton = new QPushButton(QStringLiteral("Open selected project"), projectPage);
    projectActions->addWidget(selectWorkspaceButton);
    projectActions->addWidget(scanButton);
    projectActions->addWidget(createButton);
    projectActions->addWidget(openButton);
    projectPage->layout()->addItem(projectActions);
    m_projectList = new QListWidget(projectPage);
    static_cast<QVBoxLayout *>(projectPage->layout())->addWidget(m_projectList, 1);
    m_pages->addWidget(projectPage);

    auto *annotationPage = CreatePage(m_pages);
    annotationPage->layout()->addWidget(CreateTitle(QStringLiteral("Bounding box annotation"), annotationPage));
    m_annotationStatus = new QLabel(annotationPage);
    annotationPage->layout()->addWidget(m_annotationStatus);
    auto *annotationSplitter = new QSplitter(annotationPage);
    m_annotationImages = new QListWidget(annotationSplitter);
    m_annotationCanvas = new AnnotationCanvas(annotationSplitter);
    annotationSplitter->addWidget(m_annotationImages);
    annotationSplitter->addWidget(m_annotationCanvas);
    annotationSplitter->setStretchFactor(1, 1);
    static_cast<QVBoxLayout *>(annotationPage->layout())->addWidget(annotationSplitter, 1);
    auto *annotationActions = new QHBoxLayout();
    auto *addLabelButton = new QPushButton(QStringLiteral("Add label"), annotationPage);
    auto *undoButton = new QPushButton(QStringLiteral("Undo"), annotationPage);
    auto *redoButton = new QPushButton(QStringLiteral("Redo"), annotationPage);
    auto *saveButton = new QPushButton(QStringLiteral("Save annotations"), annotationPage);
    annotationActions->addWidget(addLabelButton);
    annotationActions->addWidget(undoButton);
    annotationActions->addWidget(redoButton);
    annotationActions->addWidget(saveButton);
    annotationActions->addStretch();
    annotationPage->layout()->addItem(annotationActions);
    m_pages->addWidget(annotationPage);

    auto *datasetPage = CreatePage(m_pages);
    datasetPage->layout()->addWidget(CreateTitle(QStringLiteral("Dataset"), datasetPage));
    auto *importButton = new QPushButton(QStringLiteral("Import images"), datasetPage);
    datasetPage->layout()->addWidget(importButton);
    m_datasetTable = new QTableWidget(datasetPage);
    m_datasetTable->setColumnCount(5);
    m_datasetTable->setHorizontalHeaderLabels({QStringLiteral("File"), QStringLiteral("Size"), QStringLiteral("Imported UTC"), QStringLiteral("Annotations"), QStringLiteral("Project path")});
    m_datasetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    static_cast<QVBoxLayout *>(datasetPage->layout())->addWidget(m_datasetTable, 1);
    m_pages->addWidget(datasetPage);

    auto *trainingPage = CreatePage(m_pages);
    trainingPage->layout()->addWidget(CreateTitle(QStringLiteral("Training"), trainingPage));
    m_trainingStatus = new QLabel(trainingPage);
    m_trainingStatus->setWordWrap(true);
    trainingPage->layout()->addWidget(m_trainingStatus);
    auto *trainingParameters = new QHBoxLayout();
    m_epochSpinBox = new QSpinBox(trainingPage);
    m_epochSpinBox->setRange(1, 100000);
    m_epochSpinBox->setValue(20);
    m_batchSpinBox = new QSpinBox(trainingPage);
    m_batchSpinBox->setRange(1, 1024);
    m_batchSpinBox->setValue(4);
    m_learningRateSpinBox = new QDoubleSpinBox(trainingPage);
    m_learningRateSpinBox->setRange(0.000001, 1.0);
    m_learningRateSpinBox->setDecimals(6);
    m_learningRateSpinBox->setValue(0.001);
    m_horizontalFlipCheckBox = new QCheckBox(QStringLiteral("Horizontal flip"), trainingPage);
    m_horizontalFlipCheckBox->setChecked(true);
    trainingParameters->addWidget(new QLabel(QStringLiteral("Epochs"), trainingPage));
    trainingParameters->addWidget(m_epochSpinBox);
    trainingParameters->addWidget(new QLabel(QStringLiteral("Batch size"), trainingPage));
    trainingParameters->addWidget(m_batchSpinBox);
    trainingParameters->addWidget(new QLabel(QStringLiteral("Learning rate"), trainingPage));
    trainingParameters->addWidget(m_learningRateSpinBox);
    trainingParameters->addWidget(m_horizontalFlipCheckBox);
    trainingPage->layout()->addItem(trainingParameters);
    auto *resumeLayout = new QHBoxLayout();
    m_resumeCheckpointEdit = new QLineEdit(trainingPage);
    m_resumeCheckpointEdit->setPlaceholderText(QStringLiteral("Optional checkpoint path"));
    resumeLayout->addWidget(new QLabel(QStringLiteral("Resume checkpoint"), trainingPage));
    resumeLayout->addWidget(m_resumeCheckpointEdit, 1);
    trainingPage->layout()->addItem(resumeLayout);
    auto *trainingActions = new QHBoxLayout();
    m_startTrainingButton = new QPushButton(QStringLiteral("Start 640 training"), trainingPage);
    m_cancelTrainingButton = new QPushButton(QStringLiteral("Cancel training"), trainingPage);
    m_cancelTrainingButton->setEnabled(false);
    trainingActions->addWidget(m_startTrainingButton);
    trainingActions->addWidget(m_cancelTrainingButton);
    trainingActions->addStretch();
    trainingPage->layout()->addItem(trainingActions);
    m_pages->addWidget(trainingPage);

    auto *inferencePage = new QWidget(m_pages);
    m_inferenceUi = new Ui::InferencePage();
    m_inferenceUi->setupUi(inferencePage);
    m_inferenceModelEdit = m_inferenceUi->modelPathEdit;
    m_inferenceImageComboBox = m_inferenceUi->imageComboBox;
    m_startInferenceButton = m_inferenceUi->startInferenceButton;
    m_inferenceStatus = m_inferenceUi->statusLabel;
    m_inferencePreview = m_inferenceUi->previewLabel;
    m_inferenceTable = m_inferenceUi->detectionTable;
    m_inferenceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_inferenceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_pages->addWidget(inferencePage);

    connect(projectButton, &QPushButton::clicked, this, [this]() { SetPage(0); });
    connect(m_annotationNavigation, &QPushButton::clicked, this, [this]() { SetPage(1); });
    connect(m_datasetNavigation, &QPushButton::clicked, this, [this]() { SetPage(2); });
    connect(m_trainingNavigation, &QPushButton::clicked, this, [this]() { SetPage(3); });
    connect(m_inferenceNavigation, &QPushButton::clicked, this, [this]() { SetPage(4); });
    connect(selectWorkspaceButton, &QPushButton::clicked, this, &WorkspaceWindow::SelectWorkspace);
    connect(scanButton, &QPushButton::clicked, this, &WorkspaceWindow::ScanWorkspace);
    connect(createButton, &QPushButton::clicked, this, &WorkspaceWindow::CreateProject);
    connect(openButton, &QPushButton::clicked, this, &WorkspaceWindow::OpenSelectedProject);
    connect(importButton, &QPushButton::clicked, this, &WorkspaceWindow::ImportImages);
    connect(addLabelButton, &QPushButton::clicked, this, &WorkspaceWindow::AddLabel);
    connect(m_annotationImages, &QListWidget::currentRowChanged, this, [this](int row) { LoadImageForAnnotation(row); });
    connect(undoButton, &QPushButton::clicked, this, &WorkspaceWindow::UndoAnnotation);
    connect(redoButton, &QPushButton::clicked, this, &WorkspaceWindow::RedoAnnotation);
    connect(saveButton, &QPushButton::clicked, this, &WorkspaceWindow::SaveAnnotations);
    connect(m_startTrainingButton, &QPushButton::clicked, this, &WorkspaceWindow::StartTraining);
    connect(m_cancelTrainingButton, &QPushButton::clicked, this, &WorkspaceWindow::CancelTraining);
    connect(m_inferenceUi->browseModelButton, &QPushButton::clicked, this, &WorkspaceWindow::SelectInferenceModel);
    connect(m_startInferenceButton, &QPushButton::clicked, this, &WorkspaceWindow::StartInference);
    connect(m_annotationCanvas, &AnnotationCanvas::OperationFailed, this, [this](const QString &message)
    {
        ReportError(QStringLiteral("Annotation failed"), message);
    });
}

void WorkspaceWindow::SetPage(int pageIndex)
{
    if (pageIndex != 0 && !m_session.HasProject())
    {
        ReportError(QStringLiteral("Project required"), QStringLiteral("Open a detection project before using this page."));
        return;
    }
    m_pages->setCurrentIndex(pageIndex);
}

void WorkspaceWindow::SelectWorkspace()
{
    const QString selected = QFileDialog::getExistingDirectory(this, QStringLiteral("Select workspace"), m_workspaceRoot);
    if (selected.isEmpty())
    {
        return;
    }
    m_workspaceRoot = selected;
    ScanWorkspace();
}

void WorkspaceWindow::ScanWorkspace()
{
    m_projectList->clear();
    if (m_workspaceRoot.isEmpty())
    {
        ReportError(QStringLiteral("Workspace required"), QStringLiteral("Select a workspace before scanning projects."));
        return;
    }
    QDirIterator iterator(m_workspaceRoot, {QStringLiteral("project.json")}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString definitionPath = iterator.next();
        const QString projectRoot = QFileInfo(definitionPath).dir().absolutePath();
        ProjectSession candidate;
        const auto opened = candidate.Open(projectRoot);
        if (opened.IsSuccess())
        {
            auto *item = new QListWidgetItem(candidate.Definition().name, m_projectList);
            item->setData(Qt::UserRole, projectRoot);
            item->setToolTip(projectRoot);
        }
    }
    if (m_projectList->count() == 0)
    {
        m_projectStatus->setText(QStringLiteral("No valid detection.yolo11.grid.v1 projects were found in the workspace."));
    }
}

void WorkspaceWindow::CreateProject()
{
    CreateProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    const QString projectRoot = dialog.CreatedProjectRoot();
    if (projectRoot.isEmpty())
    {
        ReportError(QStringLiteral("Project creation failed"), QStringLiteral("The project dialog did not return the project path."));
        return;
    }
    m_workspaceRoot = QFileInfo(projectRoot).dir().absolutePath();
    OpenProject(projectRoot);
    ScanWorkspace();
}

void WorkspaceWindow::OpenSelectedProject()
{
    auto *item = m_projectList->currentItem();
    if (item == nullptr)
    {
        ReportError(QStringLiteral("Project required"), QStringLiteral("Select a project from the workspace list."));
        return;
    }
    OpenProject(item->data(Qt::UserRole).toString());
}

void WorkspaceWindow::OpenProject(const QString &projectRoot)
{
    const auto opened = m_session.Open(projectRoot);
    if (!opened.IsSuccess())
    {
        ReportError(QStringLiteral("Open project failed"), QString::fromStdString(opened.Failure().message));
    }
}

void WorkspaceWindow::ImportImages()
{
    if (!m_session.HasProject())
    {
        return;
    }
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Import images"), QString(), QStringLiteral("Images (*.bmp *.jpg *.jpeg *.png *.tif *.tiff)"));
    if (paths.isEmpty())
    {
        return;
    }
    project_store::DatasetIndex index;
    for (const QString &path : paths)
    {
        const auto imported = index.ImportImage(m_session.ProjectRoot(), path);
        if (!imported.IsSuccess())
        {
            ReportError(QStringLiteral("Image import failed"), QString::fromStdString(imported.Failure().message));
            return;
        }
    }
    const auto refreshed = m_session.Refresh();
    if (!refreshed.IsSuccess())
    {
        ReportError(QStringLiteral("Dataset refresh failed"), QString::fromStdString(refreshed.Failure().message));
        return;
    }
    RefreshDatasetPage();
    RefreshAnnotationPage();
}

void WorkspaceWindow::AddLabel()
{
    if (!m_session.HasProject())
    {
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Add label"), QStringLiteral("Label name"), QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted || name.isEmpty())
    {
        return;
    }
    const QColor color = QColorDialog::getColor(Qt::green, this, QStringLiteral("Select label color"));
    if (!color.isValid())
    {
        return;
    }
    project_store::LabelStore labels;
    const auto added = labels.AddLabel(m_session.ProjectRoot(), name, color.name(QColor::HexRgb));
    if (!added.IsSuccess())
    {
        ReportError(QStringLiteral("Label creation failed"), QString::fromStdString(added.Failure().message));
        return;
    }
    const auto refreshed = m_session.Refresh();
    if (!refreshed.IsSuccess())
    {
        ReportError(QStringLiteral("Project refresh failed"), QString::fromStdString(refreshed.Failure().message));
        return;
    }
    RefreshAnnotationPage();
}

void WorkspaceWindow::RefreshProjectPage()
{
    if (!m_session.HasProject())
    {
        m_projectStatus->setText(QStringLiteral("No project is open."));
        return;
    }
    m_projectStatus->setText(QStringLiteral("Current project: %1\n%2\nModel: %3")
                                 .arg(m_session.Definition().name, m_session.ProjectRoot(), m_session.Definition().modelId));
}

void WorkspaceWindow::RefreshDatasetPage()
{
    m_datasetTable->setRowCount(0);
    if (!m_session.HasProject())
    {
        return;
    }
    const auto &images = m_session.Images();
    m_datasetTable->setRowCount(static_cast<int>(images.size()));
    for (int row = 0; row < static_cast<int>(images.size()); ++row)
    {
        const auto &image = images.at(static_cast<size_t>(row));
        m_datasetTable->setItem(row, 0, new QTableWidgetItem(image.fileName));
        m_datasetTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("%1 x %2").arg(image.size.width()).arg(image.size.height())));
        m_datasetTable->setItem(row, 2, new QTableWidgetItem(image.importedUtc));
        m_datasetTable->setItem(row, 3, new QTableWidgetItem(QString::number(AnnotationCount(image.imageId))));
        m_datasetTable->setItem(row, 4, new QTableWidgetItem(image.relativePath));
    }
    m_datasetTable->resizeColumnsToContents();
}

void WorkspaceWindow::RefreshAnnotationPage()
{
    m_annotationImages->clear();
    if (!m_session.HasProject())
    {
        return;
    }
    for (const auto &image : m_session.Images())
    {
        auto *item = new QListWidgetItem(image.fileName, m_annotationImages);
        item->setData(Qt::UserRole, image.imageId);
    }
    if (!m_session.Images().empty())
    {
        m_annotationImages->setCurrentRow(0);
    }
    else
    {
        m_annotationStatus->setText(QStringLiteral("Import an image before drawing bounding boxes."));
    }
}

void WorkspaceWindow::RefreshAvailability()
{
    const bool hasProject = m_session.HasProject();
    m_annotationNavigation->setEnabled(hasProject);
    m_datasetNavigation->setEnabled(hasProject);
    m_trainingNavigation->setEnabled(hasProject);
    m_inferenceNavigation->setEnabled(hasProject);
    if (!hasProject)
    {
        m_annotationStatus->setText(QStringLiteral("Open a detection project to annotate images."));
        if (!m_trainingController.IsRunning())
        {
            m_trainingStatus->setText(QString(u8"打开检测项目后可开始训练。"));
        }
        if (!m_inferenceController.IsRunning())
        {
            m_inferenceStatus->setText(QString(u8"打开检测项目后可进行 OpenVINO 推理。"));
        }
    }
    else
    {
        if (!m_trainingController.IsRunning())
        {
            m_trainingStatus->setText(QString(u8"设置训练参数后可开始 yolodet 训练。"));
        }
        if (!m_inferenceController.IsRunning())
        {
            m_inferenceStatus->setText(QString(u8"请选择 ONNX 模型和项目图片。"));
        }
    }
}

void WorkspaceWindow::SelectDatasetImage()
{
    LoadImageForAnnotation(m_annotationImages->currentRow());
}

void WorkspaceWindow::LoadImageForAnnotation(int row)
{
    if (!m_session.HasProject() || row < 0 || row >= static_cast<int>(m_session.Images().size()))
    {
        return;
    }
    const auto &image = m_session.Images().at(static_cast<size_t>(row));
    const QImage loaded(QDir(m_session.ProjectRoot()).filePath(image.relativePath));
    if (loaded.isNull())
    {
        ReportError(QStringLiteral("Image loading failed"), QStringLiteral("The imported image could not be opened: %1").arg(image.relativePath));
        return;
    }
    const auto imageSet = m_annotationCanvas->SetImage(loaded);
    const auto locationSet = m_annotationCanvas->SetDocumentLocation(m_session.ProjectRoot(), image.imageId);
    if (!imageSet.IsSuccess() || !locationSet.IsSuccess())
    {
        ReportError(QStringLiteral("Annotation setup failed"), imageSet.IsSuccess() ? QString::fromStdString(locationSet.Failure().message) : QString::fromStdString(imageSet.Failure().message));
        return;
    }
    if (m_session.Labels().empty())
    {
        m_annotationStatus->setText(QStringLiteral("Add a label before drawing bounding boxes."));
        return;
    }
    const auto active = m_annotationCanvas->SetActiveLabel(m_session.Labels().front().labelId);
    const auto tool = m_annotationCanvas->SetTool(AnnotationCanvas::Tool::BoundingBox);
    const auto annotations = m_annotationCanvas->LoadAnnotations();
    if (!active.IsSuccess() || !tool.IsSuccess() || !annotations.IsSuccess())
    {
        ReportError(QStringLiteral("Annotation loading failed"), !active.IsSuccess() ? QString::fromStdString(active.Failure().message) : (!tool.IsSuccess() ? QString::fromStdString(tool.Failure().message) : QString::fromStdString(annotations.Failure().message)));
        return;
    }
    m_annotationStatus->setText(QStringLiteral("Drawing bounding boxes for %1. Active label: %2")
                                    .arg(image.fileName, m_session.Labels().front().name));
}

void WorkspaceWindow::SaveAnnotations()
{
    const auto saved = m_annotationCanvas->SaveAnnotations();
    if (!saved.IsSuccess())
    {
        ReportError(QStringLiteral("Annotation save failed"), QString::fromStdString(saved.Failure().message));
        return;
    }
    RefreshDatasetPage();
}

void WorkspaceWindow::UndoAnnotation()
{
    const auto undone = m_annotationCanvas->Undo();
    if (!undone.IsSuccess())
    {
        ReportError(QStringLiteral("Annotation undo failed"), QString::fromStdString(undone.Failure().message));
    }
}

void WorkspaceWindow::RedoAnnotation()
{
    const auto redone = m_annotationCanvas->Redo();
    if (!redone.IsSuccess())
    {
        ReportError(QStringLiteral("Annotation redo failed"), QString::fromStdString(redone.Failure().message));
    }
}

void WorkspaceWindow::StartTraining()
{
    if (!m_session.HasProject())
    {
        ReportError(QStringLiteral("Project required"), QStringLiteral("Open a detection project before training."));
        return;
    }
    if (m_session.Labels().empty())
    {
        ReportError(QStringLiteral("Labels required"), QStringLiteral("Add at least one detection label before training."));
        return;
    }
    bool hasBoundingBox = false;
    for (const auto &image : m_session.Images())
    {
        if (AnnotationCount(image.imageId) > 0)
        {
            hasBoundingBox = true;
            break;
        }
    }
    if (!hasBoundingBox)
    {
        ReportError(QStringLiteral("Annotations required"), QStringLiteral("Add and save at least one bounding box annotation before training."));
        return;
    }
    m_trainingStatus->setText(QString(u8"正在检查 CUDA 并启动训练线程..."));
    m_trainingController.Start({m_session.ProjectRoot(),
                                m_epochSpinBox->value(),
                                m_batchSpinBox->value(),
                                m_learningRateSpinBox->value(),
                                m_horizontalFlipCheckBox->isChecked(),
                                m_resumeCheckpointEdit->text().trimmed()});
}

void WorkspaceWindow::CancelTraining()
{
    m_trainingController.Cancel();
    m_trainingStatus->setText(QString(u8"正在取消训练..."));
}

void WorkspaceWindow::SelectInferenceModel()
{
    const QString modelPath = QFileDialog::getOpenFileName(
        this,
        QString(u8"选择 ONNX 模型"),
        m_inferenceModelEdit->text(),
        QString(u8"ONNX 模型 (*.onnx)"));
    if (!modelPath.isEmpty())
    {
        m_inferenceModelEdit->setText(modelPath);
    }
}

void WorkspaceWindow::StartInference()
{
    if (!m_session.HasProject())
    {
        ReportError(QString(u8"需要项目"), QString(u8"打开检测项目后才能推理。"));
        return;
    }
    if (m_session.Labels().empty())
    {
        ReportError(QString(u8"需要标签"), QString(u8"推理前项目必须至少有一个标签。"));
        return;
    }

    const QString modelPath = m_inferenceModelEdit->text().trimmed();
    const QString imagePath = m_inferenceImageComboBox->currentData().toString();
    if (modelPath.isEmpty() || imagePath.isEmpty())
    {
        ReportError(QString(u8"推理参数无效"), QString(u8"请选择 ONNX 模型和项目图片。"));
        return;
    }

    m_inferenceStatus->setText(QString(u8"正在执行 OpenVINO 推理..."));
    m_inferenceController.Start({modelPath, imagePath, static_cast<int>(m_session.Labels().size())});
}

void WorkspaceWindow::RefreshInferencePage()
{
    if (m_inferenceImageComboBox == nullptr)
    {
        return;
    }

    const QString selectedPath = m_inferenceImageComboBox->currentData().toString();
    m_inferenceImageComboBox->clear();
    if (!m_session.HasProject())
    {
        m_inferencePreview->clear();
        m_inferenceTable->setRowCount(0);
        return;
    }

    for (const auto &image : m_session.Images())
    {
        const QString imagePath = QDir(m_session.ProjectRoot()).filePath(image.relativePath);
        m_inferenceImageComboBox->addItem(image.fileName, imagePath);
    }
    const int selectedIndex = m_inferenceImageComboBox->findData(selectedPath);
    if (selectedIndex >= 0)
    {
        m_inferenceImageComboBox->setCurrentIndex(selectedIndex);
    }
}

void WorkspaceWindow::RenderInferenceResult(const QVector<InferenceDetection> &detections)
{
    const QString imagePath = m_inferenceImageComboBox->currentData().toString();
    QImage image(imagePath);
    if (image.isNull())
    {
        m_inferenceStatus->setText(QString(u8"推理完成，但结果图片无法加载: %1").arg(imagePath));
        return;
    }

    QPainter painter(&image);
    QPen pen(Qt::red);
    pen.setWidth(3);
    painter.setPen(pen);
    for (const InferenceDetection &detection : detections)
    {
        const QString className = detection.classIndex >= 0 && detection.classIndex < static_cast<int>(m_session.Labels().size())
            ? m_session.Labels().at(static_cast<size_t>(detection.classIndex)).name
            : QString(u8"未知类别");
        painter.drawRect(QRectF(
            detection.x1,
            detection.y1,
            detection.x2 - detection.x1,
            detection.y2 - detection.y1));
        painter.drawText(
            QPointF(detection.x1, std::max(14.0F, detection.y1 - 4.0F)),
            QStringLiteral("%1 %2").arg(className).arg(detection.score, 0, 'f', 3));
    }
    painter.end();

    m_inferencePreview->setPixmap(QPixmap::fromImage(image));
    m_inferenceTable->setRowCount(detections.size());
    for (int row = 0; row < detections.size(); ++row)
    {
        const InferenceDetection &detection = detections.at(row);
        const QString className = detection.classIndex >= 0 && detection.classIndex < static_cast<int>(m_session.Labels().size())
            ? m_session.Labels().at(static_cast<size_t>(detection.classIndex)).name
            : QString(u8"未知类别");
        const QStringList values{
            className,
            QString::number(detection.score, 'f', 4),
            QString::number(detection.x1, 'f', 2),
            QString::number(detection.y1, 'f', 2),
            QString::number(detection.x2, 'f', 2),
            QString::number(detection.y2, 'f', 2)};
        for (int column = 0; column < values.size(); ++column)
        {
            m_inferenceTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
    m_inferenceStatus->setText(QString(u8"推理完成，共检测到 %1 个目标。").arg(detections.size()));
}

int WorkspaceWindow::AnnotationCount(const QString &imageId) const
{
    if (!m_session.HasProject())
    {
        return 0;
    }
    annotation::AnnotationStore store;
    const auto loaded = store.Load(m_session.ProjectRoot(), imageId);
    if (!loaded.IsSuccess())
    {
        return 0;
    }
    int count = 0;
    for (const auto &annotation : loaded.Value())
    {
        if (annotation.kind == annotation::AnnotationKind::BoundingBox)
        {
            ++count;
        }
    }
    return count;
}

void WorkspaceWindow::ReportError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}
}
