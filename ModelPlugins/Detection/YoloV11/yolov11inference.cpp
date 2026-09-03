#include "yolov11inference.h"

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

#include <cuda_runtime_api.h>
#include <torch/cuda.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace visionaiflow::yolov11
{
namespace
{
constexpr int kInputChannels = 3;
constexpr int kRegMax = 16;
constexpr std::array<int, 3> kStrides{8, 16, 32};

struct LetterboxTransform final
{
    double scale{1.0};
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

bool CheckCudaStatus(const cudaError_t status, const QString &operation, QString *errorMessage)
{
    if (status == cudaSuccess)
    {
        return true;
    }

    *errorMessage =
        QString(u8"YOLO11 CUDA 操作失败, %1: %2").arg(operation, QString::fromLocal8Bit(cudaGetErrorString(status)));
    return false;
}

bool ReadCheckpointMetadata(const QString &checkpointPath, Yolo11CheckpointMetadata *metadata, QString *errorMessage)
{
    const QString metadataPath = checkpointPath + QStringLiteral(".json");
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        *errorMessage =
            QString(u8"YOLO11 checkpoint 缺少或无法读取元数据 %1: %2").arg(metadataPath, metadataFile.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        *errorMessage = QString(u8"YOLO11 checkpoint 元数据不是有效 JSON: %1").arg(metadataPath);
        return false;
    }

    const QJsonObject object = document.object();
    const QJsonArray classNames = object.value(QStringLiteral("class_names")).toArray();
    const QJsonArray strides = object.value(QStringLiteral("strides")).toArray();
    const int classCount = object.value(QStringLiteral("class_count")).toInt(-1);
    if (object.value(QStringLiteral("format")).toString() != QStringLiteral("VisionAIFlow.YoloV11.Checkpoint.1") ||
        object.value(QStringLiteral("ultralytics_version")).toString() != QStringLiteral("8.3.4") ||
        object.value(QStringLiteral("model_variant")).toString() != QStringLiteral("yolo11n") || classCount <= 0 ||
        classNames.size() != classCount || object.value(QStringLiteral("image_width")).toInt() <= 0 ||
        object.value(QStringLiteral("image_height")).toInt() <= 0 ||
        object.value(QStringLiteral("reg_max")).toInt() != kRegMax ||
        strides.size() != static_cast<int>(kStrides.size()))
    {
        *errorMessage = QString(u8"YOLO11 checkpoint 格式不匹配, 请使用修复后的 YOLO11 训练重新生成 best.pt");
        return false;
    }

    for (int index = 0; index < strides.size(); ++index)
    {
        if (strides.at(index).toInt() != kStrides[static_cast<size_t>(index)])
        {
            *errorMessage = QString(u8"YOLO11 checkpoint 步长元数据不匹配: %1").arg(metadataPath);
            return false;
        }
    }

    metadata->classNames.clear();
    metadata->classNames.reserve(classCount);
    for (const QJsonValue &value : classNames)
    {
        const QString className = value.toString();
        if (className.isEmpty())
        {
            *errorMessage = QString(u8"YOLO11 checkpoint 类别元数据不完整: %1").arg(metadataPath);
            return false;
        }
        metadata->classNames.append(className);
    }
    metadata->imageWidth = object.value(QStringLiteral("image_width")).toInt();
    metadata->imageHeight = object.value(QStringLiteral("image_height")).toInt();
    return true;
}

bool ValidateRawOutput(const torch::Tensor &output,
                       const int classCount,
                       const int expectedHeight,
                       const int expectedWidth,
                       const torch::Device &device,
                       QString *errorMessage)
{
    const int expectedChannels = kRegMax * 4 + classCount;
    if (!output.defined() || output.dim() != 4 || output.size(0) != 1 || output.size(1) != expectedChannels ||
        output.size(2) != expectedHeight || output.size(3) != expectedWidth || output.device() != device)
    {
        *errorMessage = QString(u8"YOLO11 原始输出形状或 CUDA 设备无效, 期望 [1,%1,%2,%3]")
                            .arg(expectedChannels)
                            .arg(expectedHeight)
                            .arg(expectedWidth);
        return false;
    }
    if (!torch::isfinite(output).all().item<bool>())
    {
        *errorMessage = QString(u8"YOLO11 原始输出包含 NaN 或 Inf");
        return false;
    }
    return true;
}

bool Letterbox(const QImage &source,
               const int targetWidth,
               const int targetHeight,
               QImage *output,
               LetterboxTransform *transform,
               QString *errorMessage)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0)
    {
        *errorMessage = QString(u8"YOLO11 推理图像无效");
        return false;
    }
    transform->scale = std::min(static_cast<double>(targetWidth) / source.width(),
                                static_cast<double>(targetHeight) / source.height());
    const int width = std::max(1, static_cast<int>(std::round(source.width() * transform->scale)));
    const int height = std::max(1, static_cast<int>(std::round(source.height() * transform->scale)));
    transform->paddingX = (targetWidth - width) / 2;
    transform->paddingY = (targetHeight - height) / 2;
    transform->originalWidth = source.width();
    transform->originalHeight = source.height();
    QImage canvas(targetWidth, targetHeight, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    QPainter painter(&canvas);
    painter.drawImage(transform->paddingX,
                      transform->paddingY,
                      source.convertToFormat(QImage::Format_RGB888)
                          .scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();
    *output = std::move(canvas);
    return true;
}

torch::Tensor ToTensor(const QImage &image, const torch::Device &device)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const int width = rgb.width();
    const int height = rgb.height();
    std::vector<float> data(static_cast<size_t>(kInputChannels) * width * height);
    const size_t plane = static_cast<size_t>(width) * height;
    for (int y = 0; y < height; ++y)
    {
        const uchar *line = rgb.constScanLine(y);
        for (int x = 0; x < width; ++x)
        {
            const size_t index = static_cast<size_t>(y) * width + x;
            data[index] = static_cast<float>(line[3 * x]) / 255.0F;
            data[plane + index] = static_cast<float>(line[3 * x + 1]) / 255.0F;
            data[2 * plane + index] = static_cast<float>(line[3 * x + 2]) / 255.0F;
        }
    }
    return torch::from_blob(data.data(),
                            {1, kInputChannels, height, width},
                            torch::TensorOptions().dtype(torch::kFloat32))
        .clone()
        .to(device);
}

bool DecodePredictions(const std::vector<torch::Tensor> &outputs,
                       const int classCount,
                       const int imageWidth,
                       const int imageHeight,
                       const torch::Device &device,
                       torch::Tensor *prediction,
                       QString *errorMessage)
{
    if (outputs.size() != kStrides.size())
    {
        *errorMessage = QString(u8"YOLO11 输出数量无效, 期望 3, 实际 %1").arg(outputs.size());
        return false;
    }

    const int outputChannels = kRegMax * 4 + classCount;
    std::vector<torch::Tensor> flattened;
    std::vector<float> anchorValues;
    std::vector<float> strideValues;
    flattened.reserve(outputs.size());
    for (size_t level = 0; level < outputs.size(); ++level)
    {
        const int stride = kStrides[level];
        const int expectedHeight = imageHeight / stride;
        const int expectedWidth = imageWidth / stride;
        if (!ValidateRawOutput(outputs[level], classCount, expectedHeight, expectedWidth, device, errorMessage))
        {
            return false;
        }
        flattened.push_back(outputs[level].view({1, outputChannels, -1}));
        for (int y = 0; y < expectedHeight; ++y)
        {
            for (int x = 0; x < expectedWidth; ++x)
            {
                anchorValues.push_back(static_cast<float>(x) + 0.5F);
                anchorValues.push_back(static_cast<float>(y) + 0.5F);
                strideValues.push_back(static_cast<float>(stride));
            }
        }
    }

    const int64_t anchorCount = static_cast<int64_t>(strideValues.size());
    const torch::Tensor values = torch::cat(flattened, 2);
    const torch::Tensor box = values.slice(1, 0, kRegMax * 4);
    const torch::Tensor scores = values.slice(1, kRegMax * 4);
    const torch::Tensor anchorPoints =
        torch::from_blob(anchorValues.data(), {anchorCount, 2}, torch::TensorOptions().dtype(torch::kFloat32))
            .clone()
            .to(device)
            .transpose(0, 1)
            .unsqueeze(0);
    const torch::Tensor strides =
        torch::from_blob(strideValues.data(), {1, 1, anchorCount}, torch::TensorOptions().dtype(torch::kFloat32))
            .clone()
            .to(device);
    const torch::Tensor projection =
        torch::arange(kRegMax, torch::TensorOptions().device(device).dtype(torch::kFloat32)).view({1, kRegMax, 1, 1});
    const torch::Tensor distance =
        (box.view({1, 4, kRegMax, anchorCount}).transpose(1, 2).softmax(1) * projection).sum(1);
    const torch::Tensor leftTop = distance.slice(1, 0, 2);
    const torch::Tensor rightBottom = distance.slice(1, 2, 4);
    const torch::Tensor topLeft = anchorPoints - leftTop;
    const torch::Tensor bottomRight = anchorPoints + rightBottom;
    const torch::Tensor boxes = torch::cat({(topLeft + bottomRight) / 2.0, bottomRight - topLeft}, 1) * strides;
    *prediction = torch::cat({boxes, scores.sigmoid()}, 1);
    if (!torch::isfinite(*prediction).all().item<bool>())
    {
        *errorMessage = QString(u8"YOLO11 解码结果包含 NaN 或 Inf");
        return false;
    }
    return true;
}

double IoU(const Candidate &first, const Candidate &second)
{
    const double left = std::max(first.x, second.x);
    const double top = std::max(first.y, second.y);
    const double right = std::min(first.x + first.width, second.x + second.width);
    const double bottom = std::min(first.y + first.height, second.y + second.height);
    const double intersection = std::max(0.0, right - left) * std::max(0.0, bottom - top);
    const double unionArea = first.width * first.height + second.width * second.height - intersection;
    return unionArea > 0.0 ? intersection / unionArea : 0.0;
}
} // namespace

bool ReadYolo11CheckpointMetadata(const QString &checkpointPath,
                                  Yolo11CheckpointMetadata *metadata,
                                  QString *errorMessage)
{
    return ReadCheckpointMetadata(checkpointPath, metadata, errorMessage);
}

bool EnsureYolo11TorchCudaBackend(QString *errorMessage)
{
    static QLibrary torchCudaLibrary;
    if (torchCudaLibrary.isLoaded())
    {
        return true;
    }

    const QString libraryPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("torch_cuda.dll"));
    if (!QFileInfo::exists(libraryPath))
    {
        *errorMessage = QString(u8"YOLO11 的 LibTorch CUDA 后端不存在: %1").arg(libraryPath);
        return false;
    }

    torchCudaLibrary.setFileName(libraryPath);
    torchCudaLibrary.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint);
    if (!torchCudaLibrary.load())
    {
        *errorMessage =
            QString(u8"无法加载 YOLO11 的 LibTorch CUDA 后端 %1: %2").arg(libraryPath, torchCudaLibrary.errorString());
        return false;
    }
    return true;
}

