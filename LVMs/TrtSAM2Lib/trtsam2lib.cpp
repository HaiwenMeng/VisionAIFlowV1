#include "trtsam2lib.h"

#include <exception>

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <opencv2/imgproc.hpp>

#include "third_party/sam2src/sam2.h"

TrtSam2::TrtSam2()
{
}

TrtSam2::~TrtSam2()
{
}

bool TrtSam2::initialize(const QString &modelDir, int deviceId)
{
    m_modelDir = modelDir;
    m_deviceId = deviceId;
    m_initialized = false;
    m_sam.reset();

    const QString encoderEnginePath = toResolveEncoderEnginePath(modelDir);
    const QString decoderEnginePath = toResolveDecoderEnginePath(modelDir);
    if (encoderEnginePath.isEmpty() || decoderEnginePath.isEmpty())
    {
        qWarning() << "SAM2 model file missing in" << modelDir;
        return false;
    }

    try
    {
        m_sam = std::make_shared<Sam2::SAM>(encoderEnginePath.toStdString(),
                                            decoderEnginePath.toStdString(),
                                            true,
                                            m_deviceId);
        m_initialized = (m_sam != nullptr);
    }
    catch (const std::exception &ex)
    {
        qWarning() << "SAM2 init exception:" << ex.what();
        m_sam.reset();
        m_initialized = false;
    }
    catch (...)
    {
        qWarning() << "SAM2 init failed with unknown exception.";
        m_sam.reset();
        m_initialized = false;
    }

    if (m_initialized && !m_currentImage.isNull())
    {
        setCurrentImage(m_currentImage);
    }

    return m_initialized;
}

void TrtSam2::setCurrentImage(const QImage &image)
{
    m_currentImage = toImageForInference(image);

    if (m_currentImage.isNull())
    {
        m_currentImageBgr.reset();
        return;
    }

    m_currentImageBgr.reset(new cv::Mat(toQImageAsBgrMat(m_currentImage)));

    if (m_sam)
    {
        m_sam->reset();
    }
}

bool TrtSam2::isInitialized() const
{
    return m_initialized;
}

QString TrtSam2::modelDir() const
{
    return m_modelDir;
}

QString TrtSam2::displayName() const
{
    return QStringLiteral("SAM2");
}

TrtSam2InferResult TrtSam2::inferByPoint(const QPointF &point, const QString &labelName) const
{
    return toInferByPrompt(&point, nullptr, labelName);
}

TrtSam2InferResult TrtSam2::inferByRect(const QRectF &rect, const QString &labelName) const
{
    const QRectF normalizedRect = rect.normalized();
    return toInferByPrompt(nullptr, &normalizedRect, labelName);
}

TrtSam2InferResult TrtSam2::inferByRects(const QVector<QRectF> &rects, const QString &labelName) const
{
    TrtSam2InferResult result;

    if (rects.isEmpty())
    {
        result.errorMessage = QStringLiteral("Prompt rectangles are empty.");
        return result;
    }

    for (const QRectF &rect : rects)
    {
        const TrtSam2InferResult currentResult = inferByRect(rect, labelName);
        if (!currentResult.success)
        {
            result.errorMessage = currentResult.errorMessage;
            return result;
        }

        result.objects += currentResult.objects;
    }

    if (result.objects.isEmpty())
    {
        result.errorMessage = QStringLiteral("SAM2 inference returned no objects.");
        return result;
    }

    result.success = true;
    return result;
}

bool TrtSam2::isReady() const
{
    return m_initialized && (m_sam != nullptr);
}

QRectF TrtSam2::toClampRect(const QRectF &rect) const
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

