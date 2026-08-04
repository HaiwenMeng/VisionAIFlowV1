#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace visionaiflow::annotation
{
struct LabelUsageInImage final
{
    QString imageId;
    qsizetype annotationCount{0};
};

struct LabelImpact final
{
    QString labelId;
    qsizetype totalAnnotationCount{0};
    std::vector<LabelUsageInImage> images;
};

class LabelImpactAnalyzer final
{
public:
    foundation::Result<std::vector<LabelImpact>> Analyze(const QString &projectRoot, const QStringList &labelIds) const;
};
}
