#include "mlsdprocesser.h"

#include <QStringConverter>

MlsdProcesser::MlsdProcesser()
{
}

MlsdJson::MlsdJson()
{
    m_CurrAddJson = QJsonObject();
    m_ConvertedJson = QJsonObject();
    m_OutJsonArray = QJsonArray();
}

void MlsdJson::clearData()
{
    m_CurrAddJson = QJsonObject();
    m_ConvertedJson = QJsonObject();
    m_OutJsonArray = QJsonArray();
}

void MlsdJson::addSingleJson(QString JsonPath)
{
    QFile file(JsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << QString(u8"无法打开文件:") << JsonPath;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileData);
    if (doc.isNull())
    {
        qWarning() << QString(u8"无效的 JSON 文件:") << JsonPath;
    }

    m_CurrAddJson = doc.object();
    m_ConvertedJson = convertSingleToValid(m_CurrAddJson);
    m_OutJsonArray.append(m_ConvertedJson);
}

void MlsdJson::addSingleJson(QString JsonPath, QString ImgDir)
{
    QFile file(JsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << QString(u8"无法打开文件:") << JsonPath;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileData);
    if (doc.isNull())
    {
        qWarning() << QString(u8"无效的 JSON 文件:") << JsonPath;
    }

    m_CurrAddJson = doc.object();
    m_ConvertedJson = convertSingleToValid(m_CurrAddJson, ImgDir);
    m_OutJsonArray.append(m_ConvertedJson);
}

void MlsdJson::toSaveColletedJson(QString SavePath)
{
    // 写入 train/valid.json 文件
    QFile outputFile(SavePath);
    if (!outputFile.open(QIODevice::WriteOnly))
    {
        qWarning() << u8"无法创建输出文件:" << SavePath;
        return;
    }

    QJsonDocument outputDoc(m_OutJsonArray);
    outputFile.write(outputDoc.toJson(QJsonDocument::Indented));
    outputFile.close();
}

QJsonObject MlsdJson::convertSingleToValid(const QJsonObject &singleObj)
{
    QJsonObject validObj;

    // 设置 filename
    validObj["filename"] = singleObj["imagePath"].toString();

    // 转换 lines 格式
    QJsonArray linesArray;
    QJsonArray shapesArray = singleObj["shapes"].toArray();

    for (const QJsonValue &shapeValue : shapesArray)
    {
        QJsonObject shape = shapeValue.toObject();
        if (shape["shape_type"].toString() == "line")
        {
            QJsonArray pointsArray = shape["points"].toArray();
            if (pointsArray.size() == 2)
            {
                QJsonArray lineArray;
                // 提取两个点的坐标并展平
                QJsonArray point1 = pointsArray[0].toArray();
                QJsonArray point2 = pointsArray[1].toArray();

                if (point1.size() == 2 && point2.size() == 2)
                {
                    lineArray.append(point1[0].toDouble()); // x1
                    lineArray.append(point1[1].toDouble()); // y1
                    lineArray.append(point2[0].toDouble()); // x2
                    lineArray.append(point2[1].toDouble()); // y2
                    linesArray.append(lineArray);
                }
            }
        }
    }

    validObj["lines"] = linesArray;
    validObj["height"] = singleObj["imageHeight"].toInt();
    validObj["width"] = singleObj["imageWidth"].toInt();

    return validObj;
}

QJsonObject MlsdJson::convertSingleToValid(const QJsonObject &singleObj, QString ImgDir)
{
    QJsonObject validObj;

    // 设置 filename
    validObj["filename"] = ImgDir + "/" + singleObj["imagePath"].toString();

    // 转换 lines 格式
    QJsonArray linesArray;
    QJsonArray shapesArray = singleObj["shapes"].toArray();

    for (const QJsonValue &shapeValue : shapesArray)
    {
        QJsonObject shape = shapeValue.toObject();
        if (shape["shape_type"].toString() == "line")
        {
            QJsonArray pointsArray = shape["points"].toArray();
            if (pointsArray.size() == 2)
            {
                QJsonArray lineArray;
                // 提取两个点的坐标并展平
                QJsonArray point1 = pointsArray[0].toArray();
                QJsonArray point2 = pointsArray[1].toArray();

                if (point1.size() == 2 && point2.size() == 2)
                {
                    lineArray.append(point1[0].toDouble()); // x1
                    lineArray.append(point1[1].toDouble()); // y1
                    lineArray.append(point2[0].toDouble()); // x2
                    lineArray.append(point2[1].toDouble()); // y2
                    linesArray.append(lineArray);
                }
            }
        }
    }

    validObj["lines"] = linesArray;
    validObj["height"] = singleObj["imageHeight"].toInt();
    validObj["width"] = singleObj["imageWidth"].toInt();

    return validObj;
}

