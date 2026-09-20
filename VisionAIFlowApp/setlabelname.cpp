#include "setlabelname.h"
#include "ui_setlabelname.h"

#include <QColorDialog>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>
#include <QVector>

namespace
{
struct JsonFileUpdate
{
    QString path;
    QJsonDocument document;
};

bool writeJsonDocument(const QString &path, const QJsonDocument &document, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        *errorMessage = QString(u8"无法打开JSON文件进行写入: %1, %2").arg(path, file.errorString());
        return false;
    }

    const QByteArray data = document.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size())
    {
        *errorMessage = QString(u8"写入JSON文件失败: %1, %2").arg(path, file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        *errorMessage = QString(u8"提交JSON文件失败: %1, %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool writeClassFile(const QString &path, const QJsonArray &nameList, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法打开标签类别文件进行写入: %1, %2").arg(path, file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const QJsonValue &value : nameList)
    {
        stream << value.toString() << "\n";
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok)
    {
        *errorMessage = QString(u8"写入标签类别文件失败: %1").arg(path);
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        *errorMessage = QString(u8"提交标签类别文件失败: %1, %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
} // namespace

SetLabelName::SetLabelName(QWidget *parent) : QDialog(parent), ui(new Ui::SetLabelName)
{
    ui->setupUi(this);
    m_SetName = QString(u8"标签1");
    m_SetColor = Qt::red;
    toSetData(m_SetName, m_SetColor);
}

SetLabelName::~SetLabelName()
{
    delete ui;
}

void SetLabelName::on_PB_Confirm_clicked()
{
    m_SetName = ui->lineEdit->text();
    accept();
}

void SetLabelName::on_PB_SetColor_clicked()
{
    QColorDialog tDlg(m_SetColor);
    if (QDialog::Accepted == tDlg.exec())
    {
        m_SetColor = tDlg.selectedColor();
        ui->PB_SetColor->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                           .arg(m_SetColor.red())
                                           .arg(m_SetColor.green())
                                           .arg(m_SetColor.blue()));
    }
}

void SetLabelName::toSetData(QString NameS, QColor SetColor)
{
    m_SetName = NameS;
    m_SetColor = SetColor;
    ui->PB_SetColor->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                                       .arg(m_SetColor.red())
                                       .arg(m_SetColor.green())
                                       .arg(m_SetColor.blue()));
    ui->lineEdit->setText(m_SetName);
    ui->lineEdit->setReadOnly(false);
    ui->label_ModifyName->setVisible(false);
    ui->LE_ModifyName->setVisible(false);
    ui->PB_Modify->setVisible(false);
    ui->PB_Confirm->setVisible(true);
    m_OriginalName.clear();
    m_ProjectPath.clear();
    m_LabelIndex = -1;
}

void SetLabelName::toSetModifyData(const QString &name, const QColor &color, int labelIndex, const QString &projectPath)
{
    toSetData(name, color);
    m_OriginalName = name;
    m_ProjectPath = QDir::cleanPath(projectPath);
    m_LabelIndex = labelIndex;
    ui->lineEdit->setReadOnly(true);
    ui->LE_ModifyName->setText(name);
    ui->label_ModifyName->setVisible(true);
    ui->LE_ModifyName->setVisible(true);
    ui->PB_Modify->setVisible(true);
    ui->PB_Confirm->setVisible(false);
}

void SetLabelName::toGetData(QString &NameS, QColor &SetColor)
{
    NameS = m_SetName;
    SetColor = m_SetColor;
}

void SetLabelName::on_PB_Modify_clicked()
{
    const QString newName = ui->LE_ModifyName->text();
    if (newName.isEmpty())
    {
        QMessageBox::warning(this, QString(u8"警告"), QString(u8"标签不能为空"), QString(u8"确定"));
        return;
    }

    QString errorMessage;
    if (!toModifyLabelName(newName, &errorMessage))
    {
        qCritical().noquote() << errorMessage;
        QMessageBox::critical(this, QString(u8"修改标签名失败"), errorMessage);
        return;
    }

    m_SetName = newName;
    accept();
}

void SetLabelName::on_PB_Cancle_clicked()
{
    reject();
}

bool SetLabelName::toModifyLabelName(const QString &newName, QString *errorMessage)
{
    if (newName.isEmpty())
    {
        *errorMessage = QString(u8"标签不能为空");
        return false;
    }

    const QDir projectDirectory(m_ProjectPath);
    if (m_ProjectPath.isEmpty() || !projectDirectory.exists())
    {
        *errorMessage = QString(u8"标注项目目录不存在: %1").arg(m_ProjectPath);
        return false;
    }

    const QString defineLabelPath = projectDirectory.filePath(QStringLiteral("DefineLabel.json"));
    const QString classPath = projectDirectory.filePath(QStringLiteral("Class.txt"));
    if (!QFileInfo::exists(defineLabelPath))
    {
        *errorMessage = QString(u8"未找到标签定义文件: %1").arg(defineLabelPath);
        return false;
    }
    if (!QFileInfo::exists(classPath))
    {
        *errorMessage = QString(u8"未找到标签类别文件: %1").arg(classPath);
        return false;
    }

    QFile defineLabelFile(defineLabelPath);
    if (!defineLabelFile.open(QIODevice::ReadOnly))
    {
        *errorMessage = QString(u8"无法读取标签定义文件: %1, %2").arg(defineLabelPath, defineLabelFile.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument defineLabelDocument = QJsonDocument::fromJson(defineLabelFile.readAll(), &parseError);
    defineLabelFile.close();
    if (parseError.error != QJsonParseError::NoError || !defineLabelDocument.isObject())
    {
        *errorMessage =
            QString(u8"标签定义文件不是有效JSON对象: %1, %2").arg(defineLabelPath, parseError.errorString());
        return false;
    }

    QJsonObject defineLabelObject = defineLabelDocument.object();
    QJsonArray nameList = defineLabelObject.value(QStringLiteral("NameList")).toArray();
    QJsonArray colorList = defineLabelObject.value(QStringLiteral("ClorDefine")).toArray();
    if (m_LabelIndex < 0 || m_LabelIndex >= nameList.size() || m_LabelIndex >= colorList.size())
    {
        *errorMessage = QString(u8"标签索引无效: %1").arg(m_LabelIndex);
        return false;
    }
    if (nameList.at(m_LabelIndex).toString() != m_OriginalName)
    {
        *errorMessage = QString(u8"标签定义已发生变化，索引%1当前名称为%2，预期为%3")
                            .arg(m_LabelIndex)
                            .arg(nameList.at(m_LabelIndex).toString(), m_OriginalName);
        return false;
    }

    nameList[m_LabelIndex] = newName;
    colorList[m_LabelIndex] = static_cast<int>(m_SetColor.rgb());
    defineLabelObject.insert(QStringLiteral("NameList"), nameList);
    defineLabelObject.insert(QStringLiteral("ClorDefine"), colorList);

    QVector<JsonFileUpdate> annotationUpdates;
    int annotationFileCount = 0;
    int replacementCount = 0;
    QDirIterator iterator(m_ProjectPath,
                          QStringList{QStringLiteral("*.json")},
                          QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString annotationPath = iterator.next();
        if (QDir::cleanPath(annotationPath).compare(QDir::cleanPath(defineLabelPath), Qt::CaseInsensitive) == 0)
        {
            continue;
        }
        ++annotationFileCount;

        QFile annotationFile(annotationPath);
        if (!annotationFile.open(QIODevice::ReadOnly))
        {
            *errorMessage = QString(u8"无法读取标注文件: %1, %2").arg(annotationPath, annotationFile.errorString());
            return false;
        }
        parseError = QJsonParseError();
        const QJsonDocument annotationDocument = QJsonDocument::fromJson(annotationFile.readAll(), &parseError);
        annotationFile.close();
        if (parseError.error != QJsonParseError::NoError || !annotationDocument.isObject())
        {
            *errorMessage = QString(u8"标注文件不是有效JSON对象: %1, %2").arg(annotationPath, parseError.errorString());
            return false;
        }

        QJsonObject annotationObject = annotationDocument.object();
        const QJsonValue shapesValue = annotationObject.value(QStringLiteral("shapes"));
        if (!shapesValue.isArray())
        {
            *errorMessage = QString(u8"标注文件缺少shapes数组: %1").arg(annotationPath);
            return false;
        }

        QJsonArray shapes = shapesValue.toArray();
        bool changed = false;
        for (int i = 0; i < shapes.size(); ++i)
        {
            if (!shapes.at(i).isObject())
            {
                *errorMessage = QString(u8"标注文件中的shape不是JSON对象: %1").arg(annotationPath);
                return false;
            }
            QJsonObject shape = shapes.at(i).toObject();
            if (newName != m_OriginalName && shape.value(QStringLiteral("label")).toString() == m_OriginalName)
            {
                shape.insert(QStringLiteral("label"), newName);
                shapes[i] = shape;
                changed = true;
                ++replacementCount;
            }
        }
        if (changed)
        {
            annotationObject.insert(QStringLiteral("shapes"), shapes);
            annotationUpdates.append({annotationPath, QJsonDocument(annotationObject)});
        }
    }

    for (const JsonFileUpdate &update : annotationUpdates)
    {
        if (!writeJsonDocument(update.path, update.document, errorMessage))
        {
            return false;
        }
    }
    if (!writeJsonDocument(defineLabelPath, QJsonDocument(defineLabelObject), errorMessage))
    {
        return false;
    }
    if (!writeClassFile(classPath, nameList, errorMessage))
    {
        return false;
    }

    qInfo().noquote() << QString(u8"标签名修改完成: %1 -> %2, 扫描%3个标注文件，替换%4处")
                             .arg(m_OriginalName, newName)
                             .arg(annotationFileCount)
                             .arg(replacementCount);
    return true;
}
