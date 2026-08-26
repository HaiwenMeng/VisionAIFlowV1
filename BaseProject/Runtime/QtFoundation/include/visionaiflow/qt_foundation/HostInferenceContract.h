#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QCborArray>
#include <QCborMap>
#include <QVector>

#include <string>
#include <vector>

namespace visionaiflow::qt_foundation
{
struct HostImageInput final
{
    QVector<float> values;
    int channels{0};
    int height{0};
    int width{0};
};

inline foundation::Result<QVector<float>> ParseHostFeatureInput(const QCborMap &request, const char *backendName)
{
    const QCborArray values = request.value(QStringLiteral("features")).toArray();
    if (values.isEmpty()) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " inference requires a non-empty features array"));
    QVector<float> features;
    features.reserve(values.size());
    for (const QCborValue &value : values)
    {
        if (!value.isDouble() && !value.isInteger()) return foundation::Result<QVector<float>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " inference features must be numeric"));
        features.append(static_cast<float>(value.toDouble()));
    }
    return foundation::Result<QVector<float>>::Success(std::move(features));
}

inline foundation::Result<HostImageInput> ParseHostImageInput(const QCborMap &request, const char *backendName)
{
    const QCborValue channelValue = request.value(QStringLiteral("channels"));
    const QCborValue heightValue = request.value(QStringLiteral("height"));
    const QCborValue widthValue = request.value(QStringLiteral("width"));
    if (!channelValue.isInteger() || !heightValue.isInteger() || !widthValue.isInteger()) return foundation::Result<HostImageInput>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 inference requires integer channels, height and width"));
    HostImageInput input;
    input.channels = static_cast<int>(channelValue.toInteger());
    input.height = static_cast<int>(heightValue.toInteger());
    input.width = static_cast<int>(widthValue.toInteger());
    if (input.channels <= 0 || input.height <= 0 || input.width <= 0) return foundation::Result<HostImageInput>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image dimensions must be positive"));
    const QCborArray values = request.value(QStringLiteral("image")).toArray();
    const qsizetype expectedElements = static_cast<qsizetype>(input.channels) * static_cast<qsizetype>(input.height) * static_cast<qsizetype>(input.width);
    if (values.size() != expectedElements) return foundation::Result<HostImageInput>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image element count does not match channels, height and width"));
    input.values.reserve(values.size());
    for (const QCborValue &value : values)
    {
        if (!value.isDouble() && !value.isInteger()) return foundation::Result<HostImageInput>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, std::string(backendName) + " YOLO11 image values must be numeric"));
        input.values.append(static_cast<float>(value.toDouble()));
    }
    return foundation::Result<HostImageInput>::Success(std::move(input));
}

inline QCborArray HostFloatsToCbor(const QVector<float> &values)
{
    QCborArray array;
    for (const float value : values) array.append(value);
    return array;
}

template <typename TDetection>
inline QCborArray HostDetectionsToCbor(const std::vector<TDetection> &detections)
{
    QCborArray array;
    for (const auto &detection : detections)
    {
        QCborMap item;
        item.insert(QStringLiteral("classIndex"), detection.classIndex);
        item.insert(QStringLiteral("score"), detection.score);
        item.insert(QStringLiteral("x1"), detection.box.x1);
        item.insert(QStringLiteral("y1"), detection.box.y1);
        item.insert(QStringLiteral("x2"), detection.box.x2);
        item.insert(QStringLiteral("y2"), detection.box.y2);
        array.append(item);
    }
    return array;
}
}
