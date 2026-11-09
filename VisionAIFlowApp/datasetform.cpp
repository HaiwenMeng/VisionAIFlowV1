#include "mlsdprocesser.h"
#include "datasetform.h"
#include "taskrepository.h"
#include "AnnoForms/ClassForms/classificationlabelform.h"
#include "release/ui_datasetform.h"
#include <QTreeWidgetItem>
#include <QStyleFactory>
#include <QFileIconProvider>
#include <algorithm>
#include <cmath>
#include <random>
#include <QtConcurrent/QtConcurrent>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QStringConverter>
#include <QThreadPool>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <opencv2/imgproc.hpp>

namespace
{
QString yamlQuoted(const QString &value)
{
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

bool saveYoloeImageAndMask(const QString &imagePath,
                           const QString &imageOutput,
                           const QString &maskOutput,
                           const QString &jsonPath,
                           const YtRoiLabelSet &labels,
                           int backgroundFill,
                           QString *errorMessage)
{
    QImage image(imagePath);
    if (image.isNull() || image.width() != labels.m_imWidth || image.height() != labels.m_imHight)
    {
        *errorMessage = QString(u8"YOLOE图像读取失败或标注尺寸不匹配: %1").arg(imagePath);
        return false;
    }
    try
    {
        cv::Mat mask(image.height(), image.width(), CV_8UC1, cv::Scalar(0));
        for (int operation = 0; operation < 2; ++operation)
        {
            for (LabelSet label : labels.m_SetLabeset)
            {
                if (!label.isTrainingIgnored() || label.isIgnoreSubtract() != (operation == 1))
                    continue;
                std::vector<cv::Point> polygon;
                for (const QPointF &point : label.toGetPoints())
                {
                    if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || point.x() < 0 || point.y() < 0 ||
                        point.x() > image.width() || point.y() > image.height())
                    {
                        *errorMessage = QString(u8"忽略区坐标无效: %1").arg(jsonPath);
                        return false;
                    }
                    polygon.emplace_back(qRound(point.x()), qRound(point.y()));
                }
                if (polygon.size() < 3 || std::abs(cv::contourArea(polygon)) <= 0.0)
                {
                    *errorMessage = QString(u8"无效的忽略区多边形: %1").arg(jsonPath);
                    return false;
                }
                cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{polygon}, cv::Scalar(operation == 0 ? 255 : 0));
            }
        }
        const bool highDepth = image.depth() > 32 || image.format() == QImage::Format_Grayscale16;
        if (highDepth)
        {
            image = image.convertToFormat(QImage::Format_RGBA64);
            const quint16 value = static_cast<quint16>(backgroundFill * 257);
            for (int y = 0; y < image.height(); ++y)
            {
                QRgba64 *row = reinterpret_cast<QRgba64 *>(image.scanLine(y));
                const uchar *maskRow = mask.ptr<uchar>(y);
                for (int x = 0; x < image.width(); ++x)
                    if (maskRow[x])
                        row[x] = QRgba64::fromRgba64(value, value, value, 65535);
            }
        }
        else
        {
            image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
            for (int y = 0; y < image.height(); ++y)
            {
                QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
                const uchar *maskRow = mask.ptr<uchar>(y);
                for (int x = 0; x < image.width(); ++x)
                    if (maskRow[x])
                        row[x] = qRgb(backgroundFill, backgroundFill, backgroundFill);
            }
        }
        const QImage maskImage(mask.data, mask.cols, mask.rows, static_cast<int>(mask.step), QImage::Format_Grayscale8);
        if (!image.save(imageOutput, "PNG") || !maskImage.save(maskOutput, "PNG"))
        {
            *errorMessage = QString(u8"保存YOLOE图像或忽略区失败: %1").arg(imagePath);
            return false;
        }
    }
    catch (const cv::Exception &error)
    {
        *errorMessage = QString(u8"生成YOLOE忽略区失败: %1, %2").arg(jsonPath, QString::fromUtf8(error.what()));
        return false;
    }
    return true;
}
bool saveYoloeLabels(const QString &path,
                     const YtRoiLabelSet &labels,
                     const QVector<QString> &names,
                     QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法写入YOLOE标签: %1, %2").arg(path, file.errorString());
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (LabelSet label : labels.m_SetLabeset)
    {
        if (label.isTrainingIgnored() || label.Name.compare(QStringLiteral("background"), Qt::CaseInsensitive) == 0)
            continue;
        const int classId = names.indexOf(label.Name);
        const QVector<QPointF> points = label.toGetPoints();
        if (classId < 0 || label.toGetRoiType() != LabelSet::LpolygonROI || points.size() < 3 ||
            labels.m_imWidth <= 0 || labels.m_imHight <= 0)
        {
            *errorMessage = QString(u8"YOLOE目标类别或多边形无效: %1").arg(path);
            return false;
        }
        double area = 0.0;
        out << classId;
        for (int i = 0; i < points.size(); ++i)
        {
            const QPointF &point = points.at(i);
            const QPointF &next = points.at((i + 1) % points.size());
            if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || point.x() < 0 || point.y() < 0 ||
                point.x() > labels.m_imWidth || point.y() > labels.m_imHight)
            {
                *errorMessage = QString(u8"YOLOE目标坐标无效: %1").arg(path);
                return false;
            }
            area += point.x() * next.y() - next.x() * point.y();
            out << ' ' << QString::number(point.x() / labels.m_imWidth, 'f', 6) << ' '
                << QString::number(point.y() / labels.m_imHight, 'f', 6);
        }
        if (!std::isfinite(area) || std::abs(area) <= 0.0)
        {
            *errorMessage = QString(u8"YOLOE目标多边形面积无效: %1").arg(path);
            return false;
        }
        out << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit())
    {
        *errorMessage = QString(u8"保存YOLOE标签失败: %1, %2").arg(path, file.errorString());
        return false;
    }
    return true;
}
} // namespace

DataSetForm::DataSetForm(QWidget *parent) : QWidget(parent), ui(new Ui::DataSetForm)
{
    ui->setupUi(this);
    ui->labelBackgroundFill->setVisible(false);
    ui->comboBackgroundFill->setVisible(false);
    ui->TW_LabeSet->verticalHeader()->setVisible(false); // 隐藏垂直表头
    ui->TW_LabeSet->horizontalHeader()->setStretchLastSection(true);
    ui->TW_LabeSet->setEditTriggers(QAbstractItemView::NoEditTriggers); // 不可编辑
    ui->TW_LabeSet->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->TW_LabeSet->setSelectionBehavior(QAbstractItemView::SelectRows); // 选择一行
    ui->TW_LabeSet->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->ytRoiShowDisp->toSetBGColor(QColor(33, 34, 35));
    ui->ToolTreeWidget->setIconSize(QSize(22, 22));
    ui->ToolTreeWidget->setStyle(QStyleFactory::create("windows"));
    ui->ToolTreeWidget->header()->setVisible(false);
    ui->ToolTreeWidget->clear();

    ui->ToolTreeWidget->header()->setVisible(false);
    ui->ToolTreeWidget->clear();
    connect(ui->ToolTreeWidget, &QTreeWidget::itemClicked, this, &DataSetForm::slot_ItemDoubleClicked);
    //
    ui->ytRoiShowDisp->addOverPlayPtr(&m_OverPlayShow, "set");
    connect(this, &DataSetForm::Sigfinish, this, &DataSetForm::slotFinish);
    connect(this, &DataSetForm::SigProcess, &m_ShowProcessForm, &ShowProcessForm::toSetProcess);
}

DataSetForm::~DataSetForm()
{
    delete ui;
}

void DataSetForm::toSetProcessName(QString Setname)
{
    m_ProcessName = Setname;
}

void DataSetForm::setYoloEMode(bool enabled)
{
    m_yoloEMode = enabled;
    ui->labelBackgroundFill->setVisible(enabled);
    ui->comboBackgroundFill->setVisible(enabled);
    if (enabled)
    {
        ui->comboBox->setCurrentIndex(2);
    }
    ui->comboBox->setEnabled(!enabled);
}

