#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

namespace visionaiflow::project_store
{
class ProjectLock final
{
public:
    ProjectLock() = default;
    ~ProjectLock();
    ProjectLock(const ProjectLock &) = delete;
    ProjectLock &operator=(const ProjectLock &) = delete;

    foundation::Result<void> Acquire(const QString &projectRoot, const QString &productVersion);
    foundation::Result<void> Release();
    [[nodiscard]] bool IsHeld() const noexcept;

private:
    void *m_handle = nullptr;
    QString m_path;
};
}
