#include "trtsam3lib.h"

#include <array>
#include <exception>
#include <string>
#include <vector>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

#include <opencv2/imgproc.hpp>

#include "third_party/sam3src/common/object.hpp"
#include "third_party/sam3src/infer/infer.hpp"
#include "third_party/sam3src/infer/sam3type.hpp"

namespace
{
const char *kGeometryLabel = "visual";
const int kMaxGeometryBoxes = 20;
} // namespace

TrtSam3::TrtSam3()
{
}

TrtSam3::~TrtSam3()
{
}

bool TrtSam3::initialize(const QString &modelDir, int deviceId)
{
    m_modelDir = modelDir;
    m_deviceId = deviceId;
    m_initialized = false;
    m_engine.reset();

    const QString visionPath = toResolveEnginePath(modelDir,
                                                   {QStringLiteral("vision-encoder.fp16.trt"),
                                                    QStringLiteral("vision-encoder.trt"),
                                                    QStringLiteral("vision-encoder.engine")});
    const QString textPath = toResolveEnginePath(modelDir,
                                                 {QStringLiteral("text-encoder.fp16.trt"),
                                                  QStringLiteral("text-encoder.trt"),
                                                  QStringLiteral("text-encoder.engine")});
    const QString geometryPath = toResolveEnginePath(modelDir,
                                                     {QStringLiteral("geometry-encoder.fp16.trt"),
                                                      QStringLiteral("geometry-encoder.trt"),
                                                      QStringLiteral("geometry-encoder.engine")});
    const QString decoderPath = toResolveEnginePath(
        modelDir,
        {QStringLiteral("decoder.fp16.trt"), QStringLiteral("decoder.trt"), QStringLiteral("decoder.engine")});

    if (visionPath.isEmpty() || textPath.isEmpty() || geometryPath.isEmpty() || decoderPath.isEmpty())
    {
        qWarning() << "SAM3 model file missing in" << modelDir << "vision:" << visionPath << "text:" << textPath
                   << "geometry:" << geometryPath << "decoder:" << decoderPath;
        return false;
    }

    try
    {
        m_engine = load(visionPath.toStdString(),
                        textPath.toStdString(),
                        geometryPath.toStdString(),
                        decoderPath.toStdString(),
                        m_deviceId);
        m_initialized = (m_engine != nullptr);
    }
    catch (const std::exception &ex)
    {
        qWarning() << "SAM3 init exception:" << ex.what();
        m_engine.reset();
        m_initialized = false;
    }
    catch (...)
    {
        qWarning() << "SAM3 init failed with unknown exception.";
        m_engine.reset();
        m_initialized = false;
    }

    if (m_initialized && !m_currentImage.isNull())
    {
        setCurrentImage(m_currentImage);
    }

    return m_initialized;
}

void TrtSam3::setCurrentImage(const QImage &image)
{
    m_currentImage = toImageForInference(image);

    if (m_currentImage.isNull())
    {
        m_currentImageBgr.reset();
        return;
    }

    m_currentImageBgr.reset(new cv::Mat(toQImageAsBgrMat(m_currentImage)));
}

bool TrtSam3::isInitialized() const
{
    return m_initialized;
}

QString TrtSam3::modelDir() const
{
    return m_modelDir;
}

QString TrtSam3::displayName() const
{
    return QStringLiteral("SAM3");
}

TrtSam3InferResult TrtSam3::inferByPoint(const QPointF &point, const QString &labelName) const
{
    return toInferByGeometryRect(toPointPromptRect(point), labelName);
}

TrtSam3InferResult TrtSam3::inferByRect(const QRectF &rect, const QString &labelName) const
{
    return toInferByGeometryRect(rect.normalized(), labelName);
}

TrtSam3InferResult TrtSam3::inferByRects(const QVector<QRectF> &rects, const QString &labelName) const
{
    return toInferByGeometryRects(rects, labelName);
}

bool TrtSam3::isReady() const
{
    return m_initialized && (m_engine != nullptr);
}

QRectF TrtSam3::toClampRect(const QRectF &rect) const
{
    if (m_currentImage.isNull())
    {
        return QRectF();
    }

    const qreal maxX = qMax(0, m_currentImage.width() - 1);
    const qreal maxY = qMax(0, m_currentImage.height() - 1);

    const qreal left = qBound<qreal>(0.0, rect.left(), maxX);
    const qreal top = qBound<qreal>(0.0, rect.top(), maxY);
    const qreal right = qBound<qreal>(0.0, rect.right(), maxX);
    const qreal bottom = qBound<qreal>(0.0, rect.bottom(), maxY);

    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
}

