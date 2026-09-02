#include "taskrepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace
{
QString g_workPath;

QString SettingsPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("YtYoloDefine.json"));
}

bool WriteJsonFile(const QString &path, const QJsonObject &object, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        *errorMessage = QString(u8"无法写入文件 %1: %2").arg(path, file.errorString());
        return false;
    }

    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit())
    {
        *errorMessage = QString(u8"无法提交文件 %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool ReadJsonFile(const QString &path, QJsonObject *object, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *errorMessage = QString(u8"无法读取文件 %1: %2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        *errorMessage = QString(u8"任务文件不是有效 JSON: %1").arg(path);
        return false;
    }
    *object = document.object();
    return true;
}
} // namespace

bool TaskRepository::Initialize(QString *errorMessage)
{
    if (!g_workPath.isEmpty())
    {
        return true;
    }

    QJsonObject settings;
    QString ignoredError;
    if (QFileInfo::exists(SettingsPath()) && ReadJsonFile(SettingsPath(), &settings, &ignoredError))
    {
        g_workPath = settings.value(QStringLiteral("WorkPath")).toString();
    }
    if (g_workPath.isEmpty())
    {
        g_workPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                         .filePath(QStringLiteral("VisionAIFlowWorkspace"));
    }

    const QStringList roots{LabelRoot(), DataRoot(), TrainRoot(), ValidationRoot()};
    for (const QString &root : roots)
    {
        if (!QDir().mkpath(root))
        {
            *errorMessage = QString(u8"无法创建工作目录: %1").arg(root);
            return false;
        }
    }

    settings.insert(QStringLiteral("WorkPath"), g_workPath);
    return WriteJsonFile(SettingsPath(), settings, errorMessage);
}

QString TaskRepository::WorkPath()
{
    return g_workPath;
}

QString TaskRepository::LabelRoot()
{
    return QDir(g_workPath).filePath(QStringLiteral("LabelSheet"));
}

QString TaskRepository::DataRoot()
{
    return QDir(g_workPath).filePath(QStringLiteral("DataSheet"));
}

QString TaskRepository::TrainRoot()
{
    return QDir(g_workPath).filePath(QStringLiteral("TrainSheet"));
}

QString TaskRepository::ValidationRoot()
{
    return QDir(g_workPath).filePath(QStringLiteral("ValSheet"));
}

bool TaskRepository::ListTasks(QVector<TaskDefinition> *tasks, QString *errorMessage)
{
    tasks->clear();
    const QFileInfoList entries = QDir(LabelRoot()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QFileInfo &entry : entries)
    {
        TaskDefinition task;
        if (!LoadTask(entry.fileName(), &task, errorMessage))
        {
            return false;
        }
        tasks->append(task);
    }
    return true;
}

