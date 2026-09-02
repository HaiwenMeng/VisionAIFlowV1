#include "yolov11inference.h"

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QPainter>

#include <cuda_runtime_api.h>
#include <torch/cuda.h>

#include <algorithm>
#include <cmath>

namespace visionaiflow::yolov11
{
namespace
{
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

bool CheckCudaStatus(const cudaError_t status,
                     const QString &operation,
                     QString *errorMessage)
{
    if (status == cudaSuccess)
    {
        return true;
    }

    *errorMessage = QString(u8"YOLO11 CUDA 操作失败, %1: %2")
                        .arg(operation, QString::fromLocal8Bit(cudaGetErrorString(status)));
    return false;
}

bool Letterbox(const QImage &source, const int targetWidth, const int targetHeight, QImage *output, LetterboxTransform *transform,
               QString *errorMessage)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0)
    {
        *errorMessage = QString(u8"YOLO11 推理图像无效");
        return false;
    }
    transform->scale = std::min(static_cast<double>(targetWidth) / source.width(), static_cast<double>(targetHeight) / source.height());
    const int width = std::max(1, static_cast<int>(std::round(source.width() * transform->scale)));
    const int height = std::max(1, static_cast<int>(std::round(source.height() * transform->scale)));
    transform->paddingX = (targetWidth - width) / 2;
    transform->paddingY = (targetHeight - height) / 2;
    transform->originalWidth = source.width();
    transform->originalHeight = source.height();
    QImage canvas(targetWidth, targetHeight, QImage::Format_RGB888);
    canvas.fill(QColor(114, 114, 114));
    QPainter painter(&canvas);
    painter.drawImage(transform->paddingX, transform->paddingY,
                      source.convertToFormat(QImage::Format_RGB888).scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    painter.end();
    *output = std::move(canvas);
    return true;
}

torch::Tensor ToTensor(const QImage &image, const torch::Device &device)
{
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const int width = rgb.width();
    const int height = rgb.height();
    std::vector<float> data(static_cast<size_t>(3) * width * height);
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
    return torch::from_blob(data.data(), {1, 3, height, width}, torch::TensorOptions().dtype(torch::kFloat32)).clone().to(device);
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
        *errorMessage = QString(u8"无法加载 YOLO11 的 LibTorch CUDA 后端 %1: %2")
                            .arg(libraryPath, torchCudaLibrary.errorString());
        return false;
    }
    return true;
}

