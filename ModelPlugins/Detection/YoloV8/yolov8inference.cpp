#include "yolov8inference.h"

#include <cuda_runtime_api.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace visionaiflow::yolov8
{
namespace
{
constexpr int kInputChannels = 3;
constexpr int kRegMax = 16;

struct CheckpointMetadata final
{
    QString modelVariant;
    QStringList classNames;
    int imageWidth{0};
    int imageHeight{0};
};

struct LetterboxTransform final
{
    double scale{0.0};
    int paddingX{0};
    int paddingY{0};
    int originalWidth{0};
    int originalHeight{0};
};

struct Candidate final
{
    int classId{-1};
    double confidence{0.0};
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

bool IsSupportedVariant(const QString &variant)
{
    return variant == QStringLiteral("yolov8n") || variant == QStringLiteral("yolov8s") ||
           variant == QStringLiteral("yolov8m") || variant == QStringLiteral("yolov8l") ||
           variant == QStringLiteral("yolov8x");
}

bool CheckCudaStatus(const cudaError_t status, const QString &operation, QString *errorMessage)
{
    if (status == cudaSuccess)
    {
        return true;
    }

    *errorMessage =
        QString(u8"YOLOv8 CUDA 操作失败, %1: %2").arg(operation, QString::fromLocal8Bit(cudaGetErrorString(status)));
    return false;
}

bool ValidateCudaDevice(const int gpuId, torch::Device *device, QString *errorMessage)
{
    int cudaDeviceCount = 0;
    if (!CheckCudaStatus(cudaGetDeviceCount(&cudaDeviceCount), QString(u8"读取设备数量"), errorMessage))
    {
        return false;
    }
    if (cudaDeviceCount <= 0)
    {
        *errorMessage = QString(u8"YOLOv8 推理需要可用的 CUDA 设备");
        return false;
    }

    const int libTorchDeviceCount = torch::cuda::device_count();
    if (gpuId < 0 || gpuId >= cudaDeviceCount || gpuId >= libTorchDeviceCount)
    {
        *errorMessage = QString(u8"YOLOv8 GPU 编号无效: %1, CUDA Runtime 设备数: %2, LibTorch 设备数: %3")
                            .arg(gpuId)
                            .arg(cudaDeviceCount)
                            .arg(libTorchDeviceCount);
        return false;
    }

    if (!CheckCudaStatus(cudaSetDevice(gpuId), QString(u8"设置目标设备"), errorMessage))
    {
        return false;
    }

    *device = torch::Device(torch::kCUDA, gpuId);
    return true;
}

bool ReadCheckpointMetadata(const QString &modelPath, CheckpointMetadata *metadata, QString *errorMessage)
{
    const QString metadataPath = modelPath + QStringLiteral(".json");
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        *errorMessage =
            QString(u8"无法读取 YOLOv8 checkpoint 元数据 %1: %2").arg(metadataPath, metadataFile.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        *errorMessage = QString(u8"YOLOv8 checkpoint 元数据不是有效 JSON: %1").arg(metadataPath);
        return false;
    }

    const QJsonObject object = document.object();
    metadata->modelVariant = object.value(QStringLiteral("model_variant")).toString();
    metadata->imageWidth = object.value(QStringLiteral("image_width")).toInt();
    metadata->imageHeight = object.value(QStringLiteral("image_height")).toInt();
    const int classCount = object.value(QStringLiteral("class_count")).toInt(-1);
    const QJsonArray classNames = object.value(QStringLiteral("class_names")).toArray();
    if (!IsSupportedVariant(metadata->modelVariant) || metadata->imageWidth <= 0 || metadata->imageHeight <= 0 ||
        classCount <= 0 || classNames.size() != classCount)
    {
        *errorMessage = QString(u8"YOLOv8 checkpoint 元数据缺少有效模型规格、输入尺寸或类别定义: %1").arg(metadataPath);
        return false;
    }

    metadata->classNames.clear();
    metadata->classNames.reserve(classNames.size());
    for (const QJsonValue &value : classNames)
    {
        const QString className = value.toString();
        if (className.isEmpty())
        {
            *errorMessage = QString(u8"YOLOv8 checkpoint 元数据包含空类别名称: %1").arg(metadataPath);
            return false;
        }
        metadata->classNames.append(className);
    }
    return true;
}

bool ValidateOutputTensor(const torch::Tensor &output, const int classCount, QString *errorMessage)
{
    const int expectedChannels = 5 + classCount + 4 * kRegMax;
    if (!output.defined() || output.dim() != 4 || output.size(0) != 1 || output.size(1) <= 0 || output.size(2) <= 0 ||
        output.size(3) != expectedChannels)
    {
        *errorMessage =
            QString(u8"YOLOv8 输出张量形状无效, 期望 [1,H,W,%1], 实际维度: %2").arg(expectedChannels).arg(output.dim());
        return false;
    }
    if (!torch::isfinite(output).all().item<bool>())
    {
        *errorMessage = QString(u8"YOLOv8 输出张量包含 NaN 或 Inf");
        return false;
    }
    return true;
}

bool LetterboxImage(const QImage &source,
                    const int targetWidth,
                    const int targetHeight,
                    QImage *letterboxedImage,
                    LetterboxTransform *transform,
                    QString *errorMessage)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0)
    {
        *errorMessage = QString(u8"YOLOv8 推理图像无效");
        return false;
    }

    const double scale = std::min(static_cast<double>(targetWidth) / static_cast<double>(source.width()),
                                  static_cast<double>(targetHeight) / static_cast<double>(source.height()));
    const int resizedWidth = std::max(1, static_cast<int>(std::round(source.width() * scale)));
    const int resizedHeight = std::max(1, static_cast<int>(std::round(source.height() * scale)));
    const int paddingX = (targetWidth - resizedWidth) / 2;
    const int paddingY = (targetHeight - resizedHeight) / 2;

    QImage canvas(targetWidth, targetHeight, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    QPainter painter(&canvas);
    painter.drawImage(paddingX,
                      paddingY,
                      source.convertToFormat(QImage::Format_RGB888)
                          .scaled(resizedWidth, resizedHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();

    *letterboxedImage = std::move(canvas);
    transform->scale = scale;
    transform->paddingX = paddingX;
    transform->paddingY = paddingY;
    transform->originalWidth = source.width();
    transform->originalHeight = source.height();
    return true;
}

torch::Tensor CreateInputTensor(const QImage &image, const torch::Device &device, const bool useFp16)
{
    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    std::vector<float> values(static_cast<size_t>(kInputChannels) * rgbImage.width() * rgbImage.height());
    const size_t planeSize = static_cast<size_t>(rgbImage.width()) * rgbImage.height();
    for (int y = 0; y < rgbImage.height(); ++y)
    {
        const uchar *row = rgbImage.constScanLine(y);
        for (int x = 0; x < rgbImage.width(); ++x)
        {
            const size_t pixelIndex = static_cast<size_t>(y) * rgbImage.width() + x;
            const size_t sourceIndex = static_cast<size_t>(x) * kInputChannels;
            values[pixelIndex] = static_cast<float>(row[sourceIndex]) / 255.0F;
            values[planeSize + pixelIndex] = static_cast<float>(row[sourceIndex + 1]) / 255.0F;
            values[planeSize * 2U + pixelIndex] = static_cast<float>(row[sourceIndex + 2]) / 255.0F;
        }
    }

    torch::Tensor input = torch::from_blob(values.data(),
                                           {1, kInputChannels, rgbImage.height(), rgbImage.width()},
                                           torch::TensorOptions().dtype(torch::kFloat32))
                              .clone()
                              .to(device);
    if (useFp16)
    {
        input = input.to(torch::kFloat16);
    }
    return input;
}

double Sigmoid(const double value)
{
    if (value >= 0.0)
    {
        const double exponent = std::exp(-value);
        return 1.0 / (1.0 + exponent);
    }

    const double exponent = std::exp(value);
    return exponent / (1.0 + exponent);
}

double IntersectionOverUnion(const Candidate &first, const Candidate &second)
{
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.x + first.width, second.x + second.width);
    const double bottom = std::min(first.y + first.height, second.y + second.height);
    const double intersectionWidth = std::max(0.0, right - left);
    const double intersectionHeight = std::max(0.0, bottom - top);
    const double intersection = intersectionWidth * intersectionHeight;
    const double unionArea = first.width * first.height + second.width * second.height - intersection;
    return unionArea > 0.0 ? intersection / unionArea : 0.0;
}

QVector<Candidate> ApplyNms(QVector<Candidate> candidates, const double threshold)
{
    std::sort(candidates.begin(),
              candidates.end(),
              [](const Candidate &first, const Candidate &second)
              {
                  return first.confidence > second.confidence;
              });

    QVector<Candidate> results;
    for (const Candidate &candidate : candidates)
    {
        bool suppressed = false;
        for (const Candidate &kept : results)
        {
            if (candidate.classId == kept.classId && IntersectionOverUnion(candidate, kept) > threshold)
            {
                suppressed = true;
                break;
            }
        }
        if (!suppressed)
        {
            results.append(candidate);
        }
    }
    return results;
}
} // namespace

bool EnsureYoloV8TorchCudaBackend(QString *errorMessage)
{
    static QLibrary torchCudaLibrary;
    if (torchCudaLibrary.isLoaded())
    {
        return true;
    }

    const QString libraryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("torch_cuda.dll"));
    if (!QFileInfo::exists(libraryPath))
    {
        *errorMessage = QString(u8"YOLOv8 的 LibTorch CUDA 后端不存在: %1").arg(libraryPath);
        return false;
    }

    torchCudaLibrary.setFileName(libraryPath);
    torchCudaLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint);
    if (!torchCudaLibrary.load())
    {
        *errorMessage =
            QString(u8"无法加载 YOLOv8 的 LibTorch CUDA 后端 %1: %2").arg(libraryPath, torchCudaLibrary.errorString());
        return false;
    }
    return true;
}

