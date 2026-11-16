#include "lineannoform.h"

#include "BaseAnnoDisplayWidget.h"
#include "filecopysetdlg.h"
#include "taskrepository.h"
#include "release/ui_lineannoform.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSaveFile>
#include <QSignalBlocker>

LineAnnoForm::LineAnnoForm(QWidget *parent) : QWidget(parent), ui(new Ui::LineAnnoForm)
{
    ui->setupUi(this);
    ui->canvas->setDrawingShape(BaseAnnoShapeType::Line);
    ui->canvas->setDefaultAnnotation(QStringLiteral("line"), 0xff0000);
    ui->canvas->setShowAnnotationLabels(false);
    ui->canvas->setAnnotationDrawingEnabled(true);
    connect(ui->buttonDraw, &QPushButton::toggled, ui->canvas, &BaseAnnoDisplayWidget::setAnnotationDrawingEnabled);
    connect(ui->buttonImport, &QPushButton::clicked, this, &LineAnnoForm::importImages);
    connect(ui->comboDirectory, &QComboBox::currentIndexChanged, this, &LineAnnoForm::loadImages);
    connect(ui->includedDirectories,
            &QListWidget::itemChanged,
            this,
            [this]()
            {
                TaskDefinition task;
                QString error;
                if (!TaskRepository::LoadTask(m_taskName, &task, &error))
                {
                    reportError(error);
                    return;
                }
                task.annotationDirectories.clear();
                for (int index = 0; index < ui->includedDirectories->count(); ++index)
                {
                    const QListWidgetItem *item = ui->includedDirectories->item(index);
                    if (item->checkState() != Qt::Checked)
                    {
                        task.annotationDirectories.append(item->text());
                    }
                }
                if (!TaskRepository::SaveTask(task, &error))
                {
                    reportError(error);
                }
            });
    connect(ui->listImages, &QListWidget::currentRowChanged, this, &LineAnnoForm::showImage);
    connect(ui->buttonSave, &QPushButton::clicked, this, &LineAnnoForm::save);
    connect(ui->canvas,
            &BaseAnnoDisplayWidget::annotationSelectionChanged,
            this,
            [this](int index)
            {
                m_selected = index;
            });
    connect(ui->buttonDelete,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (m_selected >= 0)
                {
                    QString error;
                    if (!ui->canvas->removeAnnotation(m_selected, &error))
                    {
                        reportError(error);
                    }
                    m_selected = -1;
                }
            });
}

LineAnnoForm::~LineAnnoForm()
{
    delete ui;
}

void LineAnnoForm::initialize(const QString &taskName)
{
    m_taskName = taskName;
    TaskDefinition task;
    QString error;
    if (!TaskRepository::LoadTask(taskName, &task, &error))
    {
        reportError(error);
        return;
    }
    const QSignalBlocker blocker(ui->comboDirectory);
    const QSignalBlocker listBlocker(ui->includedDirectories);
    ui->comboDirectory->clear();
    ui->includedDirectories->clear();
    const QStringList directories = QDir(QDir(TaskRepository::LabelRoot()).filePath(taskName))
                                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    ui->comboDirectory->addItems(directories);
    for (const QString &directory : directories)
    {
        auto *item = new QListWidgetItem(directory, ui->includedDirectories);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(task.annotationDirectories.contains(directory) ? Qt::Unchecked : Qt::Checked);
    }
    loadImages();
}

QString LineAnnoForm::directoryPath() const
{
    return QDir(QDir(TaskRepository::LabelRoot()).filePath(m_taskName)).filePath(ui->comboDirectory->currentText());
}

QString LineAnnoForm::imagePath() const
{
    const QListWidgetItem *item = ui->listImages->currentItem();
    return item == nullptr ? QString() : QDir(directoryPath()).filePath(item->text());
}

