#include "visionaiflow/domain/ProjectType.h"

#include <QHash>

namespace visionaiflow::domain
{
QString ToString(const ProjectType type)
{
    switch (type)
    {
    case ProjectType::Detection: return QStringLiteral("detection");
    case ProjectType::Classification: return QStringLiteral("classification");
    case ProjectType::InstanceSegmentation: return QStringLiteral("instance_segmentation");
    case ProjectType::SemanticSegmentation: return QStringLiteral("semantic_segmentation");
    case ProjectType::AnomalyDetection: return QStringLiteral("anomaly_detection");
    case ProjectType::LineDetection: return QStringLiteral("line_detection");
    case ProjectType::OcrDetection: return QStringLiteral("ocr_detection");
    case ProjectType::OcrRecognition: return QStringLiteral("ocr_recognition");
    case ProjectType::OcrPipeline: return QStringLiteral("ocr_pipeline");
    }
    return {};
}

QString ToString(const ClassificationMode mode)
{
    switch (mode)
    {
    case ClassificationMode::NotApplicable: return {};
    case ClassificationMode::SingleLabel: return QStringLiteral("single_label");
    case ClassificationMode::MultiLabel: return QStringLiteral("multi_label");
    }
    return {};
}

foundation::Result<ProjectType> ProjectTypeFromString(const QString &value)
{
    static const QHash<QString, ProjectType> values{{QStringLiteral("detection"), ProjectType::Detection}, {QStringLiteral("classification"), ProjectType::Classification}, {QStringLiteral("instance_segmentation"), ProjectType::InstanceSegmentation}, {QStringLiteral("semantic_segmentation"), ProjectType::SemanticSegmentation}, {QStringLiteral("anomaly_detection"), ProjectType::AnomalyDetection}, {QStringLiteral("line_detection"), ProjectType::LineDetection}, {QStringLiteral("ocr_detection"), ProjectType::OcrDetection}, {QStringLiteral("ocr_recognition"), ProjectType::OcrRecognition}, {QStringLiteral("ocr_pipeline"), ProjectType::OcrPipeline}};
    if (!values.contains(value)) return foundation::Result<ProjectType>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Unknown projectType"));
    return foundation::Result<ProjectType>::Success(values.value(value));
}

foundation::Result<ClassificationMode> ClassificationModeFromString(const QString &value)
{
    if (value.isEmpty()) return foundation::Result<ClassificationMode>::Success(ClassificationMode::NotApplicable);
    if (value == QStringLiteral("single_label")) return foundation::Result<ClassificationMode>::Success(ClassificationMode::SingleLabel);
    if (value == QStringLiteral("multi_label")) return foundation::Result<ClassificationMode>::Success(ClassificationMode::MultiLabel);
    return foundation::Result<ClassificationMode>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Unknown classificationMode"));
}
}
