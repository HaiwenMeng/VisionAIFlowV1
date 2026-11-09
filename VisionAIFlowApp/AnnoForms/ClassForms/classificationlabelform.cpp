#include "classificationlabelform.h"

#include "taskrepository.h"
#include "release/ui_classificationlabelform.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>

ClassificationLabelForm::ClassificationLabelForm(QWidget *parent) : QWidget(parent), ui(new Ui::ClassificationLabelForm)
{
    ui->setupUi(this);
    connect(ui->comboData, &QComboBox::currentIndexChanged, this, &ClassificationLabelForm::loadImages);
    connect(ui->listImages, &QListWidget::currentRowChanged, this, &ClassificationLabelForm::showCurrentImage);
    connect(ui->buttonAssign,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (ui->comboClass->currentIndex() < 0)
                {
                    reportError(QString(u8"标注失败"), QString(u8"请先创建类别"));
                    return;
                }
                assignSelected(ui->comboClass->currentText());
            });
    connect(ui->buttonClear,
            &QPushButton::clicked,
            this,
            [this]()
            {
                assignSelected(QString());
            });
    connect(ui->buttonPrevious,
            &QPushButton::clicked,
            this,
            [this]()
            {
                ui->listImages->setCurrentRow(qMax(0, ui->listImages->currentRow() - 1));
            });
    connect(ui->buttonNext,
            &QPushButton::clicked,
            this,
            [this]()
            {
                ui->listImages->setCurrentRow(qMin(ui->listImages->count() - 1, ui->listImages->currentRow() + 1));
            });
}

ClassificationLabelForm::~ClassificationLabelForm()
{
    delete ui;
}

bool ClassificationLabelForm::readImageLabel(const QJsonObject &object, QString *label, QString *errorMessage)
{
    *label = object.value(QStringLiteral("image_label")).toString();
    const QJsonValue shapesValue = object.value(QStringLiteral("shapes"));
    if (!shapesValue.isArray())
    {
        *errorMessage = QString(u8"分类标注的区域数据无效");
        return false;
    }
    const QJsonArray shapes = shapesValue.toArray();
    if (shapes.isEmpty())
    {
        return true;
    }
    if (!label->isEmpty() || shapes.size() != 1 || !shapes.first().isObject())
    {
        *errorMessage = QString(u8"分类标注包含多个类别或区域");
        return false;
    }
    const QJsonObject shape = shapes.first().toObject();
    const QJsonArray points = shape.value(QStringLiteral("points")).toArray();
    const int width = object.value(QStringLiteral("imageWidth")).toInt();
    const int height = object.value(QStringLiteral("imageHeight")).toInt();
    if (shape.value(QStringLiteral("shape_type")).toString() != QStringLiteral("rectangle") ||
        shape.value(QStringLiteral("training_role")).toString() == QStringLiteral("ignore") || width < 2 ||
        height < 2 || points.size() != 4)
    {
        *errorMessage = QString(u8"分类标注必须是整图矩形");
        return false;
    }
    QSet<QPair<int, int>> corners;
    for (const QJsonValue &value : points)
    {
        const QJsonArray point = value.toArray();
        if (point.size() != 2 || !point.at(0).isDouble() || !point.at(1).isDouble())
        {
            *errorMessage = QString(u8"分类标注的矩形坐标无效");
            return false;
        }
        const double x = point.at(0).toDouble();
        const double y = point.at(1).toDouble();
        if ((x != 0 && x != width - 1) || (y != 0 && y != height - 1))
        {
            *errorMessage = QString(u8"分类标注必须覆盖整张图像");
            return false;
        }
        corners.insert(qMakePair(static_cast<int>(x), static_cast<int>(y)));
    }
    *label = shape.value(QStringLiteral("label")).toString();
    if (corners.size() != 4 || label->isEmpty())
    {
        *errorMessage = QString(u8"分类标注的矩形或类别无效");
        return false;
    }
    return true;
}

void ClassificationLabelForm::initialize(const QString &taskName)
{
    m_taskName = taskName;
    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(taskName, &task, &error))
    {
        reportError(QString(u8"加载失败"), error);
        return;
    }
    ui->comboClass->clear();
    ui->comboClass->addItems(task.labels);
    const QSignalBlocker blocker(ui->comboData);
    ui->comboData->clear();
    const QDir root(QDir(TaskRepository::LabelRoot()).filePath(taskName));
    ui->comboData->addItems(root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name));
    loadImages();
}

QString ClassificationLabelForm::currentDirectory() const
{
    return QDir(QDir(TaskRepository::LabelRoot()).filePath(m_taskName)).filePath(ui->comboData->currentText());
}

void ClassificationLabelForm::loadImages()
{
    ui->listImages->clear();
    if (ui->comboData->currentText().isEmpty())
    {
        showCurrentImage();
        return;
    }
    const QStringList filters{QStringLiteral("*.jpg"),
                              QStringLiteral("*.jpeg"),
                              QStringLiteral("*.png"),
                              QStringLiteral("*.bmp"),
                              QStringLiteral("*.tif"),
                              QStringLiteral("*.tiff")};
    ui->listImages->addItems(QDir(currentDirectory()).entryList(filters, QDir::Files, QDir::Name));
    if (ui->listImages->count() > 0)
    {
        ui->listImages->setCurrentRow(0);
    }
}