QString TrtSam3::toResolveEnginePath(const QString &modelDir, const QStringList &candidates) const
{
    const QDir dir(modelDir);
    for (const QString &name : candidates)
    {
        const QString path = dir.filePath(name);
        if (QFileInfo::exists(path))
        {
            return path;
        }
    }
    return QString();
}

QImage TrtSam3::toImageForInference(const QImage &image) const
{
    if (image.isNull())
    {
        return QImage();
    }

    if (image.format() == QImage::Format_RGB888)
    {
        return image;
    }

    return image.convertToFormat(QImage::Format_RGB888);
}

cv::Mat TrtSam3::toQImageAsBgrMat(const QImage &image) const
{
    if (image.isNull())
    {
        return cv::Mat();
    }

    QImage rgbImage = image;
    if (rgbImage.format() != QImage::Format_RGB888)
    {
        rgbImage = rgbImage.convertToFormat(QImage::Format_RGB888);
    }

    cv::Mat rgbMat(rgbImage.height(),
                   rgbImage.width(),
                   CV_8UC3,
                   const_cast<uchar *>(rgbImage.bits()),
                   rgbImage.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}

QRectF TrtSam3::toPointPromptRect(const QPointF &point) const
{
    if (m_currentImage.isNull())
    {
        return QRectF();
    }

    const qreal promptSide = qMax<qreal>(8.0, qMin(m_currentImage.width(), m_currentImage.height()) * 0.01);
    const QPointF topLeft(point.x() - promptSide * 0.5, point.y() - promptSide * 0.5);
    const QPointF bottomRight(point.x() + promptSide * 0.5, point.y() + promptSide * 0.5);
    return toClampRect(QRectF(topLeft, bottomRight));
}

TrtSam3InferResult TrtSam3::toInferByGeometryRect(const QRectF &rect, const QString &labelName) const
{
    TrtSam3InferResult result;

    if (!isReady())
    {
        result.errorMessage = QStringLiteral("SAM3 service not initialized.");
        return result;
    }

    if (m_currentImage.isNull() || !m_currentImageBgr || m_currentImageBgr->empty())
    {
        result.errorMessage = QStringLiteral("No image loaded for SAM3 inference.");
        return result;
    }

    const QRectF clampedRect = toClampRect(rect.normalized());
    if (clampedRect.width() < 2.0 || clampedRect.height() < 2.0)
    {
        result.errorMessage = QStringLiteral("Prompt rectangle is too small.");
        return result;
    }

    std::vector<std::pair<std::string, std::array<float, 4>>> boxes;
    boxes.push_back(std::make_pair(std::string("pos"),
                                   std::array<float, 4>{static_cast<float>(clampedRect.left()),
                                                        static_cast<float>(clampedRect.top()),
                                                        static_cast<float>(clampedRect.right()),
                                                        static_cast<float>(clampedRect.bottom())}));

    try
    {
        if (!m_engine->setup_geometry_input(*m_currentImageBgr, kGeometryLabel, boxes))
        {
            result.errorMessage = QStringLiteral("SAM3 setup_geometry_input failed.");
            return result;
        }

        std::vector<Sam3Input> inputs;
        inputs.emplace_back(*m_currentImageBgr, std::vector<Sam3PromptUnit>(), m_confidenceThreshold);

        const InferResultArray inferResults = m_engine->forwards(inputs, kGeometryLabel, true);
        if (inferResults.empty())
        {
            result.errorMessage = QStringLiteral("SAM3 inference returned empty batch results.");
            return result;
        }

        const InferResult &detections = inferResults.front();
        if (detections.empty())
        {
            result.errorMessage = QStringLiteral("SAM3 inference returned no objects.");
            return result;
        }

        for (const object::DetectionBox &det : detections)
        {
            QRectF detRect(QPointF(det.box.left, det.box.top), QPointF(det.box.right, det.box.bottom));
            detRect = toClampRect(detRect.normalized());
            if (detRect.width() < 2.0 || detRect.height() < 2.0)
            {
                continue;
            }

            TrtSam3ObjectResult objectResult;
            objectResult.success = true;
            objectResult.labelName = labelName;
            objectResult.score = det.score;
            objectResult.roiData = toRectToRoiData(detRect);

            if (det.segmentation.has_value() && !det.segmentation->mask.empty())
            {
                cv::Mat mask = toMaskBinaryU8(det.segmentation->mask);
                if (!mask.empty() && (mask.cols != m_currentImage.width() || mask.rows != m_currentImage.height()))
                {
                    const object::Segmentation aligned =
                        det.segmentation->align_to_left_top(static_cast<int>(detRect.left()),
                                                            static_cast<int>(detRect.top()),
                                                            m_currentImage.width(),
                                                            m_currentImage.height());
                    mask = toMaskBinaryU8(aligned.mask);
                }

                objectResult.maskContour = toLargestContour(mask);
                objectResult.minRectRoiData = toAxisAlignedRectRoiData(objectResult.maskContour);
            }

            if (objectResult.minRectRoiData.size() < 8)
            {
                objectResult.minRectRoiData = objectResult.roiData;
            }

            result.objects.push_back(objectResult);
        }

        if (result.objects.isEmpty())
        {
            result.errorMessage = QStringLiteral("SAM3 returned objects but no valid bounding area.");
            return result;
        }

        result.success = true;
        return result;
    }
    catch (const std::exception &ex)
    {
        result.errorMessage = QStringLiteral("SAM3 inference exception: %1").arg(QString::fromStdString(ex.what()));
        return result;
    }
    catch (...)
    {
        result.errorMessage = QStringLiteral("SAM3 inference failed with unknown exception.");
        return result;
    }
}

TrtSam3InferResult TrtSam3::toInferByGeometryRects(const QVector<QRectF> &rects, const QString &labelName) const
{
    TrtSam3InferResult result;

    if (!isReady())
    {
        result.errorMessage = QStringLiteral("SAM3 service not initialized.");
        return result;
    }

    if (m_currentImage.isNull() || !m_currentImageBgr || m_currentImageBgr->empty())
    {
        result.errorMessage = QStringLiteral("No image loaded for SAM3 inference.");
        return result;
    }

    if (rects.isEmpty())
    {
        result.errorMessage = QStringLiteral("Prompt rectangles are empty.");
        return result;
    }

    std::vector<std::pair<std::string, std::array<float, 4>>> boxes;
    boxes.reserve(static_cast<size_t>(rects.size()));

    for (const QRectF &rect : rects)
    {
        const QRectF clampedRect = toClampRect(rect.normalized());
        if (clampedRect.width() < 2.0 || clampedRect.height() < 2.0)
        {
            continue;
        }

        boxes.push_back(std::make_pair(std::string("pos"),
                                       std::array<float, 4>{static_cast<float>(clampedRect.left()),
                                                            static_cast<float>(clampedRect.top()),
                                                            static_cast<float>(clampedRect.right()),
                                                            static_cast<float>(clampedRect.bottom())}));
    }

    if (boxes.empty())
    {
        result.errorMessage = QStringLiteral("No valid prompt rectangles.");
        return result;
    }

    if (boxes.size() > static_cast<size_t>(kMaxGeometryBoxes))
    {
        result.errorMessage = QStringLiteral("Prompt rectangles exceed max supported count 20.");
        return result;
    }

    try
    {
        std::vector<Sam3PromptUnit> prompts;
        prompts.emplace_back(kGeometryLabel, boxes);

        std::vector<Sam3Input> inputs;
        inputs.emplace_back(*m_currentImageBgr, prompts, m_confidenceThreshold);

        const InferResultArray inferResults = m_engine->forwards(inputs, true);
        if (inferResults.empty())
        {
            result.errorMessage = QStringLiteral("SAM3 inference returned empty batch results.");
            return result;
        }

        const InferResult &detections = inferResults.front();
        if (detections.empty())
        {
            result.errorMessage = QStringLiteral("SAM3 inference returned no objects.");
            return result;
        }

        for (const object::DetectionBox &det : detections)
        {
            QRectF detRect(QPointF(det.box.left, det.box.top), QPointF(det.box.right, det.box.bottom));
            detRect = toClampRect(detRect.normalized());
            if (detRect.width() < 2.0 || detRect.height() < 2.0)
            {
                continue;
            }

            TrtSam3ObjectResult objectResult;
            objectResult.success = true;
            objectResult.labelName = labelName;
            objectResult.score = det.score;
            objectResult.roiData = toRectToRoiData(detRect);

            if (det.segmentation.has_value() && !det.segmentation->mask.empty())
            {
                cv::Mat mask = toMaskBinaryU8(det.segmentation->mask);
                if (!mask.empty() && (mask.cols != m_currentImage.width() || mask.rows != m_currentImage.height()))
                {
                    const object::Segmentation aligned =
                        det.segmentation->align_to_left_top(static_cast<int>(detRect.left()),
                                                            static_cast<int>(detRect.top()),
                                                            m_currentImage.width(),
                                                            m_currentImage.height());
                    mask = toMaskBinaryU8(aligned.mask);
                }

                objectResult.maskContour = toLargestContour(mask);
                objectResult.minRectRoiData = toAxisAlignedRectRoiData(objectResult.maskContour);
            }

            if (objectResult.minRectRoiData.size() < 8)
            {
                objectResult.minRectRoiData = objectResult.roiData;
            }

            result.objects.push_back(objectResult);
        }

        if (result.objects.isEmpty())
        {
            result.errorMessage = QStringLiteral("SAM3 returned objects but no valid bounding area.");
            return result;
        }

        result.success = true;
        return result;
    }
    catch (const std::exception &ex)
    {
        result.errorMessage = QStringLiteral("SAM3 inference exception: %1").arg(QString::fromStdString(ex.what()));
        return result;
    }
    catch (...)
    {
        result.errorMessage = QStringLiteral("SAM3 inference failed with unknown exception.");
        return result;
    }
}

QVector<double> TrtSam3::toRectToRoiData(const QRectF &rect)
{
    return {rect.left(), rect.top(), rect.right(), rect.top(), rect.right(), rect.bottom(), rect.left(), rect.bottom()};
}

cv::Mat TrtSam3::toMaskBinaryU8(const cv::Mat &mask)
{
    if (mask.empty())
    {
        return cv::Mat();
    }

    cv::Mat maskU8;
    if (mask.type() == CV_8UC1)
    {
        maskU8 = mask.clone();
    }
    else if (mask.type() == CV_32FC1 || mask.type() == CV_64FC1)
    {
        cv::Mat thresholdMask;
        cv::threshold(mask, thresholdMask, 0.0, 255.0, cv::THRESH_BINARY);
        thresholdMask.convertTo(maskU8, CV_8UC1);
    }
    else
    {
        cv::Mat grayMask;
        if (mask.channels() == 3)
        {
            cv::cvtColor(mask, grayMask, cv::COLOR_BGR2GRAY);
        }
        else
        {
            grayMask = mask;
        }
        cv::threshold(grayMask, maskU8, 0, 255, cv::THRESH_BINARY);
    }

    return maskU8;
}

QVector<QPointF> TrtSam3::toLargestContour(const cv::Mat &mask)
{
    QVector<QPointF> contourPoints;
    const cv::Mat maskU8 = toMaskBinaryU8(mask);
    if (maskU8.empty())
    {
        return contourPoints;
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(maskU8, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty())
    {
        return contourPoints;
    }

    int bestIdx = -1;
    double bestArea = 0.0;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
        const double area = cv::contourArea(contours[i]);
        if (area > bestArea)
        {
            bestArea = area;
            bestIdx = i;
        }
    }

    if (bestIdx < 0)
    {
        return contourPoints;
    }

    const std::vector<cv::Point> &bestContour = contours[bestIdx];
    contourPoints.reserve(static_cast<int>(bestContour.size()));
    for (const cv::Point &pt : bestContour)
    {
        contourPoints.push_back(QPointF(pt.x, pt.y));
    }

    return contourPoints;
}

QVector<double> TrtSam3::toAxisAlignedRectRoiData(const QVector<QPointF> &contour)
{
    QVector<double> rectData;
    if (contour.isEmpty())
    {
        return rectData;
    }

    qreal minX = contour.first().x();
    qreal minY = contour.first().y();
    qreal maxX = minX;
    qreal maxY = minY;

    for (const QPointF &pt : contour)
    {
        minX = qMin(minX, pt.x());
        minY = qMin(minY, pt.y());
        maxX = qMax(maxX, pt.x());
        maxY = qMax(maxY, pt.y());
    }

    rectData.reserve(8);
    rectData.push_back(minX);
    rectData.push_back(minY);
    rectData.push_back(maxX);
    rectData.push_back(minY);
    rectData.push_back(maxX);
    rectData.push_back(maxY);
    rectData.push_back(minX);
    rectData.push_back(maxY);
    return rectData;
}