bool InitializeYolo11CudaDevice(const int gpuId,
                                torch::Device *device,
                                QString *errorMessage)
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
        *errorMessage = QString(u8"YOLO11 训练需要可用的 CUDA 设备");
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
    if (config.imageWidth <= 0 || config.imageHeight <= 0 || config.imageWidth % 32 != 0 || config.imageHeight % 32 != 0)
    {
        *errorMessage = QString(u8"YOLO11 推理输入尺寸必须为正数且能被 32 整除");
        return false;
    }
    torch::Device device(torch::kCPU);
    if (!InitializeYolo11CudaDevice(config.gpuId, &device, errorMessage))
    {
        return false;
    }
    try
    {
        m_device = device;
        torch::jit::ExtraFilesMap extraFiles;
        extraFiles["visionaiflow_yolo11.json"] = "";
        m_model = torch::jit::load(config.modelPath.toStdString(), m_device, extraFiles);
        m_model.eval();
        const QJsonDocument metadata = QJsonDocument::fromJson(QByteArray::fromStdString(extraFiles["visionaiflow_yolo11.json"]));
        if (!metadata.isObject())
        {
            *errorMessage = QString(u8"YOLO11 权重缺少转换元数据, 请使用 convert_ultralytics_checkpoint.py 转换原始 .pt");
            return false;
        }
        const QJsonObject object = metadata.object();
        if (object.value(QStringLiteral("format")).toString() != QStringLiteral("VisionAIFlow.YoloV11.TorchScript.1") ||
            object.value(QStringLiteral("ultralytics_version")).toString() != QStringLiteral("8.3.4") ||
            object.value(QStringLiteral("model_variant")).toString() != QStringLiteral("yolo11n") ||
            object.value(QStringLiteral("input_channels")).toInt() != 3 || object.value(QStringLiteral("nc")).toInt() <= 0 ||
            object.value(QStringLiteral("state_dict_items")).toInt() != 499)
        {
            *errorMessage = QString(u8"YOLO11 权重不是兼容的 Ultralytics 8.3.4 YOLO11n 转换文件");
            return false;
        }
        const QJsonObject names = object.value(QStringLiteral("names")).toObject();
        m_classNames.clear();
        for (int index = 0; index < object.value(QStringLiteral("nc")).toInt(); ++index)
        {
            const QString name = names.value(QString::number(index)).toString();
            if (name.isEmpty())
            {
                *errorMessage = QString(u8"YOLO11 权重的类别元数据不完整");
                return false;
            }
            m_classNames.append(name);
        }
        m_imageWidth = config.imageWidth;
        m_imageHeight = config.imageHeight;
        m_loaded = true;
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"加载 YOLO11 转换权重失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}

bool Yolo11Inference::infer(const plugin_api::DetectionInferRequest &request, plugin_api::DetectionInferResult *result, QString *errorMessage)
{
    if (result == nullptr)
    {
        *errorMessage = QString(u8"YOLO11 推理结果指针为空");
        return false;
    }
    if (!m_loaded)
    {
        *errorMessage = QString(u8"YOLO11 推理模型尚未加载");
        return false;
    }
    if (request.imagePath.isEmpty() || !QFileInfo::exists(request.imagePath))
    {
        *errorMessage = QString(u8"YOLO11 推理图像不存在: %1").arg(request.imagePath);
        return false;
    }
    QImage inputImage(request.imagePath);
    QImage letterboxed;
    LetterboxTransform transform;
    if (!Letterbox(inputImage, m_imageWidth, m_imageHeight, &letterboxed, &transform, errorMessage))
    {
        return false;
    }
    try
    {
        torch::NoGradGuard noGrad;
        const torch::jit::IValue output = m_model.forward({ToTensor(letterboxed, m_device)});
        if (!output.isTuple() || output.toTuple()->elements().empty() || !output.toTuple()->elements()[0].isTensor())
        {
            *errorMessage = QString(u8"YOLO11 TorchScript 输出不是检测张量");
            return false;
        }
        const torch::Tensor prediction = output.toTuple()->elements()[0].toTensor().to(torch::kCPU).to(torch::kFloat32);
        if (prediction.dim() != 3 || prediction.size(0) != 1 || prediction.size(1) != 4 + m_classNames.size())
        {
            *errorMessage = QString(u8"YOLO11 输出张量形状无效");
            return false;
        }
        const torch::Tensor values = prediction.squeeze(0).transpose(0, 1).contiguous();
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
            const double centerX = item[0].item<double>();
            const double centerY = item[1].item<double>();
            const double width = item[2].item<double>();
            const double height = item[3].item<double>();
            Candidate candidate;
            candidate.classId = classId;
            candidate.confidence = confidence;
            candidate.x = (centerX - width / 2.0 - transform.paddingX) / transform.scale;
            candidate.y = (centerY - height / 2.0 - transform.paddingY) / transform.scale;
            candidate.width = width / transform.scale;
            candidate.height = height / transform.scale;
            candidate.x = std::clamp(candidate.x, 0.0, static_cast<double>(transform.originalWidth));
            candidate.y = std::clamp(candidate.y, 0.0, static_cast<double>(transform.originalHeight));
            candidate.width = std::clamp(candidate.width, 0.0, static_cast<double>(transform.originalWidth) - candidate.x);
            candidate.height = std::clamp(candidate.height, 0.0, static_cast<double>(transform.originalHeight) - candidate.y);
            candidates.append(candidate);
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate &first, const Candidate &second) { return first.confidence > second.confidence; });
        result->imageWidth = transform.originalWidth;
        result->imageHeight = transform.originalHeight;
        result->boxes.clear();
        for (const Candidate &candidate : candidates)
        {
            bool suppressed = false;
            for (const plugin_api::DetectionBox &kept : result->boxes)
            {
                Candidate keptCandidate{kept.classId, kept.confidence, kept.x, kept.y, kept.width, kept.height};
                if (candidate.classId == kept.classId && IoU(candidate, keptCandidate) > request.nmsThreshold)
                {
                    suppressed = true;
                    break;
                }
            }
            if (!suppressed)
            {
                result->boxes.append({candidate.classId, m_classNames.at(candidate.classId), candidate.confidence, candidate.x, candidate.y,
                                      candidate.width, candidate.height});
            }
        }
        return true;
    }
    catch (const c10::Error &error)
    {
        *errorMessage = QString(u8"YOLO11 推理执行失败: %1").arg(QString::fromLocal8Bit(error.what()));
        return false;
    }
}
} // namespace visionaiflow::yolov11
