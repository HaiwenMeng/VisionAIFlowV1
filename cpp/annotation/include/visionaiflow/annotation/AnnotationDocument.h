#pragma once

#include "visionaiflow/annotation/AnnotationStore.h"

#include <vector>

namespace visionaiflow::annotation
{
class AnnotationDocument final
{
public:
    const std::vector<Annotation> &Annotations() const noexcept;
    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    bool IsDirty() const noexcept;
    foundation::Result<void> Add(const Annotation &annotation);
    foundation::Result<void> Replace(const QString &annotationId, const Annotation &replacement);
    foundation::Result<void> Remove(const QString &annotationId);
    foundation::Result<void> Undo();
    foundation::Result<void> Redo();
    foundation::Result<void> Reset(std::vector<Annotation> annotations);
    void MarkSaved() noexcept;

private:
    struct HistoryEntry final
    {
        std::vector<Annotation> annotations;
        size_t revision{0};
    };

    foundation::Result<void> ValidateCollection(const std::vector<Annotation> &annotations) const;
    foundation::Result<void> Commit(std::vector<Annotation> next);
    std::vector<Annotation> m_annotations;
    std::vector<HistoryEntry> m_undo;
    std::vector<HistoryEntry> m_redo;
    size_t m_revision{0};
    size_t m_nextRevision{1};
    size_t m_savedRevision{0};
};
}
