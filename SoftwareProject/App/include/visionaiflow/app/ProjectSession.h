#pragma once

#include "visionaiflow/foundation/Result.h"
#include "visionaiflow/project_store/DatasetIndex.h"
#include "visionaiflow/project_store/LabelStore.h"
#include "visionaiflow/project_store/ProjectDefinition.h"

#include <QObject>
#include <QString>

#include <vector>

namespace visionaiflow::app
{
class ProjectSession final : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSession(QObject *parent = nullptr);

    foundation::Result<void> Open(const QString &projectRoot);
    void Clear();

    [[nodiscard]] bool HasProject() const noexcept;
    [[nodiscard]] const QString &ProjectRoot() const noexcept;
    [[nodiscard]] const project_store::ProjectDefinition &Definition() const noexcept;
    [[nodiscard]] const std::vector<project_store::LabelDefinition> &Labels() const noexcept;
    [[nodiscard]] const std::vector<project_store::DatasetImage> &Images() const noexcept;

    foundation::Result<void> Refresh();

signals:
    void ProjectChanged();
    void ProjectCleared();
    void RefreshFailed(const QString &errorMessage);

private:
    QString m_projectRoot;
    project_store::ProjectDefinition m_definition;
    std::vector<project_store::LabelDefinition> m_labels;
    std::vector<project_store::DatasetImage> m_images;
};
}