bool YoloV8Inference::loadModel(const plugin_api::DetectionInferConfig &config, QString *errorMessage)
{
    if (config.modelPath.isEmpty() || !QFileInfo::exists(config.modelPath))
    {
        *errorMessage = QString(u8"YOLOv8 推理 checkpoint 不存在: %1").arg(config.modelPath);
        return false;
    }
    if (config.imageWidth <= 0 || config.imageHeight <= 0 || config.imageWidth % 32 != 0 ||
        config.imageHeight % 32 != 0)
    {
        *errorMessage = QString(u8"YOLOv8 推理输入尺寸必须为正数且能被 32 整除");
        return false;
    }
    if (!EnsureYoloV8TorchCudaBackend(errorMessage))
    {
        return false;
    }

    CheckpointMetadata metadata;
    if (!ReadCheckpointMetadata(config.modelPath, &metadata, errorMessage))
    {
        return false;
    }
    if (metadata.imageWidth != config.imageWidth || metadata.imageHeight != config.imageHeight)
    {
        *errorMessage = QString(u8"YOLOv8 checkpoint 输入尺寸与推理配置不匹配, checkpoint: %1x%2, 配置: %3x%4")
                            .arg(metadata.imageWidth)
                            .arg(metadata.imageHeight)
                            .arg(config.imageWidth)
                            .arg(config.imageHeight);
        return false;
    }

    torch::Device device(torch::kCPU);
    if (!ValidateCudaDevice(config.gpuId, &device, errorMessage))
    {
        return false;
    }

    try
    {
        YoloV8NetworkConfig networkConfig;
        networkConfig.inputChannels = kInputChannels;
        networkConfig.classCount = static_cast<size_t>(metadata.classNames.size());
        networkConfig.regMax = kRegMax;
        networkConfig.variant = metadata.modelVariant.toStdString();
        YOLOv8 model(networkConfig);
        model->to(device);
        torch::load(model, config.modelPath.toStdString(), device);
        for (const auto &parameter : model->named_parameters(true))
        {
            if (!parameter.value().defined() || parameter.value().sizes().empty() ||
                parameter.value().device() != device)
            {
                *errorMessage = QString(u8"YOLOv8 checkpoint 参数名称、形状或 CUDA 设备校验失败: %1")
                                    .arg(QString::fromStdString(parameter.key()));
                return false;
            }
        }
        if (config.useFp16)
        {
            model->to(torch::kFloat16);
        }
        else
        {
            model->to(torch::kFloat32);
        }
        model->eval();

        c10::InferenceMode inferenceMode;
        torch::Tensor warmupInput = torch::zeros(
            {1, kInputChannels, config.imageHeight, config.imageWidth},
            torch::TensorOptions().device(device).dtype(config.useFp16 ? torch::kFloat16 : torch::kFloat32));
        const std::vector<torch::Tensor> warmupOutput = model->forward(warmupInput);
        if (warmupOutput.size() != 3U)
        {
            *errorMessage = QString(u8"YOLOv8 预热输出数量无效, 期望 3, 实际 %1").arg(warmupOutput.size());
            return false;
        }
        for (const torch::Tensor &output : warmupOutput)
        {
            if (!ValidateOutputTensor(output, metadata.classNames.size(), errorMessage))
            {
                return false;
            }
        }
        if (!CheckCudaStatus(cudaDeviceSynchronize(), QString(u8"完成模型预热"), errorMessage))
        {
            return false;
        }

        m_model = std::move(model);
        m_device = device;
        m_classNames = metadata.classNames;
        m_imageWidth = config.imageWidth;
        m_imageHeight = config.imageHeight;
        m_useFp16 = config.useFp16;
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"无法加载 YOLOv8 checkpoint %1, 参数名称或形状可能不匹配: %2")
                            .arg(config.modelPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage =
            QString(u8"YOLOv8 模型加载失败 %1: %2").arg(config.modelPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
}

bool YoloV8Inference::infer(const plugin_api::DetectionInferRequest &request,
                            plugin_api::DetectionInferResult *result,
                            QString *errorMessage)
{
    if (result == nullptr)
    {
        *errorMessage = QString(u8"YOLOv8 推理结果输出指针为空");
        return false;
    }
    if (!m_model)
    {
        *errorMessage = QString(u8"YOLOv8 推理模型尚未加载");
        return false;
    }
    if (request.imagePath.isEmpty() || !QFileInfo::exists(request.imagePath))
    {
        *errorMessage = QString(u8"YOLOv8 推理图像不存在: %1").arg(request.imagePath);
        return false;
    }
    if (request.confidenceThreshold < 0.0 || request.confidenceThreshold > 1.0 || request.nmsThreshold < 0.0 ||
        request.nmsThreshold > 1.0)
    {
        *errorMessage = QString(u8"YOLOv8 推理置信度或 NMS 阈值无效");
        return false;
    }

    const QImage source(request.imagePath);
    QImage letterboxedImage;
    LetterboxTransform transform;
    if (!LetterboxImage(source, m_imageWidth, m_imageHeight, &letterboxedImage, &transform, errorMessage))
    {
        return false;
    }

    try
    {
        c10::InferenceMode inferenceMode;
        const torch::Tensor input = CreateInputTensor(letterboxedImage, m_device, m_useFp16);
        const std::vector<torch::Tensor> outputs = m_model->forward(input);
        if (outputs.size() != 3U)
        {
            *errorMessage = QString(u8"YOLOv8 推理输出数量无效, 期望 3, 实际 %1").arg(outputs.size());
            return false;
        }
        if (!CheckCudaStatus(cudaDeviceSynchronize(), QString(u8"完成模型前向"), errorMessage))
        {
            return false;
        }

        QVector<Candidate> candidates;
        for (const torch::Tensor &deviceOutput : outputs)
        {
            if (!ValidateOutputTensor(deviceOutput, m_classNames.size(), errorMessage))
            {
                return false;
            }

            const torch::Tensor output = deviceOutput.to(torch::kCPU, torch::kFloat32).contiguous();
            const auto accessor = output.accessor<float, 4>();
            const double strideX = static_cast<double>(m_imageWidth) / static_cast<double>(output.size(2));
            const double strideY = static_cast<double>(m_imageHeight) / static_cast<double>(output.size(1));
            for (int64_t y = 0; y < output.size(1); ++y)
            {
                for (int64_t x = 0; x < output.size(2); ++x)
                {
                    const double objectness = Sigmoid(accessor[0][y][x][4]);
                    if (objectness <= 0.0)
                    {
                        continue;
                    }

                    const double encodedCenterOffsetX = accessor[0][y][x][0];
                    const double encodedCenterOffsetY = accessor[0][y][x][1];
                    const double encodedWidth = accessor[0][y][x][2];
                    const double encodedHeight = accessor[0][y][x][3];
                    if (encodedCenterOffsetX < 0.0 || encodedCenterOffsetY < 0.0 || encodedWidth < 0.0 ||
                        encodedHeight < 0.0)
                    {
                        *errorMessage = QString(u8"YOLOv8 后处理得到无效边界框编码");
                        return false;
                    }

                    const double centerOffsetX =
                        encodedCenterOffsetX / static_cast<double>(kRegMax - 1) * 2.0 - 0.5;
                    const double centerOffsetY =
                        encodedCenterOffsetY / static_cast<double>(kRegMax - 1) * 2.0 - 0.5;
                    const double centerX = (static_cast<double>(x) + centerOffsetX) * strideX;
                    const double centerY = (static_cast<double>(y) + centerOffsetY) * strideY;
                    const double width = encodedWidth / static_cast<double>(kRegMax - 1) *
                                         static_cast<double>(output.size(2)) * strideX;
                    const double height = encodedHeight / static_cast<double>(kRegMax - 1) *
                                          static_cast<double>(output.size(1)) * strideY;
                    const double left = centerX - width * 0.5;
                    const double top = centerY - height * 0.5;
                    const double right = centerX + width * 0.5;
                    const double bottom = centerY + height * 0.5;
                    const double imageLeft = std::clamp((left - transform.paddingX) / transform.scale,
                                                        0.0,
                                                        static_cast<double>(transform.originalWidth));
                    const double imageTop = std::clamp((top - transform.paddingY) / transform.scale,
                                                       0.0,
                                                       static_cast<double>(transform.originalHeight));
                    const double imageRight = std::clamp((right - transform.paddingX) / transform.scale,
                                                         0.0,
                                                         static_cast<double>(transform.originalWidth));
                    const double imageBottom = std::clamp((bottom - transform.paddingY) / transform.scale,
                                                          0.0,
                                                          static_cast<double>(transform.originalHeight));
                    if (imageRight <= imageLeft || imageBottom <= imageTop)
                    {
                        continue;
                    }

                    for (int classId = 0; classId < m_classNames.size(); ++classId)
                    {
                        const double confidence = objectness * Sigmoid(accessor[0][y][x][5 + classId]);
                        if (confidence >= request.confidenceThreshold)
                        {
                            candidates.append({classId,
                                               confidence,
                                               imageLeft,
                                               imageTop,
                                               imageRight - imageLeft,
                                               imageBottom - imageTop});
                        }
                    }
                }
            }
        }

        result->imageWidth = transform.originalWidth;
        result->imageHeight = transform.originalHeight;
        result->boxes.clear();
        for (const Candidate &candidate : ApplyNms(std::move(candidates), request.nmsThreshold))
        {
            result->boxes.append({candidate.classId,
                                  m_classNames.at(candidate.classId),
                                  candidate.confidence,
                                  candidate.x,
                                  candidate.y,
                                  candidate.width,
                                  candidate.height});
        }
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLOv8 推理运行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLOv8 推理后处理失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}
} // namespace visionaiflow::yolov8