void DataSetForm::setProjectType(visionaiflow::domain::ProjectType type)
{
    using visionaiflow::domain::ProjectType;
    setYoloEMode(type == ProjectType::YoloE);
    switch (type)
    {
    case ProjectType::Detection:
        m_forcedRunIndex = 0;
        break;
    case ProjectType::Classification:
        m_forcedRunIndex = 3;
        break;
    case ProjectType::InstanceSegmentation:
    case ProjectType::YoloE:
        m_forcedRunIndex = 2;
        break;
    default:
        m_forcedRunIndex = -1;
        break;
    }
    if (m_forcedRunIndex >= 0)
    {
        m_DataGenRunIndex = m_forcedRunIndex;
        ui->comboBox->setCurrentIndex(m_forcedRunIndex);
        ui->comboBox->setEnabled(false);
    }
}

QVector<QString> DataSetForm::trainingLabelNames() const
{
    QVector<QString> names;
    for (const QString &name : m_YtYoloSetPro.m_NameList)
    {
        if ((!m_yoloEMode && m_DataGenRunIndex != 0 && m_DataGenRunIndex != 2) ||
            name.compare(QStringLiteral("background"), Qt::CaseInsensitive) != 0)
        {
            names.append(name);
        }
    }
    return names;
}

void DataSetForm::toInitShow()
{
    toLoadData();
    if (m_forcedRunIndex >= 0)
    {
        ui->comboBox->setCurrentIndex(m_forcedRunIndex);
    }
    else if (m_yoloEMode)
    {
        ui->comboBox->setCurrentIndex(2);
    }
    toShowData();
}

QFileInfoList DataSetForm::toGetCheckedDataSheetDirList()
{
    QFileInfoList checkedDirList;
    QFileInfoList allDirList = YtYoloDefine::toGetPathDirInfo(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    YtYoloSetPro setPro;
    setPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    for (int i = 0; i < allDirList.size(); i++)
    {
        if (!setPro.m_UncheckedDataSheetList.contains(allDirList.at(i).fileName()))
        {
            checkedDirList.append(allDirList.at(i));
        }
    }
    return checkedDirList;
}

bool DataSetForm::generateClassificationData(const QFileInfoList &directories)
{
    const QVector<QString> names = trainingLabelNames();
    if (names.isEmpty())
    {
        m_DataGenErrorMessage = QString(u8"分类项目没有类别");
        return false;
    }
    QSet<QString> normalizedNames;
    QVector<QStringList> samples(names.size());
    QSet<QString> imageNames;
    for (const QString &name : names)
    {
        if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") ||
            QFileInfo(name).fileName() != name ||
            name.contains(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]"))) || name.endsWith(QLatin1Char('.')) ||
            name.endsWith(QLatin1Char(' ')) || normalizedNames.contains(name.toCaseFolded()))
        {
            m_DataGenErrorMessage = QString(u8"分类类别不能作为唯一目录名称: %1").arg(name);
            return false;
        }
        normalizedNames.insert(name.toCaseFolded());
    }
    for (const QFileInfo &directory : directories)
    {
        const QFileInfoList jsonFiles =
            QDir(directory.absoluteFilePath())
                .entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QFileInfo &jsonInfo : jsonFiles)
        {
            QFile file(jsonInfo.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly))
            {
                m_DataGenErrorMessage = QString(u8"无法读取分类标注: %1").arg(jsonInfo.absoluteFilePath());
                return false;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                m_DataGenErrorMessage = QString(u8"分类标注 JSON 无效: %1").arg(jsonInfo.absoluteFilePath());
                return false;
            }
            const QJsonObject object = document.object();
            QString label;
            QString labelError;
            if (!ClassificationLabelForm::readImageLabel(object, &label, &labelError))
            {
                m_DataGenErrorMessage = QString(u8"分类标注无效: %1: %2").arg(jsonInfo.absoluteFilePath(), labelError);
                return false;
            }
            if (label.isEmpty())
            {
                continue;
            }
            const int classIndex = names.indexOf(label);
            if (classIndex < 0)
            {
                m_DataGenErrorMessage = QString(u8"分类标注包含未定义类别或区域: %1").arg(jsonInfo.absoluteFilePath());
                return false;
            }
            const QString imageName = object.value(QStringLiteral("imagePath")).toString();
            const QString imagePath = QDir(directory.absoluteFilePath()).filePath(imageName);
            const QImage image(imagePath);
            if (imageName.isEmpty() || QFileInfo(imageName).fileName() != imageName || image.isNull() ||
                image.width() != object.value(QStringLiteral("imageWidth")).toInt() ||
                image.height() != object.value(QStringLiteral("imageHeight")).toInt() ||
                imageNames.contains(imageName.toCaseFolded()))
            {
                m_DataGenErrorMessage = QString(u8"分类图像缺失、尺寸不符或文件名冲突: %1").arg(imagePath);
                return false;
            }
            imageNames.insert(imageName.toCaseFolded());
            samples[classIndex].append(jsonInfo.absoluteFilePath());
        }
    }
    const QDir root(YtYoloDefine::toGetDataPath() + QStringLiteral("/") + m_ProcessName);
    QStringList sortedNames;
    for (int classIndex = 0; classIndex < names.size(); ++classIndex)
    {
        const QString &name = names.at(classIndex);
        QStringList &classSamples = samples[classIndex];
        if (classSamples.size() < 2)
        {
            m_DataGenErrorMessage = QString(u8"类别 %1 至少需要两张已标注图像才能划分训练和验证集").arg(name);
            return false;
        }
        std::mt19937 random(static_cast<unsigned>(classIndex));
        std::shuffle(classSamples.begin(), classSamples.end(), random);
        const int valCount = qBound(1, qRound(classSamples.size() * 0.3), static_cast<int>(classSamples.size()) - 1);
        for (int index = 0; index < classSamples.size(); ++index)
        {
            const QString jsonPath = classSamples.at(index);
            YtRoiLabelSet labels;
            labels.toLoadJsonFile(jsonPath);
            const QString split = index < valCount ? QStringLiteral("val") : QStringLiteral("train");
            const QString classDir = root.filePath(split + QStringLiteral("/") + name);
            if (!QDir().mkpath(classDir))
            {
                m_DataGenErrorMessage = QString(u8"无法创建分类目录: %1").arg(classDir);
                return false;
            }
            const QString source = QDir(QFileInfo(jsonPath).absolutePath()).filePath(labels.m_imagePath);
            const QString destination = QDir(classDir).filePath(labels.m_imagePath);
            if (!QFile::copy(source, destination))
            {
                m_DataGenErrorMessage = QString(u8"复制分类图像失败: %1 -> %2").arg(source, destination);
                return false;
            }
            (index < valCount ? m_ValLabelSetfilename[classIndex] : m_TrainLabelSetfilename[classIndex])
                .append(jsonPath);
        }
        sortedNames.append(name);
    }
    std::sort(sortedNames.begin(),
              sortedNames.end(),
              [](const QString &left, const QString &right)
              {
                  const auto leftCodePoints = left.toUcs4();
                  const auto rightCodePoints = right.toUcs4();
                  return std::lexicographical_compare(leftCodePoints.cbegin(),
                                                      leftCodePoints.cend(),
                                                      rightCodePoints.cbegin(),
                                                      rightCodePoints.cend());
              });
    QJsonArray classMapping;
    for (const QString &name : sortedNames)
    {
        classMapping.append(name);
    }
    QSaveFile mapping(root.filePath(QStringLiteral("classes.json")));
    const QByteArray bytes = QJsonDocument(classMapping).toJson(QJsonDocument::Indented);
    if (!mapping.open(QIODevice::WriteOnly) || mapping.write(bytes) != bytes.size() || !mapping.commit())
    {
        m_DataGenErrorMessage = QString(u8"无法写入分类类别映射: %1").arg(mapping.fileName());
        return false;
    }
    return true;
}

