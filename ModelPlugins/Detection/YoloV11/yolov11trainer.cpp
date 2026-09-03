#include "yolov11trainer.h"

#include "yolov11inference.h"
#include "yolov11loss.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

#include <torch/cuda.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace visionaiflow::yolov11
{
namespace
{
struct TrainingSample final
{
    QString imagePath;
    QString labelPath;
};

bool ParseCsvLine(const QString &line, TrainingSample *sample)
{
    const QStringList fields = line.split(',', Qt::KeepEmptyParts);
    if (fields.size() < 2)
    {
        return false;
    }
    sample->imagePath = fields.at(0).trimmed().remove('"');
    sample->labelPath = fields.at(1).trimmed().remove('"');
    return !sample->imagePath.isEmpty() && !sample->labelPath.isEmpty();
}

bool ReadSamples(const QString &path, QVector<TrainingSample> *samples, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法读取 YOLO11 数据索引: %1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    samples->clear();
    while (!stream.atEnd())
    {
        TrainingSample sample;
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
        {
            continue;
        }
        if (!ParseCsvLine(line, &sample))
        {
            *errorMessage = QString(u8"YOLO11 数据索引行必须包含图像和标签路径: %1").arg(line);
            return false;
        }
        if (!QFileInfo::exists(sample.imagePath) || !QFileInfo::exists(sample.labelPath))
        {
            *errorMessage =
                QString(u8"YOLO11 数据索引引用的图像或标签不存在: %1, %2").arg(sample.imagePath, sample.labelPath);
            return false;
        }
        samples->append(sample);
    }
    if (samples->isEmpty())
    {
        *errorMessage = QString(u8"YOLO11 数据索引不包含可训练样本: %1").arg(path);
        return false;
    }
    return true;
}

bool ReadLabels(const QString &path,
                const int classCount,
                std::vector<std::array<float, 5>> *labels,
                QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法读取 YOLO11 标签文件: %1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    labels->clear();
    while (!stream.atEnd())
    {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
        {
            continue;
        }
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (fields.size() != 5)
        {
            *errorMessage = QString(u8"YOLO11 标签必须是 class x y width height: %1").arg(path);
            return false;
        }
        bool converted = false;
        const int classId = fields.at(0).toInt(&converted);
        if (!converted || classId < 0 || classId >= classCount)
        {
            *errorMessage = QString(u8"YOLO11 标签类别超出范围: %1").arg(path);
            return false;
        }
        std::array<float, 5> label{static_cast<float>(classId), 0.0F, 0.0F, 0.0F, 0.0F};
        for (int index = 1; index < 5; ++index)
        {
            label[index] = fields.at(index).toFloat(&converted);
            if (!converted || label[index] < 0.0F || label[index] > 1.0F)
            {
                *errorMessage = QString(u8"YOLO11 标签坐标必须在 0 到 1 范围内: %1").arg(path);
                return false;
            }
        }
        labels->push_back(label);
    }
    return true;
}

bool ImageToTensor(const QString &path, const int width, const int height, torch::Tensor *tensor, QString *errorMessage)
{
    const QImage source(path);
    if (source.isNull())
    {
        *errorMessage = QString(u8"无法加载 YOLO11 训练图像: %1").arg(path);
        return false;
    }
    const QImage image = source.convertToFormat(QImage::Format_RGB888)
                             .scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::vector<float> values(static_cast<size_t>(3) * width * height);
    const size_t plane = static_cast<size_t>(width) * height;
    for (int y = 0; y < height; ++y)
    {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            const size_t index = static_cast<size_t>(y) * width + x;
            values[index] = static_cast<float>(line[3 * x]) / 255.0F;
            values[plane + index] = static_cast<float>(line[3 * x + 1]) / 255.0F;
            values[plane * 2 + index] = static_cast<float>(line[3 * x + 2]) / 255.0F;
        }
    }
    *tensor =
        torch::from_blob(values.data(), {3, height, width}, torch::TensorOptions().dtype(torch::kFloat32)).clone();
    return true;
}

struct TransferStatistics final
{
    int copiedItems{0};
    int totalItems{0};
};

bool CopyMatchingParameters(const torch::jit::script::Module &source,
                            Yolo11NetworkImpl &target,
                            TransferStatistics *statistics,
                            QString *errorMessage)
{
    if (statistics == nullptr)
    {
        *errorMessage = QString(u8"YOLO11 预训练权重迁移统计输出为空");
        return false;
    }

    torch::NoGradGuard noGrad;
    QHash<QString, torch::Tensor> values;
    for (const auto &parameter : source.named_parameters(true))
    {
        values.insert(QString::fromStdString(parameter.name), parameter.value);
    }
    for (const auto &buffer : source.named_buffers(true))
    {
        values.insert(QString::fromStdString(buffer.name), buffer.value);
    }
    statistics->copiedItems = 0;
    statistics->totalItems = 0;
    for (const auto &parameter : target.named_parameters(true))
    {
        ++statistics->totalItems;
        const auto iterator = values.constFind(QString::fromStdString(parameter.key()));
        if (iterator == values.constEnd() || iterator->sizes() != parameter.value().sizes())
        {
            continue;
        }
        parameter.value().copy_(iterator.value().to(parameter.value().device()));
        ++statistics->copiedItems;
    }
    for (const auto &buffer : target.named_buffers(true))
    {
        ++statistics->totalItems;
        const auto iterator = values.constFind(QString::fromStdString(buffer.key()));
        if (iterator == values.constEnd() || iterator->sizes() != buffer.value().sizes())
        {
            continue;
        }
        buffer.value().copy_(iterator.value().to(buffer.value().device()));
        ++statistics->copiedItems;
    }
    return true;
}

bool SaveCheckpoint(const QString &checkpointPath,
                    Yolo11Network &model,
                    const int epoch,
                    const QString &modelVariant,
                    const plugin_api::DetectionTrainConfig &config,
                    const double loss,
                    QString *errorMessage)
{
    try
    {
        torch::save(model, checkpointPath.toStdString());
    }
    catch (const c10::Error &error)
    {
        *errorMessage =
            QString(u8"无法保存 YOLO11 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage =
            QString(u8"无法保存 YOLO11 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("format"), QStringLiteral("VisionAIFlow.YoloV11.Checkpoint.1"));
    metadata.insert(QStringLiteral("ultralytics_version"), QStringLiteral("8.3.4"));
    metadata.insert(QStringLiteral("model_variant"), modelVariant);
    metadata.insert(QStringLiteral("class_count"), config.classNames.size());
    QJsonArray classNames;
    for (const QString &className : config.classNames)
    {
        classNames.append(className);
    }
    metadata.insert(QStringLiteral("class_names"), classNames);
    metadata.insert(QStringLiteral("image_width"), config.imageWidth);
    metadata.insert(QStringLiteral("image_height"), config.imageHeight);
    metadata.insert(QStringLiteral("reg_max"), 16);
    metadata.insert(QStringLiteral("strides"), QJsonArray{8, 16, 32});
    metadata.insert(QStringLiteral("epoch"), epoch);
    metadata.insert(QStringLiteral("training_loss"), loss);

    QSaveFile metadataFile(checkpointPath + QStringLiteral(".json"));
    if (!metadataFile.open(QIODevice::WriteOnly))
    {
        *errorMessage = QString(u8"无法写入 YOLO11 checkpoint 元数据 %1: %2")
                            .arg(metadataFile.fileName(), metadataFile.errorString());
        return false;
    }
    const QByteArray content = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if (metadataFile.write(content) != content.size() || !metadataFile.commit())
    {
        *errorMessage = QString(u8"无法提交 YOLO11 checkpoint 元数据 %1: %2")
                            .arg(metadataFile.fileName(), metadataFile.errorString());
        return false;
    }
    return true;
}
} // namespace

bool Yolo11Trainer::initialize(const plugin_api::DetectionTrainConfig &config, QString *errorMessage)
{
    if (!EnsureYolo11TorchCudaBackend(errorMessage))
    {
        return false;
    }
    if (config.classNames.isEmpty() || config.epochs <= 0 || config.batchSize <= 0 || config.imageWidth <= 0 ||
        config.imageHeight <= 0 || config.imageWidth % 32 != 0 || config.imageHeight % 32 != 0)
    {
        *errorMessage = QString(u8"YOLO11 训练参数无效");
        return false;
    }
    if (config.datasetPath.isEmpty() || !QFileInfo::exists(config.datasetPath) || config.pretrainedPath.isEmpty() ||
        !QFileInfo::exists(config.pretrainedPath))
    {
        *errorMessage = QString(u8"YOLO11 训练需要有效的数据索引和转换后的预训练权重");
        return false;
    }
    m_modelVariant =
        config.algorithmOptions.value(QStringLiteral("model_variant"), QStringLiteral("yolo11n")).toString();
    if (m_modelVariant != QStringLiteral("yolo11n"))
    {
        *errorMessage = QString(u8"YOLO11 插件当前仅支持 yolo11n");
        return false;
    }
    m_config = config;
    return true;
}

TrainRunResult
Yolo11Trainer::train(std::atomic_bool &stopRequested, const ProgressCallback &onProgress, QString *errorMessage)
{
    QVector<TrainingSample> samples;
    if (!ReadSamples(m_config.datasetPath, &samples, errorMessage))
    {
        return TrainRunResult::Failed;
    }
    torch::Device device(torch::kCPU);
    if (!InitializeYolo11CudaDevice(m_config.gpuId, &device, errorMessage))
    {
        return TrainRunResult::Failed;
    }
    try
    {
        torch::jit::ExtraFilesMap extraFiles;
        extraFiles["visionaiflow_yolo11.json"] = "";
        const torch::jit::script::Module pretrainedModel =
            torch::jit::load(m_config.pretrainedPath.toStdString(), torch::Device(torch::kCPU), extraFiles);
        const QJsonDocument metadata =
            QJsonDocument::fromJson(QByteArray::fromStdString(extraFiles["visionaiflow_yolo11.json"]));
        if (!metadata.isObject() ||
            metadata.object().value(QStringLiteral("format")).toString() !=
                QStringLiteral("VisionAIFlow.YoloV11.Pretrained.1") ||
            metadata.object().value(QStringLiteral("ultralytics_version")).toString() != QStringLiteral("8.3.4") ||
            metadata.object().value(QStringLiteral("model_variant")).toString() != QStringLiteral("yolo11n") ||
            metadata.object().value(QStringLiteral("input_channels")).toInt() != 3 ||
            metadata.object().value(QStringLiteral("source_nc")).toInt() != 80 ||
            metadata.object().value(QStringLiteral("state_dict_items")).toInt() != 499)
        {
            *errorMessage =
                QString(u8"YOLO11 预训练权重不是 Ultralytics 8.3.4 yolo11n 的参数载体, 请重新转换原始 COCO yolo11n.pt");
            return TrainRunResult::Failed;
        }
        Yolo11Network model(m_config.classNames.size());
        model->to(device);
        TransferStatistics transfer;
        if (!CopyMatchingParameters(pretrainedModel, *model, &transfer, errorMessage))
        {
            return TrainRunResult::Failed;
        }
        qInfo().noquote() << QStringLiteral("YOLO11 从 COCO 预训练权重迁移 %1/%2 项")
                                 .arg(transfer.copiedItems)
                                 .arg(transfer.totalItems);
        model->train();
        Yolo11DetectionLoss lossFunction(m_config.classNames.size());
        const double learningRate = m_config.learningRate > 0.0 ? m_config.learningRate : 0.001667;
        torch::optim::AdamW optimizer(
            model->parameters(),
            torch::optim::AdamWOptions(learningRate).betas({0.9, 0.999}).weight_decay(0.0005));
        const int totalSteps = static_cast<int>((samples.size() + m_config.batchSize - 1) / m_config.batchSize);
        double bestLoss = std::numeric_limits<double>::infinity();
        const QString weightsDirectory = QDir(m_config.outputPath).filePath(QStringLiteral("weights"));
        if (!QDir().mkpath(weightsDirectory))
        {
            *errorMessage = QString(u8"无法创建 YOLO11 权重输出目录: %1").arg(weightsDirectory);
            return TrainRunResult::Failed;
        }
        const QString bestPath = QDir(weightsDirectory).filePath(QStringLiteral("best.pt"));
        const QString lastPath = QDir(weightsDirectory).filePath(QStringLiteral("last.pt"));
        double lastLoss = std::numeric_limits<double>::infinity();
        for (int epoch = 0; epoch < m_config.epochs; ++epoch)
        {
            for (int step = 0; step < totalSteps; ++step)
            {
                if (stopRequested.load())
                {
                    return TrainRunResult::Cancelled;
                }
                const int begin = step * m_config.batchSize;
                const int end = std::min(begin + m_config.batchSize, static_cast<int>(samples.size()));
                std::vector<torch::Tensor> images;
                std::vector<std::array<float, 6>> targetRows;
                for (int index = begin; index < end; ++index)
                {
                    torch::Tensor image;
                    if (!ImageToTensor(samples.at(index).imagePath,
                                       m_config.imageWidth,
                                       m_config.imageHeight,
                                       &image,
                                       errorMessage))
                    {
                        return TrainRunResult::Failed;
                    }
                    images.push_back(image);
                    std::vector<std::array<float, 5>> labels;
                    if (!ReadLabels(samples.at(index).labelPath, m_config.classNames.size(), &labels, errorMessage))
                    {
                        return TrainRunResult::Failed;
                    }
                    for (const std::array<float, 5> &label : labels)
                    {
                        targetRows.push_back(
                            {static_cast<float>(index - begin), label[0], label[1], label[2], label[3], label[4]});
                    }
                }
                torch::Tensor target = torch::zeros({0, 6}, torch::TensorOptions().dtype(torch::kFloat32));
                if (!targetRows.empty())
                {
                    target = torch::from_blob(targetRows.data(),
                                              {static_cast<int64_t>(targetRows.size()), 6},
                                              torch::TensorOptions().dtype(torch::kFloat32))
                                 .clone();
                }
                optimizer.zero_grad();
                const std::vector<torch::Tensor> output = model->forward(torch::stack(images).to(device));
                const Yolo11LossResult loss = lossFunction(
                    output,
                    target.to(device),
                    torch::tensor({static_cast<float>(m_config.imageWidth), static_cast<float>(m_config.imageHeight)},
                                  torch::TensorOptions().device(device)));
                loss.total.backward();
                optimizer.step();
                const double currentLoss = loss.total.item<double>();
                lastLoss = currentLoss;
                plugin_api::DetectionTrainProgress progress;
                progress.train.epoch = epoch + 1;
                progress.train.step = step + 1;
                progress.train.totalEpochs = m_config.epochs;
                progress.train.totalSteps = totalSteps;
                progress.train.loss = currentLoss;
                progress.train.learningRate = learningRate;
                progress.train.message = QStringLiteral("running");
                progress.boxLoss = loss.items[0].item<double>();
                progress.classLoss = loss.items[1].item<double>();
                progress.dflLoss = loss.items[2].item<double>();
                progress.positiveCount = static_cast<int>(loss.positiveAnchorCount);
                progress.meanIou = loss.meanIou;
                progress.modelPath = lastPath;
                progress.bestCheckpointPath = bestPath;
                onProgress(progress);
                if (currentLoss < bestLoss)
                {
                    bestLoss = currentLoss;
                    if (!SaveCheckpoint(bestPath, model, epoch + 1, m_modelVariant, m_config, bestLoss, errorMessage))
                    {
                        return TrainRunResult::Failed;
                    }
                }
            }
            if (!SaveCheckpoint(lastPath, model, epoch + 1, m_modelVariant, m_config, lastLoss, errorMessage))
            {
                return TrainRunResult::Failed;
            }
        }
        return TrainRunResult::Completed;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLO11 训练执行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return TrainRunResult::Failed;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLO11 训练执行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return TrainRunResult::Failed;
    }
}
} // namespace visionaiflow::yolov11