void ClassificationLabelForm::showCurrentImage()
{
    const QListWidgetItem *item = ui->listImages->currentItem();
    if (item == nullptr)
    {
        ui->labelPreview->clear();
        ui->labelCurrent->setText(QString(u8"当前类别: 未标注"));
        return;
    }
    const QString imagePath = QDir(currentDirectory()).filePath(item->text());
    const QImage image(imagePath);
    if (image.isNull())
    {
        reportError(QString(u8"读取失败"), QString(u8"无法读取图像: %1").arg(imagePath));
        return;
    }
    ui->labelPreview->setPixmap(
        QPixmap::fromImage(image).scaled(ui->labelPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QFile json(QDir(currentDirectory()).filePath(QFileInfo(item->text()).completeBaseName() + QStringLiteral(".json")));
    QString label;
    if (json.exists())
    {
        if (!json.open(QIODevice::ReadOnly))
        {
            reportError(QString(u8"读取失败"), QString(u8"无法读取 %1: %2").arg(json.fileName(), json.errorString()));
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(json.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            reportError(QString(u8"读取失败"), QString(u8"标注 JSON 无效: %1").arg(json.fileName()));
            return;
        }
        QString labelError;
        if (!readImageLabel(document.object(), &label, &labelError))
        {
            reportError(QString(u8"读取失败"), QString(u8"%1: %2").arg(json.fileName(), labelError));
            return;
        }
    }
    ui->labelCurrent->setText(QString(u8"当前类别: %1").arg(label.isEmpty() ? QString(u8"未标注") : label));
}

void ClassificationLabelForm::assignSelected(const QString &label)
{
    if (!label.isEmpty() && ui->comboClass->findText(label) < 0)
    {
        reportError(QString(u8"标注失败"), QString(u8"请选择有效类别"));
        return;
    }
    const QList<QListWidgetItem *> items = ui->listImages->selectedItems();
    if (items.isEmpty())
    {
        reportError(QString(u8"标注失败"), QString(u8"请选择图像"));
        return;
    }
    for (const QListWidgetItem *item : items)
    {
        const QString imagePath = QDir(currentDirectory()).filePath(item->text());
        const QSize imageSize = QImageReader(imagePath).size();
        if (!imageSize.isValid())
        {
            reportError(QString(u8"标注失败"), QString(u8"无法读取图像: %1").arg(imagePath));
            return;
        }
        const QString path =
            QDir(currentDirectory()).filePath(QFileInfo(item->text()).completeBaseName() + QStringLiteral(".json"));
        QJsonObject object;
        QFile previous(path);
        if (previous.exists())
        {
            if (!previous.open(QIODevice::ReadOnly))
            {
                reportError(QString(u8"标注失败"), QString(u8"无法读取 %1: %2").arg(path, previous.errorString()));
                return;
            }
            const QByteArray previousData = previous.readAll();
            previous.close();
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(previousData, &error);
            QString oldLabel;
            QString labelError;
            if (error.error != QJsonParseError::NoError || !document.isObject() ||
                !readImageLabel(document.object(), &oldLabel, &labelError))
            {
                reportError(QString(u8"标注失败"), QString(u8"分类标注文件无效或包含区域标注: %1").arg(path));
                return;
            }
            object = document.object();
            if (object.value(QStringLiteral("imagePath")).toString() != item->text())
            {
                reportError(QString(u8"标注失败"), QString(u8"存在同名图像标注冲突: %1").arg(path));
                return;
            }
        }
        object.insert(QStringLiteral("imagePath"), item->text());
        object.insert(QStringLiteral("imageWidth"), imageSize.width());
        object.insert(QStringLiteral("imageHeight"), imageSize.height());
        object.insert(QStringLiteral("shapes"), QJsonArray());
        object.insert(QStringLiteral("sample_role"), QStringLiteral("labeled"));
        if (label.isEmpty())
        {
            object.remove(QStringLiteral("image_label"));
        }
        else
        {
            object.insert(QStringLiteral("image_label"), label);
        }
        QSaveFile file(path);
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        {
            reportError(QString(u8"标注失败"), QString(u8"无法写入 %1: %2").arg(path, file.errorString()));
            return;
        }
    }
    QString error;
    if (!TaskRepository::UpdateProgress(m_taskName, QStringLiteral("annotated"), &error))
    {
        qWarning().noquote() << error;
        QMessageBox::warning(this, QString(u8"项目状态更新失败"), error);
    }
    showCurrentImage();
}

void ClassificationLabelForm::reportError(const QString &title, const QString &errorMessage)
{
    qWarning().noquote() << errorMessage;
    QMessageBox::critical(this, title, errorMessage);
}