bool DataSetForm::generateYoloData(const QFileInfoList &directories, int runIndex)
{
    const QVector<QString> names = trainingLabelNames();
    if (names.isEmpty())
    {
        m_DataGenErrorMessage = QString(u8"项目没有目标类别");
        return false;
    }
    QStringList samples;
    for (const QFileInfo &directory : directories)
    {
        const QFileInfoList files = QDir(directory.absoluteFilePath())
                                        .entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QFileInfo &file : files)
        {
            if (file.fileName() != QStringLiteral("DefineLabel.json"))
            {
                samples.append(file.absoluteFilePath());
            }
        }
    }
    if (samples.size() < 2)
    {
        m_DataGenErrorMessage = QString(u8"至少需要两张标注图像才能划分训练和验证集");
        return false;
    }
    std::mt19937 random(0);
    std::shuffle(samples.begin(), samples.end(), random);
    const int valCount = qBound(1, qRound(samples.size() * 0.3), static_cast<int>(samples.size()) - 1);
    const QDir root(YtYoloDefine::toGetDataPath() + QStringLiteral("/") + m_ProcessName);
    QSet<QString> stems;
    for (int index = 0; index < samples.size(); ++index)
    {
        const QString jsonPath = samples.at(index);
        QFile sourceJson(jsonPath);
        if (!sourceJson.open(QIODevice::ReadOnly))
        {
            m_DataGenErrorMessage = QString(u8"无法读取标注: %1").arg(jsonPath);
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(sourceJson.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            m_DataGenErrorMessage = QString(u8"标注 JSON 无效: %1").arg(jsonPath);
            return false;
        }
        const QJsonObject object = document.object();
        const QJsonArray shapes = object.value(QStringLiteral("shapes")).toArray();
        const QString imageName = object.value(QStringLiteral("imagePath")).toString();
        const QString source = QDir(QFileInfo(jsonPath).absolutePath()).filePath(imageName);
        const QImage image(source);
        const QString stem = QFileInfo(imageName).completeBaseName();
        if (imageName.isEmpty() || QFileInfo(imageName).fileName() != imageName || image.isNull() ||
            image.width() != object.value(QStringLiteral("imageWidth")).toInt() ||
            image.height() != object.value(QStringLiteral("imageHeight")).toInt() || stem.isEmpty() ||
            stems.contains(stem.toCaseFolded()))
        {
            m_DataGenErrorMessage = QString(u8"图像缺失、尺寸不符或名称冲突: %1").arg(source);
            return false;
        }
        stems.insert(stem.toCaseFolded());
        bool background = object.value(QStringLiteral("sample_role")).toString() == QStringLiteral("background");
        for (const QJsonValue &value : shapes)
        {
            background |= value.toObject()
                              .value(QStringLiteral("label"))
                              .toString()
                              .compare(QStringLiteral("background"), Qt::CaseInsensitive) == 0;
        }
        const QString split = index < valCount ? QStringLiteral("val") : QStringLiteral("train");
        const QString imageDir = root.filePath(split + QStringLiteral("/images"));
        const QString labelDir = root.filePath(split + QStringLiteral("/labels"));
        if (!QDir().mkpath(imageDir) || !QDir().mkpath(labelDir))
        {
            m_DataGenErrorMessage = QString(u8"无法创建数据集目录: %1").arg(root.path());
            return false;
        }
        const QString destination = QDir(imageDir).filePath(imageName);
        QSaveFile labelFile(QDir(labelDir).filePath(stem + QStringLiteral(".txt")));
        if (!labelFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            m_DataGenErrorMessage = QString(u8"无法写入标签: %1").arg(labelFile.fileName());
            return false;
        }
        QTextStream out(&labelFile);
        out.setEncoding(QStringConverter::Utf8);
        int targetCount = 0;
        QSet<int> classes;
        for (const QJsonValue &value : shapes)
        {
            const QJsonObject shape = value.toObject();
            const QString label = shape.value(QStringLiteral("label")).toString();
            if (shape.value(QStringLiteral("training_role")).toString() == QStringLiteral("ignore"))
            {
                m_DataGenErrorMessage = QString(u8"标准 YOLO 数据集不接受忽略区: %1").arg(jsonPath);
                return false;
            }
            if (label.compare(QStringLiteral("background"), Qt::CaseInsensitive) == 0)
            {
                continue;
            }
            const int classIndex = names.indexOf(label);
            const QString expectedShape = runIndex == 0 ? QStringLiteral("rectangle") : QStringLiteral("polygon");
            const QJsonArray points = shape.value(QStringLiteral("points")).toArray();
            if (classIndex < 0 || shape.value(QStringLiteral("shape_type")).toString() != expectedShape ||
                points.size() < (runIndex == 0 ? 4 : 3) || background)
            {
                m_DataGenErrorMessage = QString(u8"目标类别、形状或背景状态无效: %1").arg(jsonPath);
                return false;
            }
            QVector<QPointF> polygon;
            for (const QJsonValue &pointValue : points)
            {
                const QJsonArray point = pointValue.toArray();
                if (point.size() != 2 || !point.at(0).isDouble() || !point.at(1).isDouble())
                {
                    m_DataGenErrorMessage = QString(u8"目标坐标无效: %1").arg(jsonPath);
                    return false;
                }
                const double x = point.at(0).toDouble();
                const double y = point.at(1).toDouble();
                if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > image.width() || y > image.height())
                {
                    m_DataGenErrorMessage = QString(u8"目标坐标超出图像范围: %1").arg(jsonPath);
                    return false;
                }
                polygon.append(QPointF(x, y));
            }
            double area = 0.0;
            double minX = image.width(), minY = image.height(), maxX = 0, maxY = 0;
            for (int pointIndex = 0; pointIndex < polygon.size(); ++pointIndex)
            {
                const QPointF &point = polygon.at(pointIndex);
                const QPointF &next = polygon.at((pointIndex + 1) % polygon.size());
                area += point.x() * next.y() - next.x() * point.y();
                minX = qMin(minX, point.x());
                minY = qMin(minY, point.y());
                maxX = qMax(maxX, point.x());
                maxY = qMax(maxY, point.y());
            }
            if (std::abs(area) <= 0.0 || maxX <= minX || maxY <= minY)
            {
                m_DataGenErrorMessage = QString(u8"目标面积无效: %1").arg(jsonPath);
                return false;
            }
            out << classIndex;
            if (runIndex == 0)
            {
                out << ' ' << (minX + maxX) / (2 * image.width()) << ' ' << (minY + maxY) / (2 * image.height()) << ' '
                    << (maxX - minX) / image.width() << ' ' << (maxY - minY) / image.height();
            }
            else
            {
                for (const QPointF &point : polygon)
                {
                    out << ' ' << point.x() / image.width() << ' ' << point.y() / image.height();
                }
            }
            out << '\n';
            classes.insert(classIndex);
            ++targetCount;
        }
        if (!background && targetCount == 0)
        {
            m_DataGenErrorMessage = QString(u8"普通样本没有目标: %1").arg(jsonPath);
            return false;
        }
        out.flush();
        if (out.status() != QTextStream::Ok || !labelFile.commit() || !QFile::copy(source, destination))
        {
            m_DataGenErrorMessage = QString(u8"保存数据集图像或标签失败: %1").arg(source);
            return false;
        }
        for (int classIndex : classes)
        {
            const int displayIndex = m_YtYoloSetPro.m_NameList.indexOf(names.at(classIndex));
            (index < valCount ? m_ValLabelSetfilename[displayIndex] : m_TrainLabelSetfilename[displayIndex])
                .append(jsonPath);
        }
    }
    return true;
}
void DataSetForm::toProDataGet(bool isadd)
{
    qInfo() << "DataSetForm::toProDataGet begin"
            << "process" << m_ProcessName << "isadd" << isadd;
    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    qInfo() << "DataSetForm::toProDataGet labels" << m_YtYoloSetPro.m_NameList.size() << m_YtYoloSetPro.m_NameList;
    const int runIndex = m_DataGenRunIndex;

    //
    m_TrainLabelSetfilename.clear(); // 按照类别进行文件存储
    m_ValLabelSetfilename.clear();   // 按照类别进行文件存储
    m_BackGroundJsonfilename.clear();
    m_BackGroudImgname.clear();

    //
    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        m_TrainLabelSetfilename.append(QStringList());
        m_ValLabelSetfilename.append(QStringList());
        m_BackGroundJsonfilename.append(QStringList());
    }
    if (m_YtYoloSetPro.m_NameList.size() < 1)
    {
        m_TrainLabelSetfilename.append(QStringList());
        m_ValLabelSetfilename.append(QStringList());
        m_BackGroundJsonfilename.append(QStringList());
    }
    //
    QFileInfoList getDirpp = toGetCheckedDataSheetDirList();
    if (isadd)
    {
        getDirpp.append(QFileInfo(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS"));
    }
    if (getDirpp.size() < 1)
    {
        m_DataGenErrorMessage = u8"未勾选任何标注目录, 无法生成数据集";
        qWarning() << "DataSetForm::toProDataGet no checked data sheet" << m_ProcessName;
        emit Sigfinish();
        return;
    }
    if (runIndex == 3)
    {
        if (!generateClassificationData(getDirpp))
        {
            qWarning().noquote() << m_DataGenErrorMessage;
        }
        emit Sigfinish();
        return;
    }
    if (!m_yoloEMode && (runIndex == 0 || runIndex == 2))
    {
        if (!generateYoloData(getDirpp, runIndex))
        {
            qWarning().noquote() << m_DataGenErrorMessage;
        }
        emit Sigfinish();
        return;
    }
    QDir temdir;
    qInfo() << "DataSetForm::toProDataGet run index" << runIndex;
    if (runIndex < 3)
    {
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/images");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/labels");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val/images");
        temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val/labels");
        if (m_yoloEMode)
        {
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/ignore");
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val/ignore");
        }
    }
    else
    {

        for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
        {
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train/" +
                          m_YtYoloSetPro.m_NameList.at(i));
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/test/" +
                          m_YtYoloSetPro.m_NameList.at(i));
        }
        if (m_YtYoloSetPro.m_NameList.size() < 1)
        {
            temdir.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train");
        }
    }

    // m_ShowProcessForm.show();
    // 打开两个文本，用于存储csv
    QFile Trinfile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/train.csv");
    QFile Valuefile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/val.csv");
    bool isopen = Trinfile.open(QIODevice::WriteOnly | QIODevice::Text);
    bool isValOpen = Valuefile.open(QIODevice::WriteOnly | QIODevice::Text);
    qInfo() << "DataSetForm::toProDataGet csv open" << Trinfile.fileName() << isopen << Valuefile.fileName()
            << isValOpen;
    if (!isopen || !isValOpen)
    {
        m_DataGenErrorMessage = "Open train.csv or val.csv failed";
        qWarning() << "DataSetForm::toProDataGet csv open failed" << Trinfile.errorString() << Valuefile.errorString();
        emit Sigfinish();
        return;
    }

    QTextStream Trainout(&Trinfile);
    QTextStream Valout(&Valuefile);
    if (runIndex == 4 || runIndex == 5)
    {
        Trainout.setEncoding(QStringConverter::Utf8);
        Valout.setEncoding(QStringConverter::Utf8);
    }
    // 保存label文件
    QDir temdirSave;
    qInfo() << "DataSetForm::toProDataGet checked dirs" << getDirpp.size() << getDirpp;
    QFile temfile;

    // BG类别索引，用来判断是否有背景类别；
    int BG_label_index = -1;
    if (m_YtYoloSetPro.m_NameList.contains("background"))
    {
        BG_label_index = m_YtYoloSetPro.m_NameList.lastIndexOf("background");
    }
    qInfo() << "DataSetForm::toProDataGet background index" << BG_label_index;

    MlsdProcesser temMlsdProcesser;
    for (int i = 0; i < getDirpp.size(); i++)
    {
        emit SigProcess(100.0 * i / getDirpp.size());
        QFileInfo teminfor = getDirpp.at(i);
        qInfo() << "DataSetForm::toProDataGet dir begin" << i << teminfor.absoluteFilePath();

        //
        QStringList temlisfilte;
        temlisfilte << "*.json";
        QFileInfoList getfilelis = YtYoloDefine::toGetPathFileInfo(teminfor.absoluteFilePath(), temlisfilte);

        qInfo() << "DataSetForm::toProDataGet json count" << teminfor.absoluteFilePath() << getfilelis.size();
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(getfilelis.begin(), getfilelis.end(), g);

        //
        int minvalval = qMax(1.0, getfilelis.size() * 0.3); // 固定好测试集和训练集的处理

        temdirSave.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/" + teminfor.fileName());
        if (!temdirSave.exists() && runIndex != 3)
        {
            temdirSave.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/" + teminfor.fileName());
        }

        for (int j = 0; j < getfilelis.size(); j++)
        {

            emit SigProcess(100.0 * i / getDirpp.size() + 100.0 / getDirpp.size() * j / getfilelis.size());

            const QString jsonPath = getfilelis.at(j).absoluteFilePath();
            qInfo() << "DataSetForm::toProDataGet json begin" << i << j << jsonPath;
            m_YtRoiLabelSet.toLoadJsonFile(jsonPath);
            qInfo() << "DataSetForm::toProDataGet json loaded" << jsonPath << "image" << m_YtRoiLabelSet.m_imagePath
                    << "labels" << m_YtRoiLabelSet.m_NameSetLis << "roi count" << m_YtRoiLabelSet.m_SetLabeset.size();

            if (m_yoloEMode)
            {
                const QVector<QString> labelNames = trainingLabelNames();
                bool labelNameChanged = false;
                for (LabelSet &label : m_YtRoiLabelSet.m_SetLabeset)
                {
                    if (label.isTrainingIgnored())
                    {
                        continue;
                    }
                    if (label.Name.compare(QStringLiteral("background"), Qt::CaseInsensitive) == 0)
                    {
                        m_YtRoiLabelSet.m_SampleRole = YtRoiLabelSet::SampleRole::Background;
                        continue;
                    }
                    if (!labelNames.contains(label.Name))
                    {
                        bool isClassIndex = false;
                        const int classIndex = label.Name.toInt(&isClassIndex);
                        if (!isClassIndex || classIndex < 0 || classIndex >= labelNames.size())
                        {
                            m_DataGenErrorMessage =
                                QString(u8"YOLOE标注包含未定义类别 %1: %2").arg(label.Name, jsonPath);
                            qWarning() << m_DataGenErrorMessage;
                            emit Sigfinish();
                            return;
                        }
                        label.Name = labelNames.at(classIndex);
                        labelNameChanged = true;
                    }
                    if (label.toGetRoiType() != LabelSet::LpolygonROI)
                    {
                        m_DataGenErrorMessage = QString(u8"YOLOE仅接受多边形mask标注: %1").arg(jsonPath);
                        qWarning() << m_DataGenErrorMessage;
                        emit Sigfinish();
                        return;
                    }
                }
                if (labelNameChanged)
                {
                    m_YtRoiLabelSet.m_NameSetLis.clear();
                    for (const LabelSet &label : m_YtRoiLabelSet.m_SetLabeset)
                    {
                        if (!label.isTrainingIgnored() && !label.Name.isEmpty() &&
                            label.Name.compare(QStringLiteral("background"), Qt::CaseInsensitive) != 0 &&
                            !m_YtRoiLabelSet.m_NameSetLis.contains(label.Name))
                        {
                            m_YtRoiLabelSet.m_NameSetLis.append(label.Name);
                        }
                    }
                }
            }

            // 负样本（背景图）在后面才加到训练集csv。
            // train.csv中，background类别样本一定要在train的其他类别样本后面，否则python端生成.cache那一步会报错。

            if (!m_yoloEMode && m_YtRoiLabelSet.m_NameSetLis.contains("background"))
            {
                if (BG_label_index < 0 || BG_label_index >= m_BackGroundJsonfilename.size())
                {
                    m_DataGenErrorMessage = "Json contains background, but project class list has no background";
                    qWarning() << "DataSetForm::toProDataGet invalid background label" << jsonPath << "bg index"
                               << BG_label_index << "groups" << m_BackGroundJsonfilename.size();
                    emit Sigfinish();
                    return;
                }
                m_BackGroudImgname.append(
                    QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                m_BackGroundJsonfilename[BG_label_index].append(jsonPath);
                continue;
            }

            int FindexNameindex = m_YtRoiLabelSet.m_imagePath.lastIndexOf(".");
            const QString imagePath = QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath);
            if (m_YtRoiLabelSet.m_imagePath.isEmpty())
            {
                qWarning() << "DataSetForm::toProDataGet empty imagePath" << jsonPath;
                if (m_yoloEMode)
                {
                    m_DataGenErrorMessage = QString(u8"YOLOE标注缺少图像路径: %1").arg(jsonPath);
                    emit Sigfinish();
                    return;
                }
                continue;
            }
            if (!QFile::exists(imagePath))
            {
                qWarning() << "DataSetForm::toProDataGet image missing" << jsonPath << imagePath;
                if (m_yoloEMode)
                {
                    m_DataGenErrorMessage = QString(u8"YOLOE标注图像不存在: %1").arg(imagePath);
                    emit Sigfinish();
                    return;
                }
                continue;
            }
            QString datasetImagePath = imagePath;
            QString datasetLabelPath =
                QString("%1/%2.txt").arg(temdirSave.path()).arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex));
            QString segmentLabelDirectory = temdirSave.path();
            if (m_yoloEMode)
            {
                const int targetCount = m_YtRoiLabelSet.targetCount(trainingLabelNames());
                if (m_YtRoiLabelSet.isBackgroundSample() && targetCount > 0)
                {
                    m_DataGenErrorMessage = QString(u8"YOLOE背景样本不能包含普通目标: %1").arg(jsonPath);
                    qWarning() << m_DataGenErrorMessage;
                    emit Sigfinish();
                    return;
                }
                if (!m_YtRoiLabelSet.isBackgroundSample() && targetCount < 1)
                {
                    m_DataGenErrorMessage = QString(u8"YOLOE普通样本没有有效目标: %1").arg(jsonPath);
                    qWarning() << m_DataGenErrorMessage;
                    emit Sigfinish();
                    return;
                }
                const QString split = j < minvalval ? QStringLiteral("val") : QStringLiteral("train");
                const QString splitDirectory =
                    QDir(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName).filePath(split);
                const QString stem = QFileInfo(m_YtRoiLabelSet.m_imagePath).completeBaseName();
                datasetImagePath =
                    QDir(splitDirectory + QStringLiteral("/images")).filePath(stem + QStringLiteral(".png"));
                datasetLabelPath =
                    QDir(splitDirectory + QStringLiteral("/labels"))
                        .filePath(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex) + QStringLiteral(".txt"));
                segmentLabelDirectory = QFileInfo(datasetLabelPath).absolutePath();
                if (QFileInfo::exists(datasetImagePath) || QFileInfo::exists(datasetLabelPath))
                {
                    m_DataGenErrorMessage =
                        QString(u8"YOLOE数据集中存在同名文件: %1").arg(QFileInfo(datasetImagePath).fileName());
                    qWarning() << m_DataGenErrorMessage << jsonPath;
                    emit Sigfinish();
                    return;
                }
                const QString maskPath =
                    QDir(splitDirectory + QStringLiteral("/ignore")).filePath(stem + QStringLiteral(".png"));
                if (QFileInfo::exists(maskPath) || !saveYoloeImageAndMask(imagePath,
                                                                          datasetImagePath,
                                                                          maskPath,
                                                                          jsonPath,
                                                                          m_YtRoiLabelSet,
                                                                          m_backgroundFill,
                                                                          &m_DataGenErrorMessage))
                {
                    if (m_DataGenErrorMessage.isEmpty())
                        m_DataGenErrorMessage = QString(u8"YOLOE忽略区文件已存在: %1").arg(maskPath);
                    qWarning() << m_DataGenErrorMessage;
                    emit Sigfinish();
                    return;
                }
            }
            if (runIndex == 0)
            {
                // 矩形目标识别
                m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 1)
            {
                m_YtRoiLabelSet.toSaveObbTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 2)
            {
                if (m_yoloEMode)
                {
                    if (!saveYoloeLabels(datasetLabelPath,
                                         m_YtRoiLabelSet,
                                         trainingLabelNames(),
                                         &m_DataGenErrorMessage))
                    {
                        qWarning() << m_DataGenErrorMessage;
                        emit Sigfinish();
                        return;
                    }
                }
                else
                {
                    m_YtRoiLabelSet.toSaveSegTxt(segmentLabelDirectory, trainingLabelNames());
                }
                if (m_yoloEMode && !QFileInfo::exists(datasetLabelPath))
                {
                    m_DataGenErrorMessage = QString(u8"生成YOLOE分割标签失败: %1").arg(datasetLabelPath);
                    qWarning() << m_DataGenErrorMessage << jsonPath;
                    emit Sigfinish();
                    return;
                }
                if (m_yoloEMode && !m_YtRoiLabelSet.isBackgroundSample() && QFileInfo(datasetLabelPath).size() <= 0)
                {
                    m_DataGenErrorMessage = QString(u8"YOLOE普通样本的分割标签为空: %1").arg(datasetLabelPath);
                    qWarning() << m_DataGenErrorMessage << jsonPath;
                    emit Sigfinish();
                    return;
                }
            }
            else if (runIndex == 3)
            {
                // 分类识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 4)
            {
                // 字符识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
            }
            else if (runIndex == 5)
            {
                // 字符位置识别
                // m_YtRoiLabelSet.toSaveDetcTxt(temdirSave.path(),m_YtYoloSetPro.m_NameList);
                m_YtRoiLabelSet.toSaveFourPosTxt(temdirSave.path(), m_YtYoloSetPro.m_NameList);
            }
            if (runIndex == 4)
            {
                if (m_YtRoiLabelSet.m_SetLabeset.size() > 0 && m_YtRoiLabelSet.m_SetLabeset[0].Name.length() > 0)
                {
                    if (j < qMin(100, minvalval))
                    {
                        Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << "\t"
                               << QString("%1").arg(m_YtRoiLabelSet.m_SetLabeset[0].Name) << "\n";
                        m_ValLabelSetfilename[0].append(
                            QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                    }
                    else
                    {
                        Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath)
                                 << "\t" << QString("%1").arg(m_YtRoiLabelSet.m_SetLabeset[0].Name) << "\n";
                        m_TrainLabelSetfilename[0].append(
                            QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath));
                    }
                    //
                    QString getstr = m_YtRoiLabelSet.m_SetLabeset[0].Name;
                    for (int characterIndex = 0; characterIndex < getstr.length(); characterIndex++)
                    {
                        if (!m_GetKeys.contains(getstr.mid(characterIndex, 1)))
                        {
                            m_GetKeys.insert(getstr.mid(characterIndex, 1), getstr.mid(characterIndex, 1));
                        }
                    }
                }
            }
            else if (runIndex == 5)
            {
                if (j < qMin(100, minvalval))
                {
                    // 进入测试集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                           << QString("%1/%2.txt")
                                  .arg(temdirSave.path())
                                  .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                           << "\n";
                }
                else
                {
                    // 进入训练集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                             << QString("%1/%2.txt")
                                    .arg(temdirSave.path())
                                    .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                             << "\n";
                }
            }
            else if (runIndex == 6) // 直线mlsd数据集
            {
                // 测试集
                if (j < minvalval)
                {
                    temMlsdProcesser.m_ValidJsonObj.addSingleJson(getfilelis.at(j).absoluteFilePath(),
                                                                  teminfor.absoluteFilePath());

                    // ui列表数据
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Valout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                           << QString("%1/%2.txt")
                                  .arg(temdirSave.path())
                                  .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                           << "\n";
                }

                // 训练集
                else
                {
                    temMlsdProcesser.m_TrainJsonObj.addSingleJson(getfilelis.at(j).absoluteFilePath(),
                                                                  teminfor.absoluteFilePath());

                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                    }
                    Trainout << QString("%1/%2").arg(getfilelis.at(j).path()).arg(m_YtRoiLabelSet.m_imagePath) << " "
                             << QString("%1/%2.txt")
                                    .arg(temdirSave.path())
                                    .arg(m_YtRoiLabelSet.m_imagePath.left(FindexNameindex))
                             << "\n";
                }
            }
            else
            {
                //
                if (j < minvalval)
                {
                    // 进入测试集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_ValLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                        if (runIndex == 3)
                        {
                            QFile::copy(getfilelis.at(j).path() + "/" + m_YtRoiLabelSet.m_imagePath,
                                        QString("%1/test/%2/%3_%4")
                                            .arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName)
                                            .arg(m_YtRoiLabelSet.m_NameSetLis[z])
                                            .arg(i)
                                            .arg(m_YtRoiLabelSet.m_imagePath));
                        }
                    }
                    Valout << datasetImagePath << "," << datasetLabelPath << "\n";
                }
                else
                {
                    // 进入训练集
                    for (int z = 0; z < m_YtRoiLabelSet.m_NameSetLis.size(); z++)
                    {
                        int getsetindex = m_YtYoloSetPro.m_NameList.indexOf(m_YtRoiLabelSet.m_NameSetLis.at(z));
                        if (getsetindex >= 0)
                        {
                            m_TrainLabelSetfilename[getsetindex].append(jsonPath);
                        }
                        else
                        {
                            qWarning() << "DataSetForm::toProDataGet unknown label" << jsonPath
                                       << m_YtRoiLabelSet.m_NameSetLis.at(z);
                        }
                        if (runIndex >= 3)
                        {
                            QFile::copy(getfilelis.at(j).path() + "/" + m_YtRoiLabelSet.m_imagePath,
                                        QString("%1/train/%2/%3_%4")
                                            .arg(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName)
                                            .arg(m_YtRoiLabelSet.m_NameSetLis[z])
                                            .arg(i)
                                            .arg(m_YtRoiLabelSet.m_imagePath));
                        }
                    }
                    Trainout << datasetImagePath << "," << datasetLabelPath << "\n";
                }
            }
        }
    }

    if (runIndex == 6)
    {
        temMlsdProcesser.m_TrainJsonObj.toSaveColletedJson(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName +
                                                           "/train.json");
        temMlsdProcesser.m_ValidJsonObj.toSaveColletedJson(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName +
                                                           "/val.json");
    }
    // 增加负样本到训练集
    for (int j = 0; j < m_BackGroudImgname.size(); j++)
    {
        Trainout << m_BackGroudImgname.at(j) << ',' << QString("SIGNAL_BACKGROUND_IMG") << "\n";
    }

    qInfo() << "DataSetForm::toProDataGet finish"
            << "train groups" << m_TrainLabelSetfilename.size() << "val groups" << m_ValLabelSetfilename.size() << "bg"
            << m_BackGroudImgname.size();
    Trinfile.close();
    Valuefile.close();

    emit Sigfinish();
}

