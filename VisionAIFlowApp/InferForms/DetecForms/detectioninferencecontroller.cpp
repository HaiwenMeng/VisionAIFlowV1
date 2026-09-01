#include "detectioninferencecontroller.h"

#include "visionaiflow/plugin_api/PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

using visionaiflow::plugin_api::DetectionInferConfig;
using visionaiflow::plugin_api::DetectionInferRequest;
using visionaiflow::plugin_api::DetectionInferResult;
using visionaiflow::plugin_api::DetectionPluginMetadata;
using visionaiflow::plugin_api::IDetectionPlugin;
using visionaiflow::plugin_api::PluginManager;

namespace
{
const QStringList &SupportedImageFilters()
{
    static const QStringList filters{QStringLiteral("*.bmp"),
                                     QStringLiteral("*.jpg"),
                                     QStringLiteral("*.jpeg"),
                                     QStringLiteral("*.png"),
                                     QStringLiteral("*.tif"),
                                     QStringLiteral("*.tiff")};
    return filters;
}

bool IsSupportedImageFile(const QString &imagePath)
{
    const QFileInfo imageFileInfo(imagePath);
    const QString suffix = imageFileInfo.suffix().toLower();
    return suffix == QStringLiteral("bmp") || suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg") ||
           suffix == QStringLiteral("png") || suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff");
}
} // namespace

DetectionInferenceController::DetectionInferenceController(QObject *parent)
    : QObject(parent), m_pluginManager(std::make_unique<PluginManager>())
{
}

DetectionInferenceController::~DetectionInferenceController() = default;

QVector<DetectionInferencePluginDescriptor>
DetectionInferenceController::DiscoverPlugins(QStringList *errorMessages) const
{
    const QString pluginDirectory =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("AIModelPlugins"));
    const QVector<DetectionPluginMetadata> plugins =
        m_pluginManager->scanDetectionPluginMetadata(pluginDirectory, errorMessages);

    QVector<DetectionInferencePluginDescriptor> descriptors;
    descriptors.reserve(plugins.size());
    for (const DetectionPluginMetadata &plugin : plugins)
    {
        descriptors.append({plugin.filePath, plugin.info.id, plugin.info.displayName, plugin.info.version});
    }
    return descriptors;
}

bool DetectionInferenceController::LoadModel(const DetectionInferenceConfig &config, QString *errorMessage)
{
    if (config.pluginPath.isEmpty() || !QFileInfo::exists(config.pluginPath))
    {
        *errorMessage = QString(u8"未选择有效的检测推理插件: %1").arg(config.pluginPath);
        return false;
    }
    if (config.modelPath.isEmpty() || !QFileInfo::exists(config.modelPath))
    {
        *errorMessage = QString(u8"检测模型文件不存在: %1").arg(config.modelPath);
        return false;
    }
    if (config.imageWidth <= 0 || config.imageHeight <= 0)
    {
        *errorMessage = QString(u8"检测推理输入尺寸无效: %1 x %2").arg(config.imageWidth).arg(config.imageHeight);
        return false;
    }
    if (config.confidenceThreshold < 0.0 || config.confidenceThreshold > 1.0 || config.nmsThreshold < 0.0 ||
        config.nmsThreshold > 1.0)
    {
        *errorMessage = QString(u8"检测推理阈值必须位于 0 到 1 之间");
        return false;
    }
    if (!m_pluginManager->loadDetectionPlugin(config.pluginPath))
    {
        *errorMessage = m_pluginManager->errorMessage();
        return false;
    }

    IDetectionPlugin *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr)
    {
        *errorMessage = QString(u8"检测推理插件加载后为空: %1").arg(config.pluginPath);
        return false;
    }

    DetectionInferConfig pluginConfig;
    pluginConfig.modelPath = config.modelPath;
    pluginConfig.gpuId = config.gpuId;
    pluginConfig.imageWidth = config.imageWidth;
    pluginConfig.imageHeight = config.imageHeight;
    pluginConfig.useFp16 = config.useFp16;
    if (!plugin->loadInferenceModel(pluginConfig))
    {
        *errorMessage = plugin->errorMessage();
        return false;
    }

    m_config = config;
    m_modelLoaded = true;
    return true;
}

bool DetectionInferenceController::InferImage(const QString &imagePath, QString *errorMessage)
{
    return InferImageAndWriteResult(imagePath, errorMessage);
}

