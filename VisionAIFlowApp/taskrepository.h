#pragma once

#include "visionaiflow/domain/ProjectType.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

struct TaskDefinition
{
    QString name;
    QString description;
    visionaiflow::domain::ProjectType type{visionaiflow::domain::ProjectType::Detection};
    QString progress{QStringLiteral("created")};
    QStringList labels;
    QVector<QColor> colors;
    QStringList annotationDirectories;
};

class TaskRepository final
{
public:
    static bool Initialize(QString *errorMessage);
    static QString WorkPath();
    static QString LabelRoot();
    static QString DataRoot();
    static QString TrainRoot();
    static QString ValidationRoot();

    static bool ListTasks(QVector<TaskDefinition> *tasks, QString *errorMessage);
    static bool LoadTask(const QString &taskName, TaskDefinition *task, QString *errorMessage);
    static bool CreateTask(const TaskDefinition &task, QString *errorMessage);
    static bool RenameTask(const QString &oldName, const QString &newName, QString *errorMessage);
    static bool DeleteTask(const QString &taskName, QString *errorMessage);
    static bool SaveTask(const TaskDefinition &task, QString *errorMessage);
    static bool UpdateProgress(const QString &taskName, const QString &progress, QString *errorMessage);

    static QString DisplayType(visionaiflow::domain::ProjectType type);
    static QString DisplayProgress(const QString &progress);
    static bool IsDetection(const TaskDefinition &task);

private:
    static bool EnsureTaskDirectories(const QString &taskName, QString *errorMessage);
    static bool ValidateTaskName(const QString &taskName, QString *errorMessage);
    static QString DefineLabelPath(const QString &taskName);
    static QColor DefaultColor(int index);
};