void DataSetForm::toShowData()
{
    qDebug() << "toshow 11111111";
    if (m_YtYoloSetPro.m_NameList.size() == 0 && ui->comboBox->currentIndex() == 4)
    {
        ui->TW_LabeSet->setRowCount(1);
        // 序号
        ui->TW_LabeSet->setItem(0, 0, new QTableWidgetItem(QString::number(1)));
        // 名称
        ui->TW_LabeSet->setItem(0, 1, new QTableWidgetItem(u8"数据集"));
        // 全部初始化为0
        ui->TW_LabeSet->setItem(0, 2, new QTableWidgetItem(QString::number(m_TrainLabelSetfilename[0].size())));
        ui->TW_LabeSet->setItem(0, 3, new QTableWidgetItem(QString::number(m_ValLabelSetfilename[0].size())));
        ui->TW_LabeSet->setItem(0, 4, new QTableWidgetItem(QString::number(0)));
        ui->ToolTreeWidget->clear();
        //
        QTreeWidgetItem *itemParent = nullptr;
        QFileIconProvider ticoset;

        itemParent = new QTreeWidgetItem(ui->ToolTreeWidget);
        itemParent->setText(0, u8"字符集");
        itemParent->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
        itemParent->setIcon(0, ticoset.icon(QFileIconProvider::Folder));
        for (int j = 0; j < m_TrainLabelSetfilename[0].size(); j++)
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_TrainLabelSetfilename[0][j]));
            treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
        }

        for (int j = 0; j < m_ValLabelSetfilename[0].size(); j++)
        {
            QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_ValLabelSetfilename[0][j]));
            treeItem->setIcon(0, ticoset.icon(QFileIconProvider::Network));
        }
        for (int i = 0; i < 1; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (ui->TW_LabeSet->item(i, j))
                {
                    ui->TW_LabeSet->item(i, j)->setTextAlignment(Qt::AlignCenter);
                }
            }
        }

        ui->ToolTreeWidget->expandAll();

        return;
    }

    // 开始构造表格
    ui->TW_LabeSet->setRowCount(m_YtYoloSetPro.m_NameList.size());

    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        qDebug() << "toshow 11111111-i" << i;
        // 序号
        ui->TW_LabeSet->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        // 名称
        ui->TW_LabeSet->setItem(i, 1, new QTableWidgetItem(m_YtYoloSetPro.m_NameList.at(i)));
        // 全部初始化为0
        if (i >= m_TrainLabelSetfilename.size())
        {
            qDebug() << "toshow 11111111-2" << i;
            ui->TW_LabeSet->setItem(i, 2, new QTableWidgetItem(QString::number(0)));
            ui->TW_LabeSet->setItem(i, 3, new QTableWidgetItem(QString::number(0)));
            ui->TW_LabeSet->setItem(i, 4, new QTableWidgetItem(QString::number(0)));
        }
        else
        {
            qDebug() << "toshow 11111111-3" << i;
            if (i < m_TrainLabelSetfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        2,
                                        new QTableWidgetItem(QString::number(m_TrainLabelSetfilename.at(i).size())));
            }
            if (i < m_ValLabelSetfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        3,
                                        new QTableWidgetItem(QString::number(m_ValLabelSetfilename.at(i).size())));
            }
            if (i < m_BackGroundJsonfilename.size())
            {
                ui->TW_LabeSet->setItem(i,
                                        4,
                                        new QTableWidgetItem(QString::number(m_BackGroundJsonfilename.at(i).size())));
            }

            //            //增加background数量
            //            if(m_YtYoloSetPro.m_NameList.contains("background"))
            //            {
            //                int BG_label_index = m_YtYoloSetPro.m_NameList.lastIndexOf("background");
            //                qDebug()<<"show data row: "<<i<<" ;index of BG_label_index: "<<BG_label_index;
            //                ui->TW_LabeSet->setItem(i,4,new
            //                QTableWidgetItem(QString::number(m_BackGroundJsonfilename.at(i).size())));
            //            }
        }
    }

    //    for(int i=0;i<m_YtYoloSetPro.m_NameList.size();i++)
    //    {
    //        for(int j=0;j<5;j++)
    //        {
    //            qDebug()<<"toshow 11111111-4"<<i<<j;
    //            ui->TW_LabeSet->item(i,j)->setTextAlignment(Qt::AlignCenter);
    //        }
    //    }

    ui->ToolTreeWidget->clear();
    //
    QTreeWidgetItem *itemParent = nullptr;
    QFileIconProvider ticoset;
    for (int i = 0; i < m_YtYoloSetPro.m_NameList.size(); i++)
    {
        itemParent = new QTreeWidgetItem(ui->ToolTreeWidget);
        itemParent->setText(0, m_YtYoloSetPro.m_NameList.at(i));
        itemParent->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
        itemParent->setIcon(0, ticoset.icon(QFileIconProvider::Folder));
        if (i < m_TrainLabelSetfilename.size())
        {
            for (int j = 0; j < m_TrainLabelSetfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_TrainLabelSetfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
            }
        }
        if (i < m_ValLabelSetfilename.size())
        {
            for (int j = 0; j < m_ValLabelSetfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem = new QTreeWidgetItem(itemParent, QStringList(m_ValLabelSetfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::Network));
            }
        }
        if (i < m_BackGroundJsonfilename.size())
        {
            for (int j = 0; j < m_BackGroundJsonfilename[i].size(); j++)
            {
                QTreeWidgetItem *treeItem =
                    new QTreeWidgetItem(itemParent, QStringList(m_BackGroundJsonfilename[i][j]));
                treeItem->setIcon(0, ticoset.icon(QFileIconProvider::File));
            }
        }
    }
    ui->ToolTreeWidget->expandAll();
}