bool InitializeYolo11CudaDevice(const int gpuId, torch::Device *device, QString *errorMessage)
{
    if (!EnsureYolo11TorchCudaBackend(errorMessage))
    {
        return false;
    }

    int cudaDeviceCount = 0;
    if (!CheckCudaStatus(cudaGetDeviceCount(&cudaDeviceCount), QString(u8"读取设备数量"), errorMessage))
    {
        return false;
    }
    if (cudaDeviceCount <= 0)
    {
        *errorMessage = QString(u8"YOLO11 需要有效的 CUDA GPU");
        return false;
    }

    const int libTorchDeviceCount = torch::cuda::device_count();
    if (gpuId < 0 || gpuId >= cudaDeviceCount || gpuId >= libTorchDeviceCount)
    {
        *errorMessage = QString(u8"YOLO11 GPU 编号无效: %1, CUDA Runtime 设备数: %2, LibTorch 设备数: %3")
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

bool Yolo11Inference::loadModel(const plugin_api::DetectionInferConfig &config, QString *errorMessage)
{
    if (config.modelPath.isEmpty() || !QFileInfo::exists(config.modelPath))
    {
        *errorMessage = QString(u8"YOLO11 推理权重不存在: %1").arg(config.modelPath);
        return false;
    }
    if (config.imageWidth <= 0 || config.imageHeight <= 0 || config.imageWidth % 32 != 0 ||
        config.imageHeight % 32 != 0)
    {
        *errorMessage = QString(u8"YOLO11 推理输入尺寸必须为正数且能被 32 整除");
        return false;
    }

    Yolo11CheckpointMetadata metadata;
    if (!ReadYolo11CheckpointMetadata(config.modelPath, &metadata, errorMessage))
    {
        return false;
    }
    if (metadata.imageWidth != config.imageWidth || metadata.imageHeight != config.imageHeight)
    {
        *errorMessage = QString(u8"YOLO11 checkpoint 输入尺寸与推理配置不匹配, checkpoint: %1x%2, 配置: %3x%4")
                            .arg(metadata.imageWidth)
                            .arg(metadata.imageHeight)
                            .arg(config.imageWidth)
                            .arg(config.imageHeight);
        return false;
    }

    torch::Device device(torch::kCPU);
    if (!InitializeYolo11CudaDevice(config.gpuId, &device, errorMessage))
    {
        return false;
    }

    try
    {
        Yolo11Network model(metadata.classNames.size());
        model->to(device);
        torch::load(model, config.modelPath.toStdString(), device);
        for (const auto &parameter : model->named_parameters(true))
        {
            if (!parameter.value().defined() || parameter.value().device() != device)
            {
                *errorMessage = QString(u8"YOLO11 checkpoint 参数 CUDA 设备校验失败: %1")
                                    .arg(QString::fromStdString(parameter.key()));
                return false;
            }
        }
        model->eval();
        c10::InferenceMode inferenceMode;
        const torch::Tensor warmupInput = torch::zeros({1, kInputChannels, config.imageHeight, config.imageWidth},
                                                       torch::TensorOptions().device(device).dtype(torch::kFloat32));
        const std::vector<torch::Tensor> warmupOutputs = model->forward(warmupInput);
        torch::Tensor decoded;
        if (!DecodePredictions(warmupOutputs,
                               metadata.classNames.size(),
                               config.imageWidth,
                               config.imageHeight,
                               device,
                               &decoded,
                               errorMessage))
        {
            return false;
        }
        if (decoded.dim() != 3 || decoded.size(0) != 1 || decoded.size(1) != 4 + metadata.classNames.size())
        {
            *errorMessage = QString(u8"YOLO11 预热解码输出形状无效");
            return false;
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
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage =
            QString(u8"无法加载 YOLO11 checkpoint %1: %2").arg(config.modelPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage =
            QString(u8"无法加载 YOLO11 checkpoint %1: %2").arg(config.modelPath, QString::fromLocal8Bit(error.what()));
        return false;
    }
}

bool Yolo11Inference::infer(const plugin_api::DetectionInferRequest &request,
                            plugin_api::DetectionInferResult *result,
                            QString *errorMessage)
{
    if (result == nullptr)
    {
        *errorMessage = QString(u8"YOLO11 推理结果指针为空");
        return false;
    }
    if (!m_model)
    {
        *errorMessage = QString(u8"YOLO11 推理模型尚未加载");
        return false;
    }
    if (request.imagePath.isEmpty() || !QFileInfo::exists(request.imagePath))
    {
        *errorMessage = QString(u8"YOLO11 推理图像不存在: %1").arg(request.imagePath);
        return false;
    }
    if (request.confidenceThreshold < 0.0 || request.confidenceThreshold > 1.0 || request.nmsThreshold < 0.0 ||
        request.nmsThreshold > 1.0)
    {
        *errorMessage = QString(u8"YOLO11 推理置信度或 NMS 阈值无效");
        return false;
    }

    QImage letterboxed;
    LetterboxTransform transform;
    if (!Letterbox(QImage(request.imagePath), m_imageWidth, m_imageHeight, &letterboxed, &transform, errorMessage))
    {
        return false;
    }

    try
    {
        c10::InferenceMode inferenceMode;
        const std::vector<torch::Tensor> outputs = m_model->forward(ToTensor(letterboxed, m_device));
        if (!CheckCudaStatus(cudaDeviceSynchronize(), QString(u8"完成模型前向"), errorMessage))
        {
            return false;
        }
        torch::Tensor prediction;
        if (!DecodePredictions(outputs,
                               m_classNames.size(),
                               m_imageWidth,
                               m_imageHeight,
                               m_device,
                               &prediction,
                               errorMessage))
        {
            return false;
        }

        const torch::Tensor values =
            prediction.to(torch::kCPU, torch::kFloat32).squeeze(0).transpose(0, 1).contiguous();
        QVector<Candidate> candidates;
        for (int64_t index = 0; index < values.size(0); ++index)
        {
            const torch::Tensor item = values[index];
            const auto best = item.slice(0, 4).max(0);
            const double confidence = std::get<0>(best).item<double>();
            if (confidence < request.confidenceThreshold)
            {
                continue;
            }
            const int classId = std::get<1>(best).item<int>();
            if (classId < 0 || classId >= m_classNames.size())
            {
                *errorMessage =
                    QString(u8"YOLO11 推理输出类别索引越界: %1, 类别数: %2").arg(classId).arg(m_classNames.size());
                return false;
            }

            const double centerX = item[0].item<double>();
            const double centerY = item[1].item<double>();
            const double width = item[2].item<double>();
            const double height = item[3].item<double>();
            const double left = std::clamp((centerX - width / 2.0 - transform.paddingX) / transform.scale,
                                           0.0,
                                           static_cast<double>(transform.originalWidth));
            const double top = std::clamp((centerY - height / 2.0 - transform.paddingY) / transform.scale,
                                          0.0,
                                          static_cast<double>(transform.originalHeight));
            const double right = std::clamp((centerX + width / 2.0 - transform.paddingX) / transform.scale,
                                            0.0,
                                            static_cast<double>(transform.originalWidth));
            const double bottom = std::clamp((centerY + height / 2.0 - transform.paddingY) / transform.scale,
                                             0.0,
                                             static_cast<double>(transform.originalHeight));
            if (right <= left || bottom <= top)
            {
                continue;
            }

            Candidate candidate;
            candidate.classId = classId;
            candidate.confidence = confidence;
            candidate.x = left;
            candidate.y = top;
            candidate.width = right - left;
            candidate.height = bottom - top;
            candidates.append(candidate);
        }

        std::sort(candidates.begin(),
                  candidates.end(),
                  [](const Candidate &first, const Candidate &second)
                  {
                      return first.confidence > second.confidence;
                  });
        result->imageWidth = transform.originalWidth;
        result->imageHeight = transform.originalHeight;
        result->boxes.clear();
        for (const Candidate &candidate : candidates)
        {
            bool suppressed = false;
            for (const plugin_api::DetectionBox &kept : result->boxes)
            {
                const Candidate keptCandidate{kept.classId, kept.confidence, kept.x, kept.y, kept.width, kept.height};
                if (candidate.classId == kept.classId && IoU(candidate, keptCandidate) > request.nmsThreshold)
                {
                    suppressed = true;
                    break;
                }
            }
            if (!suppressed)
            {
                result->boxes.append({candidate.classId,
                                      m_classNames.at(candidate.classId),
                                      candidate.confidence,
                                      candidate.x,
                                      candidate.y,
                                      candidate.width,
                                      candidate.height});
            }
        }
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLO11 推理执行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
    catch (const std::exception &error)
    {
        *errorMessage = QString(u8"YOLO11 推理执行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}
} // namespace visionaiflow::yolov11
