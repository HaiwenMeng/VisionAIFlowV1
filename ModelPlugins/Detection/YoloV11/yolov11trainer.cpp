#include "yolov11trainer.h"

#include "yolov11inference.h"
#include "yolov11loss.h"

#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>
#include <QTextStream>

#include <opencv2/opencv.hpp>
#include <torch/cuda.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace visionaiflow::yolov11
{
namespace
{
struct TrainingSample final
{
    QString imagePath;
    QString labelPath;
};

struct DetectionBox final
{
    int classId{0};
    float x1{0.0F};
    float y1{0.0F};
    float x2{0.0F};
    float y2{0.0F};
};

struct PreparedSample final
{
    cv::Mat image;
    std::vector<DetectionBox> boxes;
    QString imagePath;
};

struct AugmentationSettings final
{
    bool plots{true};
    double mosaic{1.0};
    int closeMosaic{0};
    double mixup{0.0};
    double hsvH{0.015};
    double hsvS{0.7};
    double hsvV{0.4};
    double degrees{0.0};
    double translate{0.1};
    double scale{0.5};
    double shear{0.0};
    double perspective{0.0};
    double flipUpDown{0.0};
    double flipLeftRight{0.5};
    quint32 seed{0};
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

AugmentationSettings ReadAugmentationSettings(const plugin_api::DetectionTrainConfig &config)
{
    const QVariantMap &options = config.algorithmOptions;
    AugmentationSettings settings;
    settings.plots = options.value(QStringLiteral("plots"), true).toBool();
    settings.mosaic = options.value(QStringLiteral("mosaic"), 1.0).toDouble();
    settings.closeMosaic = options.value(QStringLiteral("close_mosaic"), 0).toInt();
    settings.mixup = options.value(QStringLiteral("mixup"), 0.0).toDouble();
    settings.hsvH = options.value(QStringLiteral("hsv_h"), 0.015).toDouble();
    settings.hsvS = options.value(QStringLiteral("hsv_s"), 0.7).toDouble();
    settings.hsvV = options.value(QStringLiteral("hsv_v"), 0.4).toDouble();
    settings.degrees = options.value(QStringLiteral("degrees"), 0.0).toDouble();
    settings.translate = options.value(QStringLiteral("translate"), 0.1).toDouble();
    settings.scale = options.value(QStringLiteral("scale"), 0.5).toDouble();
    settings.shear = options.value(QStringLiteral("shear"), 0.0).toDouble();
    settings.perspective = options.value(QStringLiteral("perspective"), 0.0).toDouble();
    settings.flipUpDown = options.value(QStringLiteral("flipud"), 0.0).toDouble();
    settings.flipLeftRight = options.value(QStringLiteral("fliplr"), 0.5).toDouble();
    settings.seed = options.value(QStringLiteral("seed"), 0).toUInt();
    return settings;
}

bool IsProbability(const double value)
{
    return value >= 0.0 && value <= 1.0;
}

bool ValidateAugmentationSettings(const AugmentationSettings &settings, QString *errorMessage)
{
    if (!IsProbability(settings.mosaic) || !IsProbability(settings.mixup) || !IsProbability(settings.flipUpDown) ||
        !IsProbability(settings.flipLeftRight) || settings.closeMosaic < 0 || settings.hsvH < 0.0 ||
        settings.hsvS < 0.0 || settings.hsvV < 0.0 || settings.degrees < 0.0 || settings.translate < 0.0 ||
        settings.scale < 0.0 || settings.shear < 0.0 || settings.perspective < 0.0)
    {
        *errorMessage = QString(u8"YOLO11 数据增强参数无效");
        return false;
    }
    return true;
}

double Uniform(std::mt19937 &generator, const double minimum, const double maximum)
{
    return std::uniform_real_distribution<double>(minimum, maximum)(generator);
}

int RandomIndex(std::mt19937 &generator, const int size)
{
    return std::uniform_int_distribution<int>(0, size - 1)(generator);
}

bool LoadBgrImage(const QString &path, cv::Mat *image, QString *errorMessage)
{
    const QImage source(path);
    if (source.isNull())
    {
        *errorMessage = QString(u8"无法加载 YOLO11 训练图像: %1").arg(path);
        return false;
    }
    const QImage rgb = source.convertToFormat(QImage::Format_RGB888);
    const cv::Mat rgbView(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar *>(rgb.constBits()), rgb.bytesPerLine());
    cv::cvtColor(rgbView, *image, cv::COLOR_RGB2BGR);
    return true;
}

bool LoadResizedSample(const TrainingSample &sample,
                       const int imageSize,
                       const int classCount,
                       PreparedSample *prepared,
                       QString *errorMessage)
{
    cv::Mat source;
    if (!LoadBgrImage(sample.imagePath, &source, errorMessage))
    {
        return false;
    }
    std::vector<std::array<float, 5>> labels;
    if (!ReadLabels(sample.labelPath, classCount, &labels, errorMessage))
    {
        return false;
    }

    const double ratio = static_cast<double>(imageSize) / std::max(source.rows, source.cols);
    const int resizedWidth = std::min(static_cast<int>(std::ceil(source.cols * ratio)), imageSize);
    const int resizedHeight = std::min(static_cast<int>(std::ceil(source.rows * ratio)), imageSize);
    if (resizedWidth != source.cols || resizedHeight != source.rows)
    {
        cv::resize(source, prepared->image, cv::Size(resizedWidth, resizedHeight), 0.0, 0.0, cv::INTER_LINEAR);
    }
    else
    {
        prepared->image = source;
    }
    prepared->imagePath = sample.imagePath;
    prepared->boxes.clear();
    prepared->boxes.reserve(labels.size());
    for (const std::array<float, 5> &label : labels)
    {
        const float centerX = label[1] * prepared->image.cols;
        const float centerY = label[2] * prepared->image.rows;
        const float width = label[3] * prepared->image.cols;
        const float height = label[4] * prepared->image.rows;
        prepared->boxes.push_back({static_cast<int>(label[0]),
                                   centerX - width * 0.5F,
                                   centerY - height * 0.5F,
                                   centerX + width * 0.5F,
                                   centerY + height * 0.5F});
    }
    return true;
}

void ClipBoxes(std::vector<DetectionBox> *boxes, const int width, const int height)
{
    for (DetectionBox &box : *boxes)
    {
        box.x1 = std::clamp(box.x1, 0.0F, static_cast<float>(width));
        box.y1 = std::clamp(box.y1, 0.0F, static_cast<float>(height));
        box.x2 = std::clamp(box.x2, 0.0F, static_cast<float>(width));
        box.y2 = std::clamp(box.y2, 0.0F, static_cast<float>(height));
    }
    boxes->erase(std::remove_if(boxes->begin(),
                                boxes->end(),
                                [](const DetectionBox &box)
                                {
                                    return box.x2 <= box.x1 || box.y2 <= box.y1;
                                }),
                 boxes->end());
}

void LetterBox(PreparedSample *sample, const int width, const int height, const bool scaleUp)
{
    double ratio =
        std::min(static_cast<double>(width) / sample->image.cols, static_cast<double>(height) / sample->image.rows);
    if (!scaleUp)
    {
        ratio = std::min(ratio, 1.0);
    }
    const int resizedWidth = static_cast<int>(std::round(sample->image.cols * ratio));
    const int resizedHeight = static_cast<int>(std::round(sample->image.rows * ratio));
    const double paddingWidth = (width - resizedWidth) / 2.0;
    const double paddingHeight = (height - resizedHeight) / 2.0;
    const int left = static_cast<int>(std::round(paddingWidth - 0.1));
    const int right = static_cast<int>(std::round(paddingWidth + 0.1));
    const int top = static_cast<int>(std::round(paddingHeight - 0.1));
    const int bottom = static_cast<int>(std::round(paddingHeight + 0.1));
    cv::Mat resized;
    if (resizedWidth != sample->image.cols || resizedHeight != sample->image.rows)
    {
        cv::resize(sample->image, resized, cv::Size(resizedWidth, resizedHeight), 0.0, 0.0, cv::INTER_LINEAR);
    }
    else
    {
        resized = sample->image;
    }
    cv::copyMakeBorder(resized,
                       sample->image,
                       top,
                       bottom,
                       left,
                       right,
                       cv::BORDER_CONSTANT,
                       cv::Scalar(114, 114, 114));
    for (DetectionBox &box : sample->boxes)
    {
        box.x1 = static_cast<float>(box.x1 * ratio + left);
        box.y1 = static_cast<float>(box.y1 * ratio + top);
        box.x2 = static_cast<float>(box.x2 * ratio + left);
        box.y2 = static_cast<float>(box.y2 * ratio + top);
    }
}

bool BuildMosaic(const QVector<TrainingSample> &samples,
                 const int baseIndex,
                 const int imageSize,
                 const int classCount,
                 std::mt19937 &generator,
                 PreparedSample *mosaic,
                 QString *errorMessage)
{
    std::array<int, 4> indices{baseIndex,
                               RandomIndex(generator, samples.size()),
                               RandomIndex(generator, samples.size()),
                               RandomIndex(generator, samples.size())};
    const int centerX = static_cast<int>(Uniform(generator, imageSize * 0.5, imageSize * 1.5));
    const int centerY = static_cast<int>(Uniform(generator, imageSize * 0.5, imageSize * 1.5));
    mosaic->image = cv::Mat(imageSize * 2, imageSize * 2, CV_8UC3, cv::Scalar(114, 114, 114));
    mosaic->boxes.clear();
    mosaic->imagePath = samples.at(baseIndex).imagePath;

    for (int tile = 0; tile < 4; ++tile)
    {
        PreparedSample source;
        if (!LoadResizedSample(samples.at(indices.at(tile)), imageSize, classCount, &source, errorMessage))
        {
            return false;
        }
        const int width = source.image.cols;
        const int height = source.image.rows;
        int destinationX1 = 0;
        int destinationY1 = 0;
        int destinationX2 = 0;
        int destinationY2 = 0;
        int sourceX1 = 0;
        int sourceY1 = 0;
        int sourceX2 = 0;
        int sourceY2 = 0;
        if (tile == 0)
        {
            destinationX1 = std::max(centerX - width, 0);
            destinationY1 = std::max(centerY - height, 0);
            destinationX2 = centerX;
            destinationY2 = centerY;
            sourceX1 = width - (destinationX2 - destinationX1);
            sourceY1 = height - (destinationY2 - destinationY1);
            sourceX2 = width;
            sourceY2 = height;
        }
        else if (tile == 1)
        {
            destinationX1 = centerX;
            destinationY1 = std::max(centerY - height, 0);
            destinationX2 = std::min(centerX + width, imageSize * 2);
            destinationY2 = centerY;
            sourceX2 = std::min(width, destinationX2 - destinationX1);
            sourceY1 = height - (destinationY2 - destinationY1);
            sourceY2 = height;
        }
        else if (tile == 2)
        {
            destinationX1 = std::max(centerX - width, 0);
            destinationY1 = centerY;
            destinationX2 = centerX;
            destinationY2 = std::min(imageSize * 2, centerY + height);
            sourceX1 = width - (destinationX2 - destinationX1);
            sourceX2 = width;
            sourceY2 = std::min(destinationY2 - destinationY1, height);
        }
        else
        {
            destinationX1 = centerX;
            destinationY1 = centerY;
            destinationX2 = std::min(centerX + width, imageSize * 2);
            destinationY2 = std::min(centerY + height, imageSize * 2);
            sourceX2 = std::min(width, destinationX2 - destinationX1);
            sourceY2 = std::min(height, destinationY2 - destinationY1);
        }
        source.image(cv::Rect(sourceX1, sourceY1, sourceX2 - sourceX1, sourceY2 - sourceY1))
            .copyTo(mosaic->image(
                cv::Rect(destinationX1, destinationY1, destinationX2 - destinationX1, destinationY2 - destinationY1)));
        const float paddingX = static_cast<float>(destinationX1 - sourceX1);
        const float paddingY = static_cast<float>(destinationY1 - sourceY1);
        for (DetectionBox box : source.boxes)
        {
            box.x1 += paddingX;
            box.y1 += paddingY;
            box.x2 += paddingX;
            box.y2 += paddingY;
            mosaic->boxes.push_back(box);
        }
    }
    ClipBoxes(&mosaic->boxes, imageSize * 2, imageSize * 2);
    return true;
}

void RandomPerspective(PreparedSample *sample,
                       const int outputWidth,
                       const int outputHeight,
                       const AugmentationSettings &settings,
                       std::mt19937 &generator)
{
    cv::Matx33f center = cv::Matx33f::eye();
    center(0, 2) = -sample->image.cols / 2.0F;
    center(1, 2) = -sample->image.rows / 2.0F;
    cv::Matx33f perspective = cv::Matx33f::eye();
    perspective(2, 0) = static_cast<float>(Uniform(generator, -settings.perspective, settings.perspective));
    perspective(2, 1) = static_cast<float>(Uniform(generator, -settings.perspective, settings.perspective));
    const double angle = Uniform(generator, -settings.degrees, settings.degrees);
    const double scale = Uniform(generator, 1.0 - settings.scale, 1.0 + settings.scale);
    const cv::Mat rotation2 = cv::getRotationMatrix2D(cv::Point2f(0.0F, 0.0F), angle, scale);
    cv::Matx33f rotation = cv::Matx33f::eye();
    for (int row = 0; row < 2; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            rotation(row, column) = static_cast<float>(rotation2.at<double>(row, column));
        }
    }
    cv::Matx33f shear = cv::Matx33f::eye();
    shear(0, 1) = static_cast<float>(std::tan(Uniform(generator, -settings.shear, settings.shear) * CV_PI / 180.0));
    shear(1, 0) = static_cast<float>(std::tan(Uniform(generator, -settings.shear, settings.shear) * CV_PI / 180.0));
    cv::Matx33f translation = cv::Matx33f::eye();
    translation(0, 2) =
        static_cast<float>(Uniform(generator, 0.5 - settings.translate, 0.5 + settings.translate) * outputWidth);
    translation(1, 2) =
        static_cast<float>(Uniform(generator, 0.5 - settings.translate, 0.5 + settings.translate) * outputHeight);
    const cv::Matx33f matrix = translation * shear * rotation * perspective * center;

    cv::Mat transformedImage;
    if (settings.perspective != 0.0)
    {
        cv::warpPerspective(sample->image,
                            transformedImage,
                            cv::Mat(matrix),
                            cv::Size(outputWidth, outputHeight),
                            cv::INTER_LINEAR,
                            cv::BORDER_CONSTANT,
                            cv::Scalar(114, 114, 114));
    }
    else
    {
        cv::warpAffine(sample->image,
                       transformedImage,
                       cv::Mat(matrix).rowRange(0, 2),
                       cv::Size(outputWidth, outputHeight),
                       cv::INTER_LINEAR,
                       cv::BORDER_CONSTANT,
                       cv::Scalar(114, 114, 114));
    }
    sample->image = transformedImage;

    std::vector<DetectionBox> transformedBoxes;
    transformedBoxes.reserve(sample->boxes.size());
    for (const DetectionBox &box : sample->boxes)
    {
        const std::array<cv::Vec3f, 4> corners{cv::Vec3f(box.x1, box.y1, 1.0F),
                                               cv::Vec3f(box.x2, box.y2, 1.0F),
                                               cv::Vec3f(box.x1, box.y2, 1.0F),
                                               cv::Vec3f(box.x2, box.y1, 1.0F)};
        float minimumX = std::numeric_limits<float>::max();
        float minimumY = std::numeric_limits<float>::max();
        float maximumX = std::numeric_limits<float>::lowest();
        float maximumY = std::numeric_limits<float>::lowest();
        for (const cv::Vec3f &corner : corners)
        {
            const cv::Vec3f point = matrix * corner;
            const float divisor = settings.perspective != 0.0 ? point[2] : 1.0F;
            const float x = point[0] / divisor;
            const float y = point[1] / divisor;
            minimumX = std::min(minimumX, x);
            minimumY = std::min(minimumY, y);
            maximumX = std::max(maximumX, x);
            maximumY = std::max(maximumY, y);
        }
        DetectionBox transformed{box.classId,
                                 std::clamp(minimumX, 0.0F, static_cast<float>(outputWidth)),
                                 std::clamp(minimumY, 0.0F, static_cast<float>(outputHeight)),
                                 std::clamp(maximumX, 0.0F, static_cast<float>(outputWidth)),
                                 std::clamp(maximumY, 0.0F, static_cast<float>(outputHeight))};
        const double originalWidth = (box.x2 - box.x1) * scale;
        const double originalHeight = (box.y2 - box.y1) * scale;
        const double transformedWidth = transformed.x2 - transformed.x1;
        const double transformedHeight = transformed.y2 - transformed.y1;
        const double aspectRatio = std::max(transformedWidth / (transformedHeight + 1.0e-16),
                                            transformedHeight / (transformedWidth + 1.0e-16));
        const double areaRatio = transformedWidth * transformedHeight / (originalWidth * originalHeight + 1.0e-16);
        if (transformedWidth > 2.0 && transformedHeight > 2.0 && areaRatio > 0.10 && aspectRatio < 100.0)
        {
            transformedBoxes.push_back(transformed);
        }
    }
    sample->boxes = std::move(transformedBoxes);
}

bool BuildPreTransform(const QVector<TrainingSample> &samples,
                       const int baseIndex,
                       const int imageSize,
                       const int classCount,
                       const bool mosaicEnabled,
                       const AugmentationSettings &settings,
                       std::mt19937 &generator,
                       PreparedSample *prepared,
                       QString *errorMessage)
{
    if (Uniform(generator, 0.0, 1.0) <= (mosaicEnabled ? settings.mosaic : 0.0))
    {
        if (!BuildMosaic(samples, baseIndex, imageSize, classCount, generator, prepared, errorMessage))
        {
            return false;
        }
        RandomPerspective(prepared, imageSize, imageSize, settings, generator);
        return true;
    }
    if (!LoadResizedSample(samples.at(baseIndex), imageSize, classCount, prepared, errorMessage))
    {
        return false;
    }
    LetterBox(prepared, imageSize, imageSize, true);
    RandomPerspective(prepared, imageSize, imageSize, settings, generator);
    return true;
}

void ApplyMixUp(PreparedSample *first, const PreparedSample &second, std::mt19937 &generator)
{
    std::gamma_distribution<double> gamma(32.0, 1.0);
    const double firstWeight = gamma(generator);
    const double secondWeight = gamma(generator);
    const double ratio = firstWeight / (firstWeight + secondWeight);
    cv::addWeighted(first->image, ratio, second.image, 1.0 - ratio, 0.0, first->image);
    first->boxes.insert(first->boxes.end(), second.boxes.begin(), second.boxes.end());
}

void ApplyHsv(PreparedSample *sample, const AugmentationSettings &settings, std::mt19937 &generator)
{
    const std::array<double, 3> gains{Uniform(generator, -1.0, 1.0) * settings.hsvH + 1.0,
                                      Uniform(generator, -1.0, 1.0) * settings.hsvS + 1.0,
                                      Uniform(generator, -1.0, 1.0) * settings.hsvV + 1.0};
    cv::Mat hsv;
    cv::cvtColor(sample->image, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    std::array<cv::Mat, 3> lookupTables{cv::Mat(1, 256, CV_8U), cv::Mat(1, 256, CV_8U), cv::Mat(1, 256, CV_8U)};
    for (int value = 0; value < 256; ++value)
    {
        lookupTables[0].at<uchar>(value) = static_cast<uchar>(static_cast<int>(value * gains[0]) % 180);
        lookupTables[1].at<uchar>(value) = cv::saturate_cast<uchar>(value * gains[1]);
        lookupTables[2].at<uchar>(value) = cv::saturate_cast<uchar>(value * gains[2]);
    }
    for (int channel = 0; channel < 3; ++channel)
    {
        cv::LUT(channels.at(channel), lookupTables.at(channel), channels.at(channel));
    }
    cv::merge(channels, hsv);
    cv::cvtColor(hsv, sample->image, cv::COLOR_HSV2BGR);
}

void ApplyFlips(PreparedSample *sample, const AugmentationSettings &settings, std::mt19937 &generator)
{
    if (Uniform(generator, 0.0, 1.0) < settings.flipUpDown)
    {
        cv::flip(sample->image, sample->image, 0);
        for (DetectionBox &box : sample->boxes)
        {
            const float top = box.y1;
            box.y1 = sample->image.rows - box.y2;
            box.y2 = sample->image.rows - top;
        }
    }
    if (Uniform(generator, 0.0, 1.0) < settings.flipLeftRight)
    {
        cv::flip(sample->image, sample->image, 1);
        for (DetectionBox &box : sample->boxes)
        {
            const float left = box.x1;
            box.x1 = sample->image.cols - box.x2;
            box.x2 = sample->image.cols - left;
        }
    }
}

bool BuildTrainingSample(const QVector<TrainingSample> &samples,
                         const int baseIndex,
                         const int imageSize,
                         const int classCount,
                         const bool mosaicEnabled,
                         const AugmentationSettings &settings,
                         std::mt19937 &generator,
                         PreparedSample *prepared,
                         QString *errorMessage)
{
    if (!BuildPreTransform(samples,
                           baseIndex,
                           imageSize,
                           classCount,
                           mosaicEnabled,
                           settings,
                           generator,
                           prepared,
                           errorMessage))
    {
        return false;
    }
    if (Uniform(generator, 0.0, 1.0) <= settings.mixup)
    {
        PreparedSample second;
        if (!BuildPreTransform(samples,
                               RandomIndex(generator, samples.size()),
                               imageSize,
                               classCount,
                               mosaicEnabled,
                               settings,
                               generator,
                               &second,
                               errorMessage))
        {
            return false;
        }
        ApplyMixUp(prepared, second, generator);
    }
    ApplyHsv(prepared, settings, generator);
    ApplyFlips(prepared, settings, generator);
    return true;
}

bool BuildValidationSample(const TrainingSample &sample,
                           const int imageSize,
                           const int classCount,
                           PreparedSample *prepared,
                           QString *errorMessage)
{
    if (!LoadResizedSample(sample, imageSize, classCount, prepared, errorMessage))
    {
        return false;
    }
    LetterBox(prepared, imageSize, imageSize, false);
    ClipBoxes(&prepared->boxes, imageSize, imageSize);
    return true;
}

torch::Tensor ImageToTensor(const cv::Mat &bgrImage)
{
    cv::Mat rgbImage;
    cv::cvtColor(bgrImage, rgbImage, cv::COLOR_BGR2RGB);
    return torch::from_blob(rgbImage.data,
                            {rgbImage.rows, rgbImage.cols, 3},
                            torch::TensorOptions().dtype(torch::kUInt8))
        .clone()
        .permute({2, 0, 1})
        .to(torch::kFloat32)
        .div_(255.0F)
        .contiguous();
}

bool SaveBatchPlot(const std::vector<PreparedSample> &samples,
                   const QStringList &classNames,
                   const bool useClassNames,
                   const QString &path,
                   QString *errorMessage)
{
    const int count = std::min(static_cast<int>(samples.size()), 16);
    if (count == 0)
    {
        *errorMessage = QString(u8"YOLO11 标注图没有可绘制样本: %1").arg(path);
        return false;
    }
    const int gridSize = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    const int sourceWidth = samples.front().image.cols;
    const int sourceHeight = samples.front().image.rows;
    const double scale = std::min(1.0, 1920.0 / gridSize / std::max(sourceWidth, sourceHeight));
    const int cellWidth = static_cast<int>(std::ceil(sourceWidth * scale));
    const int cellHeight = static_cast<int>(std::ceil(sourceHeight * scale));
    QImage mosaic(gridSize * cellWidth, gridSize * cellHeight, QImage::Format_RGB888);
    mosaic.fill(Qt::white);
    const QString fontPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Arial.Unicode.ttf"));
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (fontId < 0 || fontFamilies.isEmpty())
    {
        *errorMessage = QString(u8"无法加载 YOLO11 标注字体: %1").arg(fontPath);
        return false;
    }
    QPainter painter(&mosaic);
    QFont font(fontFamilies.front());
    font.setPixelSize(std::max(12, static_cast<int>((cellWidth + cellHeight) * gridSize * 0.01)));
    painter.setFont(font);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int index = 0; index < count; ++index)
    {
        const int x = cellWidth * (index / gridSize);
        const int y = cellHeight * (index % gridSize);
        cv::Mat rgb;
        cv::cvtColor(samples.at(index).image, rgb, cv::COLOR_BGR2RGB);
        const QImage source(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        painter.drawImage(QRect(x, y, cellWidth, cellHeight), source);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawRect(QRect(x, y, cellWidth - 1, cellHeight - 1));
        painter.setPen(QColor(220, 220, 220));
        painter.drawText(x + 5, y + font.pixelSize() + 5, QFileInfo(samples.at(index).imagePath).fileName().left(40));
        for (const DetectionBox &box : samples.at(index).boxes)
        {
            const QColor color = QColor::fromHsv((box.classId * 47) % 360, 230, 255);
            const QRectF rectangle(x + box.x1 * scale,
                                   y + box.y1 * scale,
                                   (box.x2 - box.x1) * scale,
                                   (box.y2 - box.y1) * scale);
            painter.setPen(QPen(color, 2));
            painter.drawRect(rectangle);
            const QString label = useClassNames ? classNames.value(box.classId, QString::number(box.classId))
                                                : QString::number(box.classId);
            const QRect textRectangle = painter.fontMetrics().boundingRect(label).adjusted(-3, -2, 3, 2);
            QRectF background(rectangle.left(), rectangle.top(), textRectangle.width(), textRectangle.height());
            if (background.top() < 0.0)
            {
                background.moveTop(0.0);
            }
            painter.fillRect(background, color);
            painter.setPen(Qt::black);
            painter.drawText(background.adjusted(3, 1, -3, -1), Qt::AlignLeft | Qt::AlignVCenter, label);
        }
    }
    painter.end();
    QFontDatabase::removeApplicationFont(fontId);
    if (!mosaic.save(path, "JPG", 95))
    {
        *errorMessage = QString(u8"无法保存 YOLO11 标注图: %1").arg(path);
        return false;
    }
    return true;
}

bool SaveValidationPlots(const QVector<TrainingSample> &samples,
                         const plugin_api::DetectionTrainConfig &config,
                         QString *errorMessage)
{
    const int batchCount = std::min(3, static_cast<int>((samples.size() + config.batchSize - 1) / config.batchSize));
    for (int batchIndex = 0; batchIndex < batchCount; ++batchIndex)
    {
        std::vector<PreparedSample> batch;
        const int begin = batchIndex * config.batchSize;
        const int end = std::min(begin + config.batchSize, static_cast<int>(samples.size()));
        for (int index = begin; index < end; ++index)
        {
            PreparedSample prepared;
            if (!BuildValidationSample(samples.at(index),
                                       config.imageWidth,
                                       config.classNames.size(),
                                       &prepared,
                                       errorMessage))
            {
                return false;
            }
            batch.push_back(std::move(prepared));
        }
        const QString path = QDir(config.outputPath).filePath(QStringLiteral("val_batch%1_labels.jpg").arg(batchIndex));
        if (!SaveBatchPlot(batch, config.classNames, true, path, errorMessage))
        {
            return false;
        }
    }
    return true;
}

bool ShouldSaveTrainingPlot(const int globalBatch, const int batchesPerEpoch, const int epochs, const int closeMosaic)
{
    if (globalBatch >= 0 && globalBatch <= 2)
    {
        return true;
    }
    if (closeMosaic <= 0)
    {
        return false;
    }
    const int firstClosedMosaicBatch = (epochs - closeMosaic) * batchesPerEpoch;
    return globalBatch >= firstClosedMosaicBatch && globalBatch <= firstClosedMosaicBatch + 2;
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
        config.imageHeight <= 0 || config.imageWidth != config.imageHeight || config.imageWidth % 32 != 0 ||
        config.imageHeight % 32 != 0)
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
    const AugmentationSettings augmentation = ReadAugmentationSettings(config);
    if (!ValidateAugmentationSettings(augmentation, errorMessage))
    {
        return false;
    }
    m_validationDatasetPath = QDir(QFileInfo(config.datasetPath).absolutePath()).filePath(QStringLiteral("val.csv"));
    if (!QFileInfo::exists(m_validationDatasetPath))
    {
        *errorMessage = QString(u8"YOLO11 验证数据清单不存在: %1").arg(m_validationDatasetPath);
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
    QVector<TrainingSample> validationSamples;
    if (!ReadSamples(m_validationDatasetPath, &validationSamples, errorMessage))
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
        const AugmentationSettings augmentation = ReadAugmentationSettings(m_config);
        std::mt19937 generator(augmentation.seed);
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
            std::vector<int> indices(samples.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::shuffle(indices.begin(), indices.end(), generator);
            const bool mosaicEnabled =
                augmentation.closeMosaic <= 0 || epoch < m_config.epochs - augmentation.closeMosaic;
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
                std::vector<PreparedSample> preparedSamples;
                for (int index = begin; index < end; ++index)
                {
                    PreparedSample prepared;
                    if (!BuildTrainingSample(samples,
                                             indices.at(index),
                                             m_config.imageWidth,
                                             m_config.classNames.size(),
                                             mosaicEnabled,
                                             augmentation,
                                             generator,
                                             &prepared,
                                             errorMessage))
                    {
                        return TrainRunResult::Failed;
                    }
                    images.push_back(ImageToTensor(prepared.image));
                    for (const DetectionBox &box : prepared.boxes)
                    {
                        const float centerX = (box.x1 + box.x2) * 0.5F / m_config.imageWidth;
                        const float centerY = (box.y1 + box.y2) * 0.5F / m_config.imageHeight;
                        const float boxWidth = (box.x2 - box.x1) / m_config.imageWidth;
                        const float boxHeight = (box.y2 - box.y1) / m_config.imageHeight;
                        targetRows.push_back({static_cast<float>(index - begin),
                                              static_cast<float>(box.classId),
                                              centerX,
                                              centerY,
                                              boxWidth,
                                              boxHeight});
                    }
                    preparedSamples.push_back(std::move(prepared));
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
                const int globalBatch = step + totalSteps * epoch;
                if (augmentation.plots &&
                    ShouldSaveTrainingPlot(globalBatch, totalSteps, m_config.epochs, augmentation.closeMosaic))
                {
                    const QString plotPath =
                        QDir(m_config.outputPath).filePath(QStringLiteral("train_batch%1.jpg").arg(globalBatch));
                    if (!SaveBatchPlot(preparedSamples, m_config.classNames, false, plotPath, errorMessage))
                    {
                        return TrainRunResult::Failed;
                    }
                }
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
            if (augmentation.plots && !SaveValidationPlots(validationSamples, m_config, errorMessage))
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