void DataSetForm::toSaveData(int changele, bool datapp)
{
    QFile file(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/LablData.data");
    if (!file.open(QIODevice::WriteOnly))
    {
        m_DataGenErrorMessage = QString(u8"无法写入数据集信息: %1, %2").arg(file.fileName(), file.errorString());
        return;
    }
    QDataStream out(&file);

    out << m_BackGroundJsonfilename << m_TrainLabelSetfilename << m_ValLabelSetfilename << ui->comboBox->currentIndex();

    if (out.status() != QDataStream::Ok || !file.flush())
    {
        m_DataGenErrorMessage = QString(u8"写入数据集信息失败: %1, %2").arg(file.fileName(), file.errorString());
        return;
    }
    file.close();
    // 尝试存储yml文件
    QFile filet(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/Config.yaml");
    if (!filet.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        m_DataGenErrorMessage = QString(u8"无法写入数据配置: %1").arg(filet.fileName());
        return;
    }
    QTextStream outt(&filet);
    outt.setEncoding(QStringConverter::Utf8);

    outt << QStringLiteral("path: ") << yamlQuoted(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName) << "\n";
    outt << QString("train: %1")
                .arg(m_yoloEMode || m_DataGenRunIndex < 3 ? QStringLiteral("train/images") : QStringLiteral("train"))
         << "\n";
    outt << QString("val: %1").arg(m_yoloEMode || m_DataGenRunIndex < 3 ? QStringLiteral("val/images")
                                                                        : QStringLiteral("val"))
         << "\n";
    outt << "names:"
         << "\n";
    const QVector<QString> names = trainingLabelNames();
    for (int i = 0; i < names.size(); i++)
    {
        outt << QString("  %1: %2").arg(i).arg(yamlQuoted(names.at(i))) << "\n";
    }
    outt << QString("channels: %1").arg(changele) << "\n";
    if (m_yoloEMode)
    {
        outt << "ignore:\n  train: train/ignore\n  val: val/ignore\n";
        outt << "background_fill: " << m_backgroundFill << "\n";
    }
    if (datapp)
    {
        outt << "augment:"
             << "\n";
        outt << QString("  %1: %2").arg("flipud").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("fliplr").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("mosaic").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("mixup").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("hsv_h").arg(0.015) << "\n";
        outt << QString("  %1: %2").arg("hsv_s").arg(0.7) << "\n";
        outt << QString("  %1: %2").arg("hsv_v").arg(0.4) << "\n";
        outt << QString("  %1: %2").arg("scale").arg(0.5) << "\n";
        outt << QString("  %1: %2").arg("shear").arg(0.1) << "\n";
        outt << QString("  %1: %2").arg("perspective").arg(0.1) << "\n";
    }
    outt.flush();
    if (outt.status() != QTextStream::Ok || filet.error() != QFileDevice::NoError)
        m_DataGenErrorMessage = QString(u8"写入数据配置失败: %1").arg(filet.fileName());
    filet.close();
    // 保存keys
    if (m_GetKeys.size() < 1)
    {
        return;
    }
    QFile files(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/Keys.txt");
    if (!files.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream outs(&files);
    outs.setEncoding(QStringConverter::Utf8);
    QList<QString> keylist = m_GetKeys.keys();
    for (int i = 0; i < keylist.size(); i++)
    {
        outs << keylist[i] << "\n";
    }

    files.close();
}

void DataSetForm::toLoadData()
{
    m_YtYoloSetPro.toLoadData(YtYoloDefine::toGetLabelPath() + "/" + m_ProcessName);
    m_BackGroundJsonfilename.clear();
    m_TrainLabelSetfilename.clear();
    m_ValLabelSetfilename.clear();
    m_loadedRunIndex = -1;

    const QString dataFilePath = YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/LablData.data";
    QFile file(dataFilePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "DataSetForm::toLoadData open failed" << dataFilePath << file.errorString();
        return;
    }

    QVector<QStringList> backGroundJsonfilename;
    QVector<QStringList> trainLabelSetfilename;
    QVector<QStringList> valLabelSetfilename;
    int value = -1;
    QDataStream out(&file);
    out >> backGroundJsonfilename >> trainLabelSetfilename >> valLabelSetfilename >> value;
    if (out.status() != QDataStream::Ok)
    {
        qWarning() << "DataSetForm::toLoadData stream failed" << dataFilePath << out.status();
        file.close();
        return;
    }
    file.close();

    if (value < 0 || value >= ui->comboBox->count())
    {
        qWarning() << "DataSetForm::toLoadData invalid run index" << dataFilePath << value;
        value = 0;
    }

    m_BackGroundJsonfilename = backGroundJsonfilename;
    m_TrainLabelSetfilename = trainLabelSetfilename;
    m_ValLabelSetfilename = valLabelSetfilename;
    m_loadedRunIndex = value;
    ui->comboBox->setCurrentIndex(value);
    qInfo() << "DataSetForm::toLoadData loaded" << dataFilePath << "train groups" << m_TrainLabelSetfilename.size()
            << "val groups" << m_ValLabelSetfilename.size() << "run index" << value;
}

QString DataSetForm::toGetRunType()
{

    if (m_TrainLabelSetfilename.size() < 1)
    {
        return u8"目标识别";
    }
    return ui->comboBox->currentText();
}

int DataSetForm::toGetRunIndex()
{
    toLoadData();
    if (m_forcedRunIndex >= 0 && m_loadedRunIndex != m_forcedRunIndex)
    {
        return -1;
    }
    if (m_forcedRunIndex >= 0)
    {
        ui->comboBox->setCurrentIndex(m_forcedRunIndex);
    }
    else if (m_yoloEMode)
    {
        ui->comboBox->setCurrentIndex(2);
    }
    if (m_TrainLabelSetfilename.size() < 1)
    {
        return -1;
    }
    if (!m_yoloEMode && m_forcedRunIndex >= 0)
    {
        const QDir root(YtYoloDefine::toGetDataPath() + QStringLiteral("/") + m_ProcessName);
        if (m_forcedRunIndex == 3 && !QFileInfo(root.filePath(QStringLiteral("classes.json"))).isFile())
        {
            return -1;
        }
        if ((m_forcedRunIndex == 0 || m_forcedRunIndex == 2) &&
            (QDir(root.filePath(QStringLiteral("train/images"))).entryList(QDir::Files).isEmpty() ||
             QDir(root.filePath(QStringLiteral("val/images"))).entryList(QDir::Files).isEmpty()))
        {
            return -1;
        }
    }
    return ui->comboBox->currentIndex();
}

void DataSetForm::on_PB_RunDataGen_clicked()
{
    if (toGetCheckedDataSheetDirList().size() < 1)
    {
        qWarning() << u8"未勾选任何标注目录, 无法生成数据集";
        QMessageBox::warning(0, u8"警告", u8"未勾选任何标注目录, 无法生成数据集", u8"确定");
        return;
    }
    const QString dataDirectory = YtYoloDefine::toGetDataPath() + "/" + m_ProcessName;
    const QDir existingDataDirectory(dataDirectory);
    const bool hasExistingData = existingDataDirectory.exists() &&
                                 !existingDataDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
    if (hasExistingData &&
        QMessageBox::question(this,
                              QString(u8"重新生成数据集"),
                              QString(u8"将清空并重新生成以下目录中的数据:\n%1\n是否继续?").arg(dataDirectory),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    QDir temdirPlus;
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    if (temdirPlus.exists())
    {
        qDebug() << temdirPlus.path();
        temdirPlus.removeRecursively();
    }
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName);
    if (temdirPlus.exists())
    {
        qDebug() << temdirPlus.path();
        temdirPlus.removeRecursively();
    }
    // toProDataGet();
    m_DataGenErrorMessage.clear();
    m_backgroundFill = QVector<int>{0, 128, 255}.at(ui->comboBackgroundFill->currentIndex());
    ui->comboBackgroundFill->setEnabled(false);
    m_DataGenRunIndex = m_forcedRunIndex >= 0 ? m_forcedRunIndex : (m_yoloEMode ? 2 : ui->comboBox->currentIndex());
    qInfo() << "DataSetForm::on_PB_RunDataGen_clicked start"
            << "process" << m_ProcessName << "run index" << m_DataGenRunIndex;
    m_GetKeys.clear();
    QThreadPool::globalInstance()->start(
        [this]()
        {
            toProDataGet(false);
        });
    m_ShowProcessForm.exec();
    // QMessageBox::about(0,u8"提示",u8"数据生成完成!");
}

void DataSetForm::on_PB_ViewPos_clicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName));
}

void DataSetForm::slot_ItemDoubleClicked(QTreeWidgetItem *item, int colum)
{

    if (ui->comboBox->currentIndex() == 4)
    {

        m_OverPlayShow.toClearData();
        int getindex = item->text(0).lastIndexOf(".");
        m_YtRoiLabelSet.toLoadJsonFile(item->text(0).left(getindex) + ".json");
        QImage temim = QImage(item->text(0));
        qDebug() << item->text(0).left(getindex) + ".json" << m_YtRoiLabelSet.m_SetLabeset.size();

        if (m_YtRoiLabelSet.m_SetLabeset.size() > 0)
        {

            m_OverPlayShow.append(DispTxt(m_YtRoiLabelSet.m_SetLabeset[0].Name,
                                          CMvPoint(0, temim.height()),
                                          QFont("Times", 33, 33),
                                          Qt::darkGreen));
        }
        ui->ytRoiShowDisp->toSetImage(temim);
        ui->ytRoiShowDisp->toUpdateShow();

        return;
    }

    if (!item->text(0).contains("json"))
    {
        return;
    }

    //
    m_YtRoiLabelSet.toLoadJsonFile(item->text(0));
    //
    int getindex = item->text(0).lastIndexOf("/");
    //
    if (ui->comboBox->currentIndex() == 0)
    {
        m_YtRoiLabelSet.toGenDetcOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 1)
    {
        m_YtRoiLabelSet.toGenObbOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 2)
    {
        m_YtRoiLabelSet.toGenSegOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    else if (ui->comboBox->currentIndex() == 5)
    {
        m_YtRoiLabelSet.toGenFourPosOveplay(m_OverPlayShow, m_YtYoloSetPro.m_NameList, m_YtYoloSetPro.m_ClorDefine);
    }
    ui->ytRoiShowDisp->toSetImage(QImage(item->text(0).left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath));
    ui->ytRoiShowDisp->toUpdateShow();
}

void DataSetForm::slotFinish()
{
    m_ShowProcessForm.close();
    ui->comboBackgroundFill->setEnabled(true);
    if (!m_DataGenErrorMessage.isEmpty())
    {
        QString error;
        if (!TaskRepository::UpdateProgress(m_ProcessName, QStringLiteral("data_failed"), &error))
        {
            qWarning().noquote() << error;
        }
        QMessageBox::warning(0, u8"警告", m_DataGenErrorMessage, u8"确定");
        m_DataGenErrorMessage.clear();
        return;
    }
    toShowData();
    toSaveData();
    if (!m_DataGenErrorMessage.isEmpty())
    {
        QString error;
        if (!TaskRepository::UpdateProgress(m_ProcessName, QStringLiteral("data_failed"), &error))
        {
            qWarning().noquote() << error;
        }
        QMessageBox::warning(this, QString(u8"警告"), m_DataGenErrorMessage);
        m_DataGenErrorMessage.clear();
        return;
    }
    QString error;
    if (!TaskRepository::UpdateProgress(m_ProcessName, QStringLiteral("dataset_ready"), &error))
    {
        qWarning().noquote() << error;
        QMessageBox::warning(this, QString(u8"警告"), error);
        return;
    }
    QMessageBox::about(0, u8"提示", u8"数据生成完成!");
}
void DataSetForm::on_PB_RunDataAppend_clicked()
{

    return;
    if (m_TrainLabelSetfilename.size() > 100)
    {
        QMessageBox::about(0, u8"提示", u8"数据已经足够!");

        return;
    }

    QImage temimage;
    QSize temsize;
    QDir temdirPlus;
    temdirPlus.setPath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    temdirPlus.mkpath(YtYoloDefine::toGetDataPath() + "/" + m_ProcessName + "/AddDataS");
    int pzeYY = 0;
    for (int i = 0; i < m_TrainLabelSetfilename.size(); i++)
    {
        for (int j = 0; j < m_TrainLabelSetfilename[i].size(); j++)
        {
            QString getStr = m_TrainLabelSetfilename[i][j];
            m_YtRoiLabelSet.toLoadJsonFile(getStr);
            int getindex = getStr.lastIndexOf("/");
            //
            temimage.load(getStr.left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath);
            temsize = temimage.size();
            for (int zx = -35; zx <= 35; zx += 10)
            {
                for (int zy = -35; zy < 35; zy += 10)
                {
                    QString filename = temdirPlus.path() + "/" + QString::number(pzeYY) + ".jpg";
                    qDebug() << filename << zx << zy;
                    temimage.copy(zx, zy, temsize.width(), temsize.height()).save(filename);
                    //
                    m_YtRoiLabelSet.toSetFileName(QString::number(pzeYY) + ".jpg");
                    m_YtRoiLabelSet.toMovePos(-zx, -zy);

                    m_YtRoiLabelSet.toSaveJson(temdirPlus.path());

                    m_YtRoiLabelSet.toMovePos(zx, zy);
                    pzeYY += 1;
                }
            }
        }
    }

    for (int i = 0; i < m_ValLabelSetfilename.size(); i++)
    {
        for (int j = 0; j < m_ValLabelSetfilename[i].size(); j++)
        {
            QString getStr = m_ValLabelSetfilename[i][j];
            m_YtRoiLabelSet.toLoadJsonFile(getStr);
            int getindex = getStr.lastIndexOf("/");
            //
            temimage.load(getStr.left(getindex) + "/" + m_YtRoiLabelSet.m_imagePath);
            temsize = temimage.size();
            for (int zx = -35; zx <= 35; zx += 10)
            {
                for (int zy = -35; zy < 35; zy += 10)
                {
                    QString filename = temdirPlus.path() + "/" + QString::number(pzeYY) + ".jpg";
                    qDebug() << filename << zx << zy;
                    temimage.copy(zx, zy, temsize.width(), temsize.height()).save(filename);
                    //
                    m_YtRoiLabelSet.toSetFileName(QString::number(pzeYY) + ".jpg");
                    m_YtRoiLabelSet.toMovePos(-zx, -zy);
                    m_YtRoiLabelSet.toSaveJson(temdirPlus.path());
                    m_YtRoiLabelSet.toMovePos(zx, zy);
                    pzeYY += 1;
                }
            }
        }
    }
    //
    toProDataGet(true);
    toShowData();

    QMessageBox::about(0, u8"提示", u8"数据生成完成!");
}
