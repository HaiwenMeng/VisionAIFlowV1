#pragma once

#include "visionaiflow/annotation/Geometry.h"

#include <QString>
#include <QtGlobal>

#include <vector>

namespace visionaiflow::annotation
{
enum class AnnotationKind
{
    BoundingBox,
    Polygon,
    Line,
    Classification,
    InstanceMask,
    SemanticMask,
    AnomalyMask,
    OcrQuadrilateral
};

struct MaskRun final
{
    quint32 value{0};
    quint32 length{0};
};

struct RasterMask final
{
    int width{0};
    int height{0};
    std::vector<MaskRun> runs;
};

struct Annotation final
{
    QString annotationId;
    AnnotationKind kind{AnnotationKind::BoundingBox};
    QString labelId;
    Rect boundingBox;
    std::vector<Point> polygon;
    LineSegment line;
    QString classification;
    RasterMask mask;
    QString transcription;
    QString dictionaryId;
};

foundation::Result<void> ValidateAnnotation(const Annotation &annotation);
foundation::Result<void> ValidateRasterMask(const RasterMask &mask, bool binaryOnly);

class AnnotationStore final
{
public:
    foundation::Result<void> Save(const QString &projectRoot, const QString &imageId, const std::vector<Annotation> &annotations) const;
    foundation::Result<std::vector<Annotation>> Load(const QString &projectRoot, const QString &imageId) const;
};
}
