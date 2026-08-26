#include "setnamedialog.h"
#include "release/ui_setnamedialog.h"

#include <QMessageBox>

SetNameDialog::SetNameDialog(QWidget *parent) : QDialog(parent), ui(new Ui::SetNameDialog)
{
    ui->setupUi(this);
    ui->CB_TaskType->addItem(QString(u8"目标检测"), QStringLiteral("detection"));
    ui->CB_TaskType->addItem(QString(u8"图像分类"), QStringLiteral("classification"));
    ui->CB_TaskType->addItem(QString(u8"实例分割"), QStringLiteral("instance_segmentation"));
    ui->CB_TaskType->addItem(QString(u8"语义分割"), QStringLiteral("semantic_segmentation"));
    ui->CB_TaskType->addItem(QString(u8"异常检测"), QStringLiteral("anomaly_detection"));
    ui->CB_TaskType->addItem(QString(u8"线段检测"), QStringLiteral("line_detection"));
    ui->CB_TaskType->addItem(QString(u8"OCR检测"), QStringLiteral("ocr_detection"));
    ui->CB_TaskType->addItem(QString(u8"OCR识别"), QStringLiteral("ocr_recognition"));
    ui->CB_TaskType->addItem(QString(u8"OCR流程"), QStringLiteral("ocr_pipeline"));
}

SetNameDialog::~SetNameDialog()
{
    delete ui;
}

void SetNameDialog::SetRenameMode(const QString &name, const QString &description)
{
    m_renameMode = true;
    ui->LE_TaskName->setText(name);
    ui->PTE_Description->setPlainText(description);
    ui->LB_TaskType->setVisible(false);
    ui->CB_TaskType->setVisible(false);
    setWindowTitle(QString(u8"重命名任务"));
}

QString SetNameDialog::TaskName() const
{
    return ui->LE_TaskName->text().trimmed();
}

QString SetNameDialog::Description() const
{
    return ui->PTE_Description->toPlainText().trimmed();
}

visionaiflow::domain::ProjectType SetNameDialog::TaskType() const
{
    const auto type = visionaiflow::domain::ProjectTypeFromString(ui->CB_TaskType->currentData().toString());
    return type.IsSuccess() ? type.Value() : visionaiflow::domain::ProjectType::Detection;
}

void SetNameDialog::on_PB_Confirm_clicked()
{
    if (TaskName().isEmpty())
    {
        QMessageBox::warning(this, QString(u8"任务名称"), QString(u8"任务名称不能为空"));
        return;
    }
    accept();
}

void SetNameDialog::on_PB_Cancel_clicked()
{
    reject();
}
