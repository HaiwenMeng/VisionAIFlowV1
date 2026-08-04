#include "visionaiflow/app/CreateProjectDialog.h"

#include "ui_CreateProjectDialog.h"
#include "visionaiflow/project_store/ProjectStore.h"

#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QUuid>

namespace visionaiflow::app
{
CreateProjectDialog::CreateProjectDialog(QWidget *parent) : QDialog(parent), ui(new Ui::CreateProjectDialog)
{
    ui->setupUi(this);
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
    const auto definition = visionaiflow::project_store::ProjectDefinition{QUuid::createUuid().toString(QUuid::WithoutBraces), projectName, type.Value(), classificationMode, 1};
    visionaiflow::project_store::ProjectStore store;
    const auto created = store.Create(QDir(ui->rootEdit->text()).filePath(projectName), definition);
    if (!created.IsSuccess())
    {
        QMessageBox::critical(this, QStringLiteral("Create project"), QString::fromStdString(created.Failure().message));
        return;
    }
    accept();
}
}
