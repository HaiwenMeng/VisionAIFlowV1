#include "visionaiflow/annotation/LabelImpactAnalyzer.h"

#include "visionaiflow/annotation/AnnotationStore.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace visionaiflow::annotation
{
foundation::Result<std::vector<LabelImpact>> LabelImpactAnalyzer::Analyze(const QString &projectRoot, const QStringList &labelIds) const
{
    if (projectRoot.isEmpty()) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root must not be empty when analyzing label impact"));
    if (labelIds.isEmpty()) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "At least one label id is required for label impact analysis"));
    QSet<QString> requestedIds;
    std::vector<LabelImpact> impacts;
    impacts.reserve(static_cast<size_t>(labelIds.size()));
    for (const QString &labelId : labelIds)
    {
        if (labelId.isEmpty()) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label id must not be empty when analyzing label impact"));
        if (requestedIds.contains(labelId)) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Label impact analysis requires unique label ids"));
        requestedIds.insert(labelId);
        impacts.push_back({labelId, 0, {}});
    }
    const QDir annotationDirectory(QDir(projectRoot).filePath(QStringLiteral("data/annotations")));
    if (!annotationDirectory.exists()) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Project annotation directory is missing"));
    const QFileInfoList files = annotationDirectory.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::Readable, QDir::Name);
    AnnotationStore store;
    for (const QFileInfo &file : files)
    {
        const QString imageId = file.completeBaseName();
        if (imageId.isEmpty()) return foundation::Result<std::vector<LabelImpact>>::Failure(foundation::Error::Create(foundation::ErrorCode::ProtocolViolation, "Annotation file name does not contain an image id"));
        const auto loaded = store.Load(projectRoot, imageId);
        if (!loaded.IsSuccess()) return foundation::Result<std::vector<LabelImpact>>::Failure(loaded.Failure());
        for (LabelImpact &impact : impacts)
        {
            qsizetype count = 0;
            for (const Annotation &annotation : loaded.Value())
            {
                if (annotation.labelId == impact.labelId) ++count;
            }
            if (count > 0)
            {
                impact.totalAnnotationCount += count;
                impact.images.push_back({imageId, count});
            }
        }
    }
    return foundation::Result<std::vector<LabelImpact>>::Success(std::move(impacts));
}
}
