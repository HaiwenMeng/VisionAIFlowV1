#include "visionaiflow/app/ProjectSession.h"

#include "visionaiflow/project_store/ProjectStore.h"

namespace visionaiflow::app
{
ProjectSession::ProjectSession(QObject *parent)
    : QObject(parent)
{
}

foundation::Result<void> ProjectSession::Open(const QString &projectRoot)
{
    if (projectRoot.isEmpty())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Project root must not be empty"));
    }

    project_store::ProjectStore store;
    const auto opened = store.Open(projectRoot);
    if (!opened.IsSuccess())
    {
        return foundation::Result<void>::Failure(opened.Failure());
    }
    if (opened.Value().type != domain::ProjectType::Detection ||
        opened.Value().modelId != QStringLiteral("detection.yolo11.grid.v1"))
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(
                foundation::ErrorCode::UnsupportedOperation,
                "Only detection.yolo11.grid.v1 projects are supported by the current application"));
    }

    const QString previousRoot = m_projectRoot;
    const project_store::ProjectDefinition previousDefinition = m_definition;
    const auto previousLabels = m_labels;
    const auto previousImages = m_images;
    m_projectRoot = projectRoot;
    m_definition = opened.Value();
    const auto refreshed = Refresh();
    if (!refreshed.IsSuccess())
    {
        m_projectRoot = previousRoot;
        m_definition = previousDefinition;
        m_labels = previousLabels;
        m_images = previousImages;
        return refreshed;
    }
    emit ProjectChanged();
    return foundation::Result<void>::Success();
}

void ProjectSession::Clear()
{
    m_projectRoot.clear();
    m_definition = {};
    m_labels.clear();
    m_images.clear();
    emit ProjectCleared();
}

bool ProjectSession::HasProject() const noexcept
{
    return !m_projectRoot.isEmpty();
}

const QString &ProjectSession::ProjectRoot() const noexcept
{
    return m_projectRoot;
}

const project_store::ProjectDefinition &ProjectSession::Definition() const noexcept
{
    return m_definition;
}

const std::vector<project_store::LabelDefinition> &ProjectSession::Labels() const noexcept
{
    return m_labels;
}

const std::vector<project_store::DatasetImage> &ProjectSession::Images() const noexcept
{
    return m_images;
}

foundation::Result<void> ProjectSession::Refresh()
{
    if (m_projectRoot.isEmpty())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidState, "No project is currently open"));
    }
    project_store::LabelStore labelStore;
    const auto loadedLabels = labelStore.Load(m_projectRoot);
    if (!loadedLabels.IsSuccess())
    {
        emit RefreshFailed(QString::fromStdString(loadedLabels.Failure().message));
        return foundation::Result<void>::Failure(loadedLabels.Failure());
    }
    project_store::DatasetIndex datasetIndex;
    const auto loadedImages = datasetIndex.Load(m_projectRoot);
    if (!loadedImages.IsSuccess())
    {
        emit RefreshFailed(QString::fromStdString(loadedImages.Failure().message));
        return foundation::Result<void>::Failure(loadedImages.Failure());
    }
    m_labels = loadedLabels.Value();
    m_images = loadedImages.Value();
    return foundation::Result<void>::Success();
}
}
