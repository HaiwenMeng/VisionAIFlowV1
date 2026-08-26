#include "visionaiflow/app/CreateProjectDialog.h"

#include "ui_CreateProjectDialog.h"
#include "visionaiflow/project_store/ProjectStore.h"
#include "visionaiflow/project_store/ProjectMigration.h"

#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QUuid>

namespace visionaiflow::app
{
CreateProjectDialog::CreateProjectDialog(QWidget *parent) : QDialog(parent), ui(new Ui::CreateProjectDialog)
{
    ui->setupUi(this);
    ui->projectTypeComboBox->setItemData(0, QStringLiteral("detection"));
    ui->projectTypeComboBox->setItemData(1, QStringLiteral("classification"));
    ui->projectTypeComboBox->setItemData(2, QStringLiteral("instance_segmentation"));
    ui->projectTypeComboBox->setItemData(3, QStringLiteral("semantic_segmentation"));
    ui->projectTypeComboBox->setItemData(4, QStringLiteral("anomaly_detection"));
    ui->projectTypeComboBox->setItemData(5, QStringLiteral("line_detection"));
    ui->projectTypeComboBox->setItemData(6, QStringLiteral("ocr_detection"));
    ui->projectTypeComboBox->setItemData(7, QStringLiteral("ocr_recognition"));
    ui->projectTypeComboBox->setItemData(8, QStringLiteral("ocr_pipeline"));
    ui->classificationModeComboBox->setItemData(0, QStringLiteral("single_label"));
    ui->classificationModeComboBox->setItemData(1, QStringLiteral("multi_label"));
    connect(ui->browseButton, &QPushButton::clicked, this, &CreateProjectDialog::ChooseDirectory);
    connect(ui->projectTypeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &CreateProjectDialog::UpdateClassificationMode);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &CreateProjectDialog::CreateProject);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    UpdateClassificationMode();
}

CreateProjectDialog::~CreateProjectDialog()
{
    delete ui;
}

QString CreateProjectDialog::CreatedProjectRoot() const
{
    return m_createdProjectRoot;
}

void CreateProjectDialog::ChooseDirectory()
{
    const QString root = QFileDialog::getExistingDirectory(this, QStringLiteral("Select project parent directory"));
    if (!root.isEmpty()) ui->rootEdit->setText(root);
}

void CreateProjectDialog::UpdateClassificationMode()
{
    ui->classificationModeComboBox->setEnabled(ui->projectTypeComboBox->currentData().toString() == QStringLiteral("classification"));
}

void CreateProjectDialog::CreateProject()
{
    const auto type = visionaiflow::project_store::ProjectTypeFromString(ui->projectTypeComboBox->currentData().toString());
    if (!type.IsSuccess())
    {
        QMessageBox::critical(this, QStringLiteral("Create project"), QStringLiteral("Project type selection is invalid."));
        return;
    }
    if (type.Value() != visionaiflow::project_store::ProjectType::Detection)
    {
        QMessageBox::critical(this, QStringLiteral("Create project"), QStringLiteral("Only detection projects are supported by the current application."));
        return;
    }
    visionaiflow::project_store::ClassificationMode classificationMode = visionaiflow::project_store::ClassificationMode::NotApplicable;
    if (type.Value() == visionaiflow::project_store::ProjectType::Classification)
    {
        const auto mode = visionaiflow::project_store::ClassificationModeFromString(ui->classificationModeComboBox->currentData().toString());
        if (!mode.IsSuccess())
        {
            QMessageBox::critical(this, QStringLiteral("Create project"), QStringLiteral("Classification mode selection is invalid."));
            return;
        }
        classificationMode = mode.Value();
    }
    const QString projectName = ui->nameEdit->text().trimmed();
    if (projectName.isEmpty() || ui->rootEdit->text().isEmpty())
    {
        QMessageBox::critical(this, QStringLiteral("Create project"), QStringLiteral("Project name and parent directory are required."));
        return;
    }
    const QString modelId = QStringLiteral("detection.yolo11.grid.v1");
    const auto definition = visionaiflow::project_store::ProjectDefinition{QUuid::createUuid().toString(QUuid::WithoutBraces), projectName, type.Value(), classificationMode, visionaiflow::project_store::CurrentProjectSchemaVersion, modelId};
    visionaiflow::project_store::ProjectStore store;
    const auto created = store.Create(QDir(ui->rootEdit->text()).filePath(projectName), definition);
    if (!created.IsSuccess())
    {
        QMessageBox::critical(this, QStringLiteral("Create project"), QString::fromStdString(created.Failure().message));
        return;
    }
    m_createdProjectRoot = QDir(ui->rootEdit->text()).filePath(projectName);
    accept();
}
}
