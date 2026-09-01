#include "yolov8trainer.h"
#include "yolov8inference.h"

#pragma execution_character_set("utf-8")

#ifdef slots
#undef slots
#endif

#include "third_party/koba_jon/loss.hpp"
#include "third_party/koba_jon/networks.hpp"

#include <cuda_runtime_api.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <tuple>
#include <vector>

namespace visionaiflow::yolov8
{
namespace
{
constexpr int kInputChannels = 3;
constexpr int kRegMax = 16;
constexpr double kAdamBeta1 = 0.9;
constexpr double kAdamBeta2 = 0.999;
constexpr double kLambdaBox = 0.05;
constexpr double kLambdaObjectness = 1.0;
constexpr double kLambdaClass = 0.5;
constexpr double kLambdaDfl = 0.005;

struct Annotation final
{
    int classIndex{0};
    float centerX{0.0F};
    float centerY{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct Sample final
{
    QString imagePath;
    QVector<Annotation> annotations;
};

struct Batch final
{
    torch::Tensor images;
    std::vector<std::tuple<torch::Tensor, torch::Tensor>> annotations;
};

struct EpochLoss final
{
    double total{0.0};
    double box{0.0};
    double objectness{0.0};
    double classLoss{0.0};
    double dfl{0.0};
    double iouSum{0.0};
    int batches{0};
    int positiveCount{0};
};

bool IsNormalizedCoordinate(const float value)
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool ReadAnnotations(const QString &labelPath,
                     const int classCount,
                     QVector<Annotation> *annotations,
                     QString *errorMessage)
{
    QFile file(labelPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法读取 YOLO 标签文件 %1: %2").arg(labelPath, file.errorString());
        return false;
    }

    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd())
    {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        const QStringList values = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (values.size() != 5)
        {
            *errorMessage = QString(u8"YOLO 标签格式错误: %1 第 %2 行必须包含 5 个字段").arg(labelPath).arg(lineNumber);
            return false;
        }

        bool classOk = false;
        const int classIndex = values.at(0).toInt(&classOk);
        bool centerXOk = false;
        bool centerYOk = false;
        bool widthOk = false;
        bool heightOk = false;
        const float centerX = values.at(1).toFloat(&centerXOk);
        const float centerY = values.at(2).toFloat(&centerYOk);
        const float width = values.at(3).toFloat(&widthOk);
        const float height = values.at(4).toFloat(&heightOk);
        if (!classOk || classIndex < 0 || classIndex >= classCount || !centerXOk || !centerYOk || !widthOk ||
            !heightOk || !IsNormalizedCoordinate(centerX) || !IsNormalizedCoordinate(centerY) ||
            !IsNormalizedCoordinate(width) || !IsNormalizedCoordinate(height) || width <= 0.0F || height <= 0.0F)
        {
            *errorMessage = QString(u8"YOLO 标签数值非法: %1 第 %2 行").arg(labelPath).arg(lineNumber);
            return false;
        }
        annotations->append({classIndex, centerX, centerY, width, height});
    }
    return true;
}

bool ReadDataset(const QString &csvPath, const int classCount, std::vector<Sample> *samples, QString *errorMessage)
{
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *errorMessage = QString(u8"无法读取训练数据清单 %1: %2").arg(csvPath, file.errorString());
        return false;
    }

    QTextStream stream(&file);
    int lineNumber = 0;
    while (!stream.atEnd())
    {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        const int delimiter = line.indexOf(',');
        if (delimiter <= 0 || delimiter >= line.size() - 1)
        {
            *errorMessage =
                QString(u8"数据清单格式错误: %1 第 %2 行必须为 图像路径,标签路径").arg(csvPath).arg(lineNumber);
            return false;
        }

        Sample sample;
        sample.imagePath = line.left(delimiter).trimmed();
        const QString labelPath = line.mid(delimiter + 1).trimmed();
        if (!QFileInfo::exists(sample.imagePath))
        {
            *errorMessage = QString(u8"数据清单中的图像不存在: %1").arg(sample.imagePath);
            return false;
        }
        if (labelPath != QStringLiteral("SIGNAL_BACKGROUND_IMG") &&
            !ReadAnnotations(labelPath, classCount, &sample.annotations, errorMessage))
        {
            return false;
        }
        samples->push_back(std::move(sample));
    }