bool TaskRepository::LoadTask(const QString &taskName, TaskDefinition *task, QString *errorMessage)
{
    QJsonObject object;
    const QString path = DefineLabelPath(taskName);
    const bool hasTaskDefinition = QFileInfo::exists(path);
    if (hasTaskDefinition && !ReadJsonFile(path, &object, errorMessage))
    {
        return false;
    }

    const auto type = visionaiflow::domain::ProjectTypeFromString(object.value(QStringLiteral("TaskType")).toString());
    if (!type.IsSuccess() && object.contains(QStringLiteral("TaskType")))
    {
        *errorMessage = QString(u8"任务 %1 的 TaskType 非法").arg(taskName);
        return false;
    }

    task->name = taskName;
    task->description = object.value(QStringLiteral("InfoSet")).toString();
    task->type = type.IsSuccess() ? type.Value() : visionaiflow::domain::ProjectType::Detection;
    task->progress = object.value(QStringLiteral("TaskProgress")).toString();
    if (task->progress.isEmpty())
    {
        task->progress = QStringLiteral("created");
    }

    const QJsonArray labels = object.value(QStringLiteral("NameList")).toArray();
    const QJsonArray colors = object.value(QStringLiteral("ClorDefine")).toArray();
    const QJsonArray annotationDirectories = object.value(QStringLiteral("UncheckedDataSheetList")).toArray();
    for (qsizetype index = 0; index < labels.size(); ++index)
    {
        task->labels.append(labels.at(index).toString());
        task->colors.append(index < colors.size() ? QColor::fromRgba(static_cast<QRgb>(colors.at(index).toInt()))
                                                  : DefaultColor(index));
    }
    for (const QJsonValue &value : annotationDirectories)
    {
        const QString directory = value.toString();
        if (!directory.isEmpty())
        {
            task->annotationDirectories.append(directory);
        }
    }

    bool migratedLegacyLabels = false;
    if (task->labels.isEmpty())
    {
        const QDir taskDirectory(QDir(LabelRoot()).filePath(taskName));
        const QFileInfoList dataSheetDirectories =
            taskDirectory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &dataSheetDirectory : dataSheetDirectories)
        {
            const QString legacyConfigPath =
                QDir(dataSheetDirectory.absoluteFilePath()).filePath(QStringLiteral("DefineLabel.json"));
            if (!QFileInfo::exists(legacyConfigPath))
            {
                continue;
            }

            QJsonObject legacyObject;
            if (!ReadJsonFile(legacyConfigPath, &legacyObject, errorMessage))
            {
                return false;
            }

            const QJsonArray legacyLabels = legacyObject.value(QStringLiteral("NameList")).toArray();
            if (legacyLabels.isEmpty())
            {
                continue;
            }

            const QJsonArray legacyColors = legacyObject.value(QStringLiteral("ClorDefine")).toArray();
            for (qsizetype index = 0; index < legacyLabels.size(); ++index)
            {
                task->labels.append(legacyLabels.at(index).toString());
                task->colors.append(index < legacyColors.size() ? QColor::fromRgb(legacyColors.at(index).toInt())
                                                                : DefaultColor(index));
            }
            migratedLegacyLabels = true;
            break;
        }
    }

    if (!hasTaskDefinition || migratedLegacyLabels || !object.contains(QStringLiteral("TaskType")) ||
        !object.contains(QStringLiteral("TaskProgress")))
    {
        return SaveTask(*task, errorMessage);
    }
    return true;
}

bool TaskRepository::CreateTask(const TaskDefinition &task, QString *errorMessage)
{
    if (!ValidateTaskName(task.name, errorMessage))
    {
        return false;
    }
    if (QFileInfo::exists(QDir(LabelRoot()).filePath(task.name)))
    {
        *errorMessage = QString(u8"任务已存在: %1").arg(task.name);
        return false;
    }
    if (!EnsureTaskDirectories(task.name, errorMessage))
    {
        return false;
    }
    return SaveTask(task, errorMessage);
}

bool TaskRepository::RenameTask(const QString &oldName, const QString &newName, QString *errorMessage)
{
    if (!ValidateTaskName(newName, errorMessage))
    {
        return false;
    }
    if (oldName == newName)
    {
        return true;
    }
    if (QFileInfo::exists(QDir(LabelRoot()).filePath(newName)))
    {
        *errorMessage = QString(u8"任务已存在: %1").arg(newName);
        return false;
    }

    const QStringList roots{LabelRoot(), DataRoot(), TrainRoot(), ValidationRoot()};
    for (const QString &root : roots)
    {
        const QString oldPath = QDir(root).filePath(oldName);
        if (QFileInfo::exists(oldPath) && !QDir(root).rename(oldName, newName))
        {
            *errorMessage = QString(u8"无法重命名目录: %1").arg(oldPath);
            return false;
        }
    }
    return true;
}

bool TaskRepository::DeleteTask(const QString &taskName, QString *errorMessage)
{
    if (!ValidateTaskName(taskName, errorMessage))
    {
        return false;
    }

    const QStringList roots{DataRoot(), TrainRoot(), ValidationRoot(), LabelRoot()};
    for (const QString &root : roots)
    {
        const QString taskPath = QDir(root).filePath(taskName);
        if (!QFileInfo::exists(taskPath))
        {
            continue;
        }
        if (!QDir(taskPath).removeRecursively())
        {
            *errorMessage = QString(u8"无法删除项目目录: %1").arg(taskPath);
            return false;
        }
    }
    return true;
}