MlsdYaml::MlsdYaml()
{
}

void MlsdYaml::setBatchSize(int BatchSize)
{
    m_Batchsize = BatchSize;
}

void MlsdYaml::setEpochNum(int EpochNum)
{
    m_EpochNum = EpochNum;
}

void MlsdYaml::setInputSize(int InputSize)
{
    m_InputSize = InputSize;
}

void MlsdYaml::setPretrainedPath(QString path)
{
    m_PretrainedPath = path;
}

void MlsdYaml::setImgDir(QString path)
{
    m_ImgDir = path;
}

void MlsdYaml::setTrainJsonPath(QString path)
{
    m_TrainJsonPath = path;
}

void MlsdYaml::setValidJsonPath(QString path)
{
    m_ValidJsonPath = path;
}

void MlsdYaml::setOutputDir(QString path)
{
    m_OutputDir = path;
}

void MlsdYaml::SaveMlsdYaml(QString yamlSavePath)
{
    QFile tYamlFile(yamlSavePath);
    if (!tYamlFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }
    QTextStream stream(&tYamlFile);
    stream.setEncoding(QStringConverter::Utf8);
    // 设置使用空格缩进
    stream.setFieldAlignment(QTextStream::AlignLeft);

    stream << QString("datasets:\n");
    stream << QString("    name: 'wireframe'\n");
    stream << QString("    input_size: %1\n").arg(m_InputSize);
    stream << QString("\n");

    stream << QString("model:\n");
    stream << QString("    model_name: 'mobilev2_mlsd'\n");
    stream << QString("    with_deconv: True\n");
    stream << QString("\n");

    stream << QString("train:\n");
    stream << QString("    save_dir: %1").arg(m_OutputDir) << "\n";
    stream << QString("    img_dir: %1").arg(m_ImgDir) << "\n";
    stream << QString("    label_fn: %1").arg(m_TrainJsonPath) << "\n";
    stream << QString("    num_train_epochs: %1").arg(m_EpochNum) << "\n";
    stream << QString("    batch_size: %1").arg(m_Batchsize) << "\n";
    stream << QString("    learning_rate: 0.003\n");
    stream << QString("    use_step_lr_policy: True\n");
    stream << QString("    weight_decay: 0.000001\n");
    stream << QString("    load_from: %1").arg(m_PretrainedPath) << "\n";
    stream << QString("    warmup_steps: 100\n");
    stream << QString("    milestones: [ 50, 100, 150 ]\n");
    stream << QString("    milestones_in_epo: True\n");
    stream << QString("    lr_decay_gamma: 0.2\n");
    stream << QString("\n");
    stream << QString("    data_cache_dir: \"./data/wireframe_cache/\"\n");
    stream << QString("    with_cache: False\n");
    stream << QString("    cache_to_mem: False\n");
    stream << QString("\n");

    stream << QString("val:\n");
    stream << QString("    img_dir: %1").arg(m_ImgDir) << "\n";
    stream << QString("    label_fn: %1").arg(m_ValidJsonPath) << "\n";
    stream << QString("    batch_size: 4\n");
    stream << QString("    val_after_epoch: 1\n");
    stream << QString("\n");

    stream << QString("loss:\n");
    stream << QString(
        "    loss_weight_dict_list: [ { 'tp_center_loss': 10.0,'sol_center_loss': 1.0,'tp_match_loss':1.0 } ]\n");
    stream << QString("    \n");
    stream << QString("    with_match_loss: False\n");
    stream << QString("    with_focal_loss: True\n");
    stream << QString("    focal_loss_level: 0\n");
    stream << QString("    with_sol_loss: True\n");
    stream << QString("    match_sap_thresh: 5.0\n");
    stream << QString("\n");

    stream << QString("decode:\n");
    stream << QString("    score_thresh: 0.05\n");
    stream << QString("    len_thresh: 5\n");
    stream << QString("    top_k: 500\n");

    tYamlFile.close();
}