    if (samples->empty())
    {
        *errorMessage = QString(u8"数据清单为空: %1").arg(csvPath);
        return false;
    }
    return true;
}

bool LoadImageTensor(const QString &imagePath,
                     const int imageWidth,
                     const int imageHeight,
                     torch::Tensor *imageTensor,
                     QString *errorMessage)
{
    QImage image(imagePath);
    if (image.isNull())
    {
        *errorMessage = QString(u8"无法解码训练图像: %1").arg(imagePath);
        return false;
    }

    image = image.convertToFormat(QImage::Format_RGB888)
                .scaled(imageWidth, imageHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const int rowBytes = image.bytesPerLine();
    auto bytes = torch::from_blob(const_cast<uchar *>(image.constBits()),
                                  {imageHeight, rowBytes},
                                  torch::TensorOptions().dtype(torch::kUInt8))
                     .clone()
                     .narrow(1, 0, imageWidth * kInputChannels)
                     .view({imageHeight, imageWidth, kInputChannels});
    *imageTensor = bytes.permute({2, 0, 1}).to(torch::kFloat32).div(255.0);
    return true;
}

bool BuildBatch(const std::vector<Sample> &samples,
                const std::vector<size_t> &indices,
                const size_t begin,
                const size_t end,
                const int imageWidth,
                const int imageHeight,
                Batch *batch,
                QString *errorMessage)
{
    std::vector<torch::Tensor> images;
    images.reserve(end - begin);
    batch->annotations.clear();
    batch->annotations.reserve(end - begin);
    for (size_t item = begin; item < end; ++item)
    {
        const Sample &sample = samples.at(indices.at(item));
        torch::Tensor image;
        if (!LoadImageTensor(sample.imagePath, imageWidth, imageHeight, &image, errorMessage))
        {
            return false;
        }
        images.push_back(std::move(image));

        std::vector<int64_t> ids;
        std::vector<float> coordinates;
        ids.reserve(static_cast<size_t>(sample.annotations.size()));
        coordinates.reserve(static_cast<size_t>(sample.annotations.size()) * 4U);
        for (const Annotation &annotation : sample.annotations)
        {
            ids.push_back(annotation.classIndex);
            coordinates.push_back(annotation.centerX);
            coordinates.push_back(annotation.centerY);
            coordinates.push_back(annotation.width);
            coordinates.push_back(annotation.height);
        }

        torch::Tensor idsTensor =
            ids.empty() ? torch::empty({0}, torch::TensorOptions().dtype(torch::kInt64))
                        : torch::from_blob(ids.data(), {static_cast<int64_t>(ids.size())}, torch::kInt64).clone();
        torch::Tensor coordinatesTensor =
            coordinates.empty()
                ? torch::empty({0, 4}, torch::TensorOptions().dtype(torch::kFloat32))
                : torch::from_blob(coordinates.data(), {static_cast<int64_t>(ids.size()), 4}, torch::kFloat32).clone();
        batch->annotations.emplace_back(std::move(idsTensor), std::move(coordinatesTensor));
    }
    batch->images = torch::stack(images);
    return true;
}

torch::Tensor WeightedLoss(const std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor> &losses)
{
    return std::get<0>(losses) * kLambdaBox + std::get<1>(losses) * kLambdaObjectness +
           std::get<2>(losses) * kLambdaClass + std::get<3>(losses) * kLambdaDfl;
}

void AddLoss(EpochLoss *epochLoss,
             const std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor> &losses,
             const torch::Tensor &totalLoss,
             const Loss &criterion)
{
    epochLoss->total += totalLoss.item<double>();
    epochLoss->box += (std::get<0>(losses) * kLambdaBox).item<double>();
    epochLoss->objectness += (std::get<1>(losses) * kLambdaObjectness).item<double>();
    epochLoss->classLoss += (std::get<2>(losses) * kLambdaClass).item<double>();
    epochLoss->dfl += (std::get<3>(losses) * kLambdaDfl).item<double>();
    const int positiveCount = criterion.lastPositiveCount();
    epochLoss->positiveCount += positiveCount;
    epochLoss->iouSum += criterion.lastMeanIou() * static_cast<double>(positiveCount);
    ++epochLoss->batches;
}

bool SaveCheckpoint(const QString &checkpointPath,
                    YOLOv8 &model,
                    torch::optim::Optimizer &optimizer,
                    const int epoch,
                    const QString &modelVariant,
                    const plugin_api::DetectionTrainConfig &config,
                    const double validationLoss,
                    QString *errorMessage)
{
    try
    {
        torch::save(model, checkpointPath.toStdString());
        torch::save(optimizer, (checkpointPath + QStringLiteral(".optimizer")).toStdString());
    }
    catch (const c10::Error &error)
    {
        *errorMessage =
            QString(u8"无法保存 YOLOv8 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage =
            QString(u8"无法保存 YOLOv8 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("epoch"), epoch);
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
    metadata.insert(QStringLiteral("validation_loss"), validationLoss);
    QSaveFile metadataFile(checkpointPath + QStringLiteral(".json"));
    if (!metadataFile.open(QIODevice::WriteOnly))
    {
        *errorMessage =
            QString(u8"无法写入 checkpoint 元数据 %1: %2").arg(metadataFile.fileName(), metadataFile.errorString());
        return false;
    }
    const QByteArray data = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if (metadataFile.write(data) != data.size() || !metadataFile.commit())
    {
        *errorMessage =
            QString(u8"无法提交 checkpoint 元数据 %1: %2").arg(metadataFile.fileName(), metadataFile.errorString());
        return false;
    }
    return true;
}

bool LoadCheckpoint(const QString &checkpointPath,
                    YOLOv8 &model,
                    torch::optim::Optimizer &optimizer,
                    const QString &modelVariant,
                    const plugin_api::DetectionTrainConfig &config,
                    torch::Device device,
                    int *startEpoch,
                    QString *errorMessage)
{
    const QString optimizerPath = checkpointPath + QStringLiteral(".optimizer");
    const QString metadataPath = checkpointPath + QStringLiteral(".json");
    if (!QFileInfo::exists(checkpointPath) || !QFileInfo::exists(optimizerPath) || !QFileInfo::exists(metadataPath))
    {
        *errorMessage = QString(u8"恢复训练需要模型、优化器和元数据文件: %1").arg(checkpointPath);
        return false;
    }

    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        *errorMessage = QString(u8"无法读取 checkpoint 元数据 %1: %2").arg(metadataPath, metadataFile.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        *errorMessage = QString(u8"checkpoint 元数据不是有效 JSON: %1").arg(metadataPath);
        return false;
    }
    const QJsonObject metadata = document.object();
    if (metadata.value(QStringLiteral("model_variant")).toString() != modelVariant ||
        metadata.value(QStringLiteral("class_count")).toInt(-1) != config.classNames.size() ||
        metadata.value(QStringLiteral("image_width")).toInt(-1) != config.imageWidth ||
        metadata.value(QStringLiteral("image_height")).toInt(-1) != config.imageHeight)
    {
        *errorMessage = QString(u8"checkpoint 与当前 YOLOv8 模型规格、类别数或输入尺寸不匹配: %1").arg(checkpointPath);
        return false;
    }

    try
    {
        torch::load(model, checkpointPath.toStdString(), device);
        torch::load(optimizer, optimizerPath.toStdString(), device);
    }
    catch (const c10::Error &error)
    {
        *errorMessage =
            QString(u8"无法加载 YOLOv8 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage =
            QString(u8"无法加载 YOLOv8 checkpoint %1: %2").arg(checkpointPath, QString::fromLocal8Bit(error.what()));
        return false;
    }

    *startEpoch = metadata.value(QStringLiteral("epoch")).toInt();
    return true;
}

bool Validate(const std::vector<Sample> &samples,
              const plugin_api::DetectionTrainConfig &config,
              YOLOv8 &model,
              Loss &criterion,
              torch::Device device,
              double *validationLoss,
              int *positiveCount,
              double *meanIou,
              QString *errorMessage)
{
    model->eval();
    torch::NoGradGuard noGrad;
    EpochLoss values;
    std::vector<size_t> indices(samples.size());
    std::iota(indices.begin(), indices.end(), 0U);
    for (size_t begin = 0; begin < indices.size(); begin += static_cast<size_t>(config.batchSize))
    {
        const size_t end = std::min(begin + static_cast<size_t>(config.batchSize), indices.size());
        Batch batch;
        if (!BuildBatch(samples, indices, begin, end, config.imageWidth, config.imageHeight, &batch, errorMessage))
        {
            return false;
        }
        std::vector<torch::Tensor> output = model->forward(batch.images.to(device));
        const auto losses = criterion(output, batch.annotations);
        const torch::Tensor totalLoss = WeightedLoss(losses);
        if (!torch::isfinite(totalLoss).all().item<bool>())
        {
            *errorMessage = QString(u8"YOLOv8 验证损失出现 NaN 或 Inf");
            return false;
        }
        AddLoss(&values, losses, totalLoss, criterion);
    }
    *validationLoss = values.total / static_cast<double>(values.batches);
    *positiveCount = values.positiveCount;
    *meanIou = values.positiveCount > 0 ? values.iouSum / static_cast<double>(values.positiveCount) : 0.0;
    return true;
}
} // namespace

bool YoloV8Trainer::initialize(const plugin_api::DetectionTrainConfig &config, QString *errorMessage)
{
    if (!EnsureYoloV8TorchCudaBackend(errorMessage))
    {
        return false;
    }

    if (config.datasetPath.isEmpty() || !QFileInfo::exists(config.datasetPath))
    {
        *errorMessage = QString(u8"YOLOv8 训练数据清单不存在: %1").arg(config.datasetPath);
        return false;
    }
    if (config.classNames.isEmpty() || config.epochs <= 0 || config.batchSize <= 0 || config.imageWidth <= 0 ||
        config.imageHeight <= 0 || config.learningRate <= 0.0)
    {
        *errorMessage = QString(u8"YOLOv8 训练配置无效");
        return false;
    }
    if (config.imageWidth != 640 || config.imageHeight != 640)
    {
        *errorMessage = QString(u8"YOLOv8 训练插件当前仅支持 640x640 输入");
        return false;
    }

    const QString variant = config.algorithmOptions.value(QStringLiteral("model_variant")).toString();
    const QStringList variants{QStringLiteral("yolov8n"),
                               QStringLiteral("yolov8s"),
                               QStringLiteral("yolov8m"),
                               QStringLiteral("yolov8l"),
                               QStringLiteral("yolov8x")};
    if (!variants.contains(variant))
    {
        *errorMessage = QString(u8"YOLOv8 模型规格无效: %1").arg(variant);
        return false;
    }
    if (!config.pretrainedPath.isEmpty() && !config.resumeCheckpointPath.isEmpty())
    {
        *errorMessage = QString(u8"YOLOv8 训练不能同时指定预训练权重和恢复 checkpoint");
        return false;
    }
    if (!config.pretrainedPath.isEmpty())
    {
        int cudaDeviceCount = 0;
        const cudaError_t cudaStatus = cudaGetDeviceCount(&cudaDeviceCount);
        const int libTorchDeviceCount = torch::cuda::device_count();
        if (cudaStatus != cudaSuccess || config.gpuId < 0 || config.gpuId >= cudaDeviceCount ||
            config.gpuId >= libTorchDeviceCount)
        {
            *errorMessage =
                QString(u8"YOLOv8 无法使用 GPU %1 加载预训练权重, CUDA Runtime 设备数: %2, LibTorch 设备数: %3")
                    .arg(config.gpuId)
                    .arg(cudaDeviceCount)
                    .arg(libTorchDeviceCount);
            return false;
        }

        try
        {
            YoloV8NetworkConfig networkConfig;
            networkConfig.inputChannels = kInputChannels;
            networkConfig.classCount = static_cast<size_t>(config.classNames.size());
            networkConfig.regMax = kRegMax;
            networkConfig.variant = variant.toStdString();
            const torch::Device device(torch::kCUDA, config.gpuId);
            YOLOv8 model(networkConfig);
            model->to(device);
            torch::optim::Adam optimizer(
                model->parameters(),
                torch::optim::AdamOptions(config.learningRate).betas(std::make_tuple(kAdamBeta1, kAdamBeta2)));
            int pretrainedEpoch = 0;
            if (!LoadCheckpoint(config.pretrainedPath,
                                model,
                                optimizer,
                                variant,
                                config,
                                device,
                                &pretrainedEpoch,
                                errorMessage))
            {
                return false;
            }
        }
        catch (const c10::Error &error)
        {
            *errorMessage = QString(u8"YOLOv8 预训练权重加载失败: %1").arg(QString::fromLocal8Bit(error.what()));
            return false;
        }
        catch (const std::exception &error)
        {
            *errorMessage = QString(u8"YOLOv8 预训练权重校验失败: %1").arg(QString::fromLocal8Bit(error.what()));
            return false;
        }
    }

    m_validationDatasetPath = QDir(QFileInfo(config.datasetPath).absolutePath()).filePath(QStringLiteral("val.csv"));
    if (!QFileInfo::exists(m_validationDatasetPath))
    {
        *errorMessage = QString(u8"YOLOv8 验证数据清单不存在: %1").arg(m_validationDatasetPath);
        return false;
    }
    QDir outputDirectory(config.outputPath);
    if (!outputDirectory.exists() && !QDir().mkpath(config.outputPath))
    {
        *errorMessage = QString(u8"无法创建 YOLOv8 输出目录: %1").arg(config.outputPath);
        return false;
    }
    if (!outputDirectory.mkpath(QStringLiteral("weights")))
    {
        *errorMessage =
            QString(u8"无法创建 YOLOv8 checkpoint 目录: %1").arg(outputDirectory.filePath(QStringLiteral("weights")));
        return false;
    }

    m_config = config;
    m_modelVariant = variant;
    return true;
}

TrainRunResult
YoloV8Trainer::train(std::atomic_bool &stopRequested, const ProgressCallback &onProgress, QString *errorMessage)
{
    try
    {
        int cudaDeviceCount = 0;
        const cudaError_t cudaStatus = cudaGetDeviceCount(&cudaDeviceCount);
        if (cudaStatus != cudaSuccess)
        {
            *errorMessage =
                QString(u8"YOLOv8 无法初始化 CUDA: %1").arg(QString::fromLocal8Bit(cudaGetErrorString(cudaStatus)));
            return TrainRunResult::Failed;
        }
        if (cudaDeviceCount <= 0)
        {
            *errorMessage = QString(u8"YOLOv8 训练需要可用的 CUDA 设备");
            return TrainRunResult::Failed;
        }
        const int libTorchDeviceCount = torch::cuda::device_count();
        if (m_config.gpuId < 0 || m_config.gpuId >= libTorchDeviceCount)
        {
            *errorMessage =
                QString(
                    u8"YOLOv8 的 CUDA Runtime 检测到 %1 块 GPU，但 LibTorch 仅检测到 %2 块 GPU，无法使用 GPU 编号 %3")
                    .arg(cudaDeviceCount)
                    .arg(libTorchDeviceCount)
                    .arg(m_config.gpuId);
            return TrainRunResult::Failed;
        }

        std::vector<Sample> trainingSamples;
        std::vector<Sample> validationSamples;
        if (!ReadDataset(m_config.datasetPath, m_config.classNames.size(), &trainingSamples, errorMessage) ||
            !ReadDataset(m_validationDatasetPath, m_config.classNames.size(), &validationSamples, errorMessage))
        {
            return TrainRunResult::Failed;
        }

        torch::manual_seed(static_cast<uint64_t>(QRandomGenerator::global()->generate64()));
        torch::Device device(torch::kCUDA, m_config.gpuId);
        YoloV8NetworkConfig networkConfig;
        networkConfig.inputChannels = kInputChannels;
        networkConfig.classCount = static_cast<size_t>(m_config.classNames.size());
        networkConfig.regMax = kRegMax;
        networkConfig.variant = m_modelVariant.toStdString();
        YOLOv8 model(networkConfig);
        model->to(device);
        model->apply(weights_init);
        torch::optim::Adam optimizer(
            model->parameters(),
            torch::optim::AdamOptions(m_config.learningRate).betas(std::make_tuple(kAdamBeta1, kAdamBeta2)));
        Loss criterion(static_cast<long>(m_config.classNames.size()), kRegMax);

        int startEpoch = 0;
        const QString initialWeightsPath =
            m_config.resumeCheckpointPath.isEmpty() ? m_config.pretrainedPath : m_config.resumeCheckpointPath;
        if (!initialWeightsPath.isEmpty() && !LoadCheckpoint(initialWeightsPath,
                                                             model,
                                                             optimizer,
                                                             m_modelVariant,
                                                             m_config,
                                                             device,
                                                             &startEpoch,
                                                             errorMessage))
        {
            return TrainRunResult::Failed;
        }
        if (!m_config.pretrainedPath.isEmpty())
        {
            startEpoch = 0;
        }
        if (!m_config.resumeCheckpointPath.isEmpty() && startEpoch >= m_config.epochs)
        {
            *errorMessage = QString(u8"恢复 checkpoint 的轮次已达到或超过当前训练轮次: %1").arg(startEpoch);
            return TrainRunResult::Failed;
        }

        const int batchesPerEpoch =
            static_cast<int>((trainingSamples.size() + static_cast<size_t>(m_config.batchSize) - 1U) /
                             static_cast<size_t>(m_config.batchSize));
        const int totalSteps = batchesPerEpoch * (m_config.epochs - startEpoch);
        int globalStep = 0;
        double bestValidationLoss = std::numeric_limits<double>::infinity();
        const QString weightsDirectory = QDir(m_config.outputPath).filePath(QStringLiteral("weights"));
        const QString lastCheckpointPath = QDir(weightsDirectory).filePath(QStringLiteral("last.pt"));
        const QString bestCheckpointPath = QDir(weightsDirectory).filePath(QStringLiteral("best.pt"));

        for (int epoch = startEpoch + 1; epoch <= m_config.epochs; ++epoch)
        {
            model->train();
            EpochLoss trainingLoss;
            std::vector<size_t> indices(trainingSamples.size());
            std::iota(indices.begin(), indices.end(), 0U);
            std::mt19937 generator(QRandomGenerator::global()->generate());
            std::shuffle(indices.begin(), indices.end(), generator);

            for (size_t begin = 0; begin < indices.size(); begin += static_cast<size_t>(m_config.batchSize))
            {
                if (stopRequested.load())
                {
                    return TrainRunResult::Cancelled;
                }

                const size_t end = std::min(begin + static_cast<size_t>(m_config.batchSize), indices.size());
                Batch batch;
                if (!BuildBatch(trainingSamples,
                                indices,
                                begin,
                                end,
                                m_config.imageWidth,
                                m_config.imageHeight,
                                &batch,
                                errorMessage))
                {
                    return TrainRunResult::Failed;
                }

                std::vector<torch::Tensor> output = model->forward(batch.images.to(device));
                const auto losses = criterion(output, batch.annotations);
                const torch::Tensor totalLoss = WeightedLoss(losses);
                if (!torch::isfinite(totalLoss).all().item<bool>())
                {
                    *errorMessage =
                        QString(u8"YOLOv8 训练损失出现 NaN 或 Inf，轮次 %1，批次 %2").arg(epoch).arg(globalStep + 1);
                    return TrainRunResult::Failed;
                }

                optimizer.zero_grad();
                totalLoss.backward();
                optimizer.step();
                AddLoss(&trainingLoss, losses, totalLoss, criterion);
                ++globalStep;
            }

            if (stopRequested.load())
            {
                return TrainRunResult::Cancelled;
            }

            double validationLoss = 0.0;
            int validationPositiveCount = 0;
            double validationMeanIou = 0.0;
            if (!Validate(validationSamples,
                          m_config,
                          model,
                          criterion,
                          device,
                          &validationLoss,
                          &validationPositiveCount,
                          &validationMeanIou,
                          errorMessage))
            {
                return TrainRunResult::Failed;
            }
            if (!SaveCheckpoint(lastCheckpointPath,
                                model,
                                optimizer,
                                epoch,
                                m_modelVariant,
                                m_config,
                                validationLoss,
                                errorMessage))
            {
                return TrainRunResult::Failed;
            }
            if (validationLoss < bestValidationLoss)
            {
                if (!SaveCheckpoint(bestCheckpointPath,
                                    model,
                                    optimizer,
                                    epoch,
                                    m_modelVariant,
                                    m_config,
                                    validationLoss,
                                    errorMessage))
                {
                    return TrainRunResult::Failed;
                }
                bestValidationLoss = validationLoss;
            }

            plugin_api::DetectionTrainProgress progress;
            progress.train.epoch = epoch;
            progress.train.step = globalStep;
            progress.train.totalEpochs = m_config.epochs;
            progress.train.totalSteps = totalSteps;
            progress.train.loss = trainingLoss.total / static_cast<double>(trainingLoss.batches);
            progress.train.learningRate = m_config.learningRate;
            progress.train.message = QString(u8"训练损失: %1, 验证损失: %2")
                                         .arg(progress.train.loss, 0, 'f', 6)
                                         .arg(validationLoss, 0, 'f', 6);
            progress.boxLoss = trainingLoss.box / static_cast<double>(trainingLoss.batches);
            progress.classLoss = trainingLoss.classLoss / static_cast<double>(trainingLoss.batches);
            progress.dflLoss = trainingLoss.dfl / static_cast<double>(trainingLoss.batches);
            progress.positiveCount = validationPositiveCount;
            progress.meanIou = validationMeanIou;
            progress.modelPath = lastCheckpointPath;
            progress.bestCheckpointPath = bestCheckpointPath;
            onProgress(progress);
        }
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLOv8 LibTorch 运行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return TrainRunResult::Failed;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLOv8 训练运行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return TrainRunResult::Failed;
    }
    catch (...)
    {
        *errorMessage = QString(u8"YOLOv8 训练发生未知异常");
        return TrainRunResult::Failed;
    }
    return TrainRunResult::Completed;
}
} // namespace visionaiflow::yolov8