QString TrtSam2::toResolveEncoderEnginePath(const QString &modelDir) const
{
    const QDir dir(modelDir);
    const QStringList candidates = {QStringLiteral("sam2_image_encode.fp16.trt"),
                                    QStringLiteral("sam2_image_encode.trt"),
                                    QStringLiteral("sam2_image_encode.engine")};

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

QString TrtSam2::toResolveDecoderEnginePath(const QString &modelDir) const
{
    const QDir dir(modelDir);
    const QStringList candidates = {QStringLiteral("sam2_decode.fp16.trt"),
                                    QStringLiteral("sam2_decode.trt"),
                                    QStringLiteral("sam2_decode.engine")};

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

QImage TrtSam2::toImageForInference(const QImage &image) const
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

cv::Mat TrtSam2::toQImageAsBgrMat(const QImage &image) const
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

TrtSam2InferResult
TrtSam2::toInferByPrompt(const QPointF *pointPrompt, const QRectF *rectPrompt, const QString &labelName) const
{
    TrtSam2InferResult result;

    if (!isReady())
    {
        result.errorMessage = QStringLiteral("SAM2 service not initialized.");
        return result;
    }

    if (m_currentImage.isNull() || !m_currentImageBgr || m_currentImageBgr->empty())
    {
        result.errorMessage = QStringLiteral("No image loaded for SAM2 inference.");
        return result;
    }

    Sam2::PromptPoints promptPoints;
    Sam2::PromptBboxes promptBboxes;

    if (pointPrompt)
    {
        const qreal px = qBound<qreal>(0.0, pointPrompt->x(), m_currentImage.width() - 1);
        const qreal py = qBound<qreal>(0.0, pointPrompt->y(), m_currentImage.height() - 1);
        promptPoints.push_back({static_cast<float>(px), static_cast<float>(py), 1});
    }

    if (rectPrompt)
    {
        const QRectF clampedRect = toClampRect(rectPrompt->normalized());
        if (clampedRect.width() < 2.0 || clampedRect.height() < 2.0)
        {
            result.errorMessage = QStringLiteral("Prompt rectangle is too small.");
            return result;
        }

        promptBboxes.push_back({static_cast<float>(clampedRect.left()),
                                static_cast<float>(clampedRect.top()),
                                static_cast<float>(clampedRect.right()),
                                static_cast<float>(clampedRect.bottom())});
    }

    try
    {
        Sam2::Results inferResults;
        if (!promptBboxes.empty() && promptPoints.empty())
        {
            inferResults = m_sam->forward(*m_currentImageBgr, promptBboxes);
        }
        else
        {
            inferResults = m_sam->forward(*m_currentImageBgr, promptPoints, promptBboxes);
        }

        if (inferResults.empty())
        {
            result.errorMessage = QStringLiteral("SAM2 inference returned empty results.");
            return result;
        }

        bool hasValidMask = false;
        float bestScore = -1.0f;
        QRectF bestRect;
        cv::Mat bestMask;

        for (const Sam2::Result &batchResult : inferResults)
        {
            const int maskCount = static_cast<int>(batchResult.masks.size());
            for (int i = 0; i < maskCount; ++i)
            {
                const cv::Mat &mask = batchResult.masks[i];
                const QRectF rect = toBoundingRectFromMask(mask);
                if (rect.width() < 2.0 || rect.height() < 2.0)
                {
                    continue;
                }

                const float score = (i < static_cast<int>(batchResult.scores.size())) ? batchResult.scores[i] : 0.0f;
                if (!hasValidMask || score > bestScore)
                {
                    bestScore = score;
                    bestRect = rect;
                    bestMask = mask.clone();
                    hasValidMask = true;
                }
            }
        }

        if (!hasValidMask)
        {
            result.errorMessage = QStringLiteral("SAM2 returned masks but no valid bounding area.");
            return result;
        }

        TrtSam2ObjectResult objectResult;
        const QRectF clampedBestRect = toClampRect(bestRect);
        objectResult.success = true;
        objectResult.labelName = labelName;
        objectResult.score = bestScore;
        objectResult.roiData = toRectToRoiData(clampedBestRect);
        objectResult.maskContour = toLargestContour(bestMask);
        objectResult.minRectRoiData = toAxisAlignedRectRoiData(objectResult.maskContour);
        if (objectResult.minRectRoiData.size() < 8)
        {
            objectResult.minRectRoiData = objectResult.roiData;
        }

        result.success = true;
        result.objects.push_back(objectResult);
        return result;
    }
    catch (const std::exception &ex)
    {
        result.errorMessage = QStringLiteral("SAM2 inference exception: %1").arg(QString::fromStdString(ex.what()));
        return result;
    }
    catch (...)
    {
        result.errorMessage = QStringLiteral("SAM2 inference failed with unknown exception.");
        return result;
    }
}

QVector<double> TrtSam2::toRectToRoiData(const QRectF &rect)
{
    return {rect.left(), rect.top(), rect.right(), rect.top(), rect.right(), rect.bottom(), rect.left(), rect.bottom()};
}

cv::Mat TrtSam2::toMaskBinaryU8(const cv::Mat &mask)
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

QRectF TrtSam2::toBoundingRectFromMask(const cv::Mat &mask)
{
    const cv::Mat maskU8 = toMaskBinaryU8(mask);
    if (maskU8.empty())
    {
        return QRectF();
    }

    std::vector<cv::Point> nonZeroPixels;
    cv::findNonZero(maskU8, nonZeroPixels);
    if (nonZeroPixels.empty())
    {
        return QRectF();
    }

    const cv::Rect bounding = cv::boundingRect(nonZeroPixels);
    return QRectF(bounding.x, bounding.y, bounding.width, bounding.height).normalized();
}

QVector<QPointF> TrtSam2::toLargestContour(const cv::Mat &mask)
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

QVector<double> TrtSam2::toAxisAlignedRectRoiData(const QVector<QPointF> &contour)
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