void LineAnnoForm::importImages()
{
    FileCopySetDlg dialog(this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    const QString name = dialog.toGetTitleName();
    if (name.isEmpty() || QFileInfo(name).fileName() != name || name == QStringLiteral(".") ||
        name == QStringLiteral(".."))
    {
        reportError(QString(u8"请输入有效的标注目录名称"));
        return;
    }
    const QString path = QDir(QDir(TaskRepository::LabelRoot()).filePath(m_taskName)).filePath(name);
    const QStringList files = dialog.toGetFileList();
    if (files.isEmpty() || !QDir().mkpath(path))
    {
        reportError(QString(u8"导入图片失败: %1").arg(path));
        return;
    }
    for (const QString &source : files)
    {
        const QString destination = QDir(path).filePath(QFileInfo(source).fileName());
        if (!QFile::copy(source, destination))
        {
            reportError(QString(u8"复制图片失败: %1 -> %2").arg(source, destination));
            return;
        }
    }
    initialize(m_taskName);
    ui->comboDirectory->setCurrentText(name);
}

void LineAnnoForm::loadImages()
{
    ui->listImages->clear();
    if (!ui->comboDirectory->currentText().isEmpty())
    {
        ui->listImages->addItems(QDir(directoryPath())
                                     .entryList({QStringLiteral("*.jpg"),
                                                 QStringLiteral("*.jpeg"),
                                                 QStringLiteral("*.png"),
                                                 QStringLiteral("*.bmp"),
                                                 QStringLiteral("*.tif"),
                                                 QStringLiteral("*.tiff")},
                                                QDir::Files,
                                                QDir::Name));
    }
    if (ui->listImages->count() > 0)
    {
        ui->listImages->setCurrentRow(0);
    }
    else
    {
        ui->canvas->clearImage();
        ui->canvas->clearAnnotations();
    }
}

void LineAnnoForm::showImage()
{
    m_selected = -1;
    m_loadedValid = false;
    ui->canvas->clearAnnotations();
    const QString image = imagePath();
    if (image.isEmpty())
    {
        ui->canvas->clearImage();
        return;
    }
    QString error;
    if (!ui->canvas->loadImage(image, &error))
    {
        reportError(error);
        return;
    }
    QFile file(QDir(directoryPath()).filePath(QFileInfo(image).completeBaseName() + QStringLiteral(".json")));
    if (!file.exists())
    {
        m_loadedValid = true;
        return;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        reportError(QString(u8"无法读取标注: %1").arg(file.fileName()));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject root = document.object();
    const QSize size = QImageReader(image).size();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || !root.value("shapes").isArray() ||
        root.value("imagePath").toString() != QFileInfo(image).fileName() ||
        root.value("imageWidth").toInt() != size.width() || root.value("imageHeight").toInt() != size.height())
    {
        reportError(QString(u8"标注无效或图像尺寸不匹配: %1").arg(file.fileName()));
        return;
    }
    BaseAnnoAnnotationList annotations;
    for (const QJsonValue &value : root.value("shapes").toArray())
    {
        const QJsonObject shape = value.toObject();
        const QJsonArray points = shape.value("points").toArray();
        if (shape.value("shape_type").toString() != QStringLiteral("line") || points.size() != 2 ||
            !points[0].isArray() || !points[1].isArray() || points[0].toArray().size() != 2 ||
            points[1].toArray().size() != 2)
        {
            reportError(QString(u8"线段标注无效: %1").arg(file.fileName()));
            return;
        }
        BaseAnnoAnnotation annotation;
        annotation.label = QStringLiteral("line");
        annotation.colorValue = 0xff0000;
        annotation.shapeType = BaseAnnoShapeType::Line;
        for (const QJsonValue &point : points)
        {
            const QJsonArray xy = point.toArray();
            if (!xy[0].isDouble() || !xy[1].isDouble() || xy[0].toDouble() < 0 || xy[1].toDouble() < 0 ||
                xy[0].toDouble() >= size.width() || xy[1].toDouble() >= size.height())
            {
                reportError(QString(u8"线段坐标无效: %1").arg(file.fileName()));
                return;
            }
            annotation.pointsImage << QPointF(xy[0].toDouble(), xy[1].toDouble());
        }
        if (annotation.pointsImage[0] == annotation.pointsImage[1])
        {
            reportError(QString(u8"线段端点重合: %1").arg(file.fileName()));
            return;
        }
        annotations.append(annotation);
    }
    ui->canvas->setAnnotations(annotations);
    m_loadedValid = true;
}

void LineAnnoForm::save()
{
    const QString image = imagePath();
    if (image.isEmpty())
    {
        reportError(QString(u8"请选择图片"));
        return;
    }
    if (!m_loadedValid)
    {
        reportError(QString(u8"图片或标注加载失败, 无法保存: %1").arg(image));
        return;
    }
    const QSize size = QImageReader(image).size();
    QJsonArray shapes;
    for (const BaseAnnoAnnotation &annotation : ui->canvas->annotations())
    {
        QJsonArray points;
        for (const QPointF &point : annotation.pointsImage)
        {
            points.append(QJsonArray{point.x(), point.y()});
        }
        shapes.append(QJsonObject{{"label", "line"}, {"shape_type", "line"}, {"points", points}});
    }
    const QJsonObject root{{"imagePath", QFileInfo(image).fileName()},
                           {"imageWidth", size.width()},
                           {"imageHeight", size.height()},
                           {"shapes", shapes}};
    QSaveFile file(QDir(directoryPath()).filePath(QFileInfo(image).completeBaseName() + QStringLiteral(".json")));
    const QByteArray bytes = QJsonDocument(root).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        reportError(QString(u8"保存标注失败: %1: %2").arg(file.fileName(), file.errorString()));
        return;
    }
    QString error;
    if (!TaskRepository::UpdateProgress(m_taskName, QStringLiteral("annotated"), &error))
    {
        reportError(error);
    }
}

void LineAnnoForm::reportError(const QString &errorMessage)
{
    qWarning().noquote() << errorMessage;
    QMessageBox::critical(this, QString(u8"线段标注"), errorMessage);
}