bool DetectionInferenceController::InferImages(const QStringList &imagePaths,
                                               int *completedImageCount,
                                               QString *errorMessage)
{
    if (completedImageCount != nullptr)
    {
        *completedImageCount = 0;
    }
    if (imagePaths.isEmpty())
    {
        *errorMessage = QString(u8"没有可执行检测推理的图像");
        return false;
    }

    for (const QString &imagePath : imagePaths)
    {
        if (!InferImageAndWriteResult(imagePath, errorMessage))
        {
            return false;
        }
        if (completedImageCount != nullptr)
        {
            ++(*completedImageCount);
        }
    }
    return true;
}

bool DetectionInferenceController::InferDirectory(const QString &directoryPath,
                                                  bool includeSubdirectories,
                                                  int *completedImageCount,
                                                  QString *errorMessage)
{
    const QDir directory(directoryPath);
    if (!directory.exists())
    {
        *errorMessage = QString(u8"检测推理目录不存在: %1").arg(directoryPath);
        return false;
    }

    QStringList imagePaths;
    const QDirIterator::IteratorFlags iteratorFlags =
        includeSubdirectories ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator iterator(directoryPath, SupportedImageFilters(), QDir::Files, iteratorFlags);
    while (iterator.hasNext())
    {
        imagePaths.append(iterator.next());
    }
    return InferImages(imagePaths, completedImageCount, errorMessage);
}

bool DetectionInferenceController::InferImageAndWriteResult(const QString &imagePath, QString *errorMessage)
{
    if (!m_modelLoaded)
    {
        *errorMessage = QString(u8"尚未加载检测推理模型");
        return false;
    }
    if (!QFileInfo::exists(imagePath))
    {
        *errorMessage = QString(u8"检测推理图像不存在: %1").arg(imagePath);
        return false;
    }
    if (!IsSupportedImageFile(imagePath))
    {
        *errorMessage = QString(u8"检测推理不支持图像格式: %1").arg(imagePath);
        return false;
    }

    IDetectionPlugin *plugin = m_pluginManager->detectionPlugin();
    if (plugin == nullptr)
    {
        *errorMessage = QString(u8"检测推理插件为空");
        return false;
    }

    DetectionInferRequest request;
    request.imagePath = imagePath;
    request.confidenceThreshold = m_config.confidenceThreshold;
    request.nmsThreshold = m_config.nmsThreshold;

    DetectionInferResult result;
    if (!plugin->infer(request, &result))
    {
        *errorMessage = QString(u8"检测推理失败: %1, %2").arg(imagePath, plugin->errorMessage());
        return false;
    }
    return WriteResultFile(imagePath, result, errorMessage);
}

bool DetectionInferenceController::WriteResultFile(const QString &imagePath,
                                                   const DetectionInferResult &result,
                                                   QString *errorMessage) const
{
    if (result.imageWidth <= 0 || result.imageHeight <= 0)
    {
        *errorMessage = QString(u8"检测推理结果图像尺寸无效: %1").arg(imagePath);
        return false;
    }

    const QFileInfo imageFileInfo(imagePath);
    const QString outputPath =
        QDir(imageFileInfo.absolutePath()).filePath(imageFileInfo.completeBaseName() + QStringLiteral(".txt"));
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        *errorMessage = QString(u8"无法写入检测推理结果 %1: %2").arg(outputPath, outputFile.errorString());
        return false;
    }

    QTextStream outputStream(&outputFile);
    outputStream.setEncoding(QStringConverter::Utf8);
    for (const visionaiflow::plugin_api::DetectionBox &box : result.boxes)
    {
        if (box.classId < 0)
        {
            *errorMessage = QString(u8"检测推理结果类别索引无效: %1").arg(imagePath);
            return false;
        }

        const double left = std::clamp(box.x, 0.0, static_cast<double>(result.imageWidth));
        const double top = std::clamp(box.y, 0.0, static_cast<double>(result.imageHeight));
        const double right = std::clamp(box.x + box.width, 0.0, static_cast<double>(result.imageWidth));
        const double bottom = std::clamp(box.y + box.height, 0.0, static_cast<double>(result.imageHeight));
        if (right <= left || bottom <= top)
        {
            *errorMessage = QString(u8"检测推理结果坐标无效: %1").arg(imagePath);
            return false;
        }

        const double centerX = (left + right) / 2.0 / static_cast<double>(result.imageWidth);
        const double centerY = (top + bottom) / 2.0 / static_cast<double>(result.imageHeight);
        const double width = (right - left) / static_cast<double>(result.imageWidth);
        const double height = (bottom - top) / static_cast<double>(result.imageHeight);
        outputStream << box.classId << ' ' << QString::number(centerX, 'f', 6) << ' '
                     << QString::number(centerY, 'f', 6) << ' ' << QString::number(width, 'f', 6) << ' '
                     << QString::number(height, 'f', 6) << ' ' << QString::number(box.confidence, 'f', 6) << '\n';
    }
    outputFile.close();
    return true;
}