bool TaskRepository::SaveTask(const TaskDefinition &task, QString *errorMessage)
{
    if (!ValidateTaskName(task.name, errorMessage) || !EnsureTaskDirectories(task.name, errorMessage))
    {
        return false;
    }

    QJsonObject object;
    const QString path = DefineLabelPath(task.name);
    if (QFileInfo::exists(path) && !ReadJsonFile(path, &object, errorMessage))
    {
        return false;
    }

    object.insert(QStringLiteral("InfoSet"), task.description);
    object.insert(QStringLiteral("TaskType"), visionaiflow::domain::ToString(task.type));
    object.insert(QStringLiteral("TaskProgress"), task.progress);
    QJsonArray labels;
    QJsonArray colors;
    QJsonArray annotationDirectories;
    for (qsizetype index = 0; index < task.labels.size(); ++index)
    {
        labels.append(task.labels.at(index));
        const QColor color = index < task.colors.size() ? task.colors.at(index) : DefaultColor(index);
        colors.append(static_cast<int>(color.rgba()));
    }
    for (const QString &directory : task.annotationDirectories)
    {
        annotationDirectories.append(directory);
    }
    object.insert(QStringLiteral("NameList"), labels);
    object.insert(QStringLiteral("ClorDefine"), colors);
    object.insert(QStringLiteral("UncheckedDataSheetList"), annotationDirectories);
    return WriteJsonFile(path, object, errorMessage);
}

bool TaskRepository::UpdateProgress(const QString &taskName, const QString &progress, QString *errorMessage)
{
    TaskDefinition task;
    if (!LoadTask(taskName, &task, errorMessage))
    {
        return false;
    }
    task.progress = progress;
    return SaveTask(task, errorMessage);
}

QString TaskRepository::DisplayType(const visionaiflow::domain::ProjectType type)
{
    switch (type)
    {
    case visionaiflow::domain::ProjectType::Detection:
        return QString(u8"目标检测");
    case visionaiflow::domain::ProjectType::Classification:
        return QString(u8"图像分类");
    case visionaiflow::domain::ProjectType::InstanceSegmentation:
        return QString(u8"实例分割");
    case visionaiflow::domain::ProjectType::SemanticSegmentation:
        return QString(u8"语义分割");
    case visionaiflow::domain::ProjectType::AnomalyDetection:
        return QString(u8"异常检测");
    case visionaiflow::domain::ProjectType::LineDetection:
        return QString(u8"线段检测");
    case visionaiflow::domain::ProjectType::OcrDetection:
        return QString(u8"OCR检测");
    case visionaiflow::domain::ProjectType::OcrRecognition:
        return QString(u8"OCR识别");
    case visionaiflow::domain::ProjectType::OcrPipeline:
        return QString(u8"OCR流程");
    }
    return QString();
}

QString TaskRepository::DisplayProgress(const QString &progress)
{
    const QHash<QString, QString> values{{QStringLiteral("created"), QString(u8"已创建")},
                                         {QStringLiteral("annotated"), QString(u8"已标注")},
                                         {QStringLiteral("dataset_ready"), QString(u8"数据集就绪")},
                                         {QStringLiteral("training"), QString(u8"训练中")},
                                         {QStringLiteral("trained"), QString(u8"已训练")},
                                         {QStringLiteral("validated"), QString(u8"已验证")},
                                         {QStringLiteral("data_failed"), QString(u8"数据失败")},
                                         {QStringLiteral("training_failed"), QString(u8"训练失败")},
                                         {QStringLiteral("validation_failed"), QString(u8"验证失败")}};
    return values.value(progress, QString(u8"未知状态"));
}

bool TaskRepository::IsDetection(const TaskDefinition &task)
{
    return task.type == visionaiflow::domain::ProjectType::Detection;
}

bool TaskRepository::EnsureTaskDirectories(const QString &taskName, QString *errorMessage)
{
    const QString labelPath = QDir(LabelRoot()).filePath(taskName);
    if (!QDir().mkpath(labelPath))
    {
        *errorMessage = QString(u8"无法创建任务目录: %1").arg(labelPath);
        return false;
    }
    return true;
}

bool TaskRepository::ValidateTaskName(const QString &taskName, QString *errorMessage)
{
    if (taskName.trimmed().isEmpty() || taskName.contains(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]"))))
    {
        *errorMessage = QString(u8"任务名称不能为空且不能包含文件系统保留字符");
        return false;
    }
    return true;
}

QString TaskRepository::DefineLabelPath(const QString &taskName)
{
    return QDir(LabelRoot()).filePath(taskName + QStringLiteral("/DefineLabel.json"));
}

QColor TaskRepository::DefaultColor(const int index)
{
    static const QVector<QColor> colors{Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::magenta, Qt::cyan};
    return colors.at(index % colors.size());
}
