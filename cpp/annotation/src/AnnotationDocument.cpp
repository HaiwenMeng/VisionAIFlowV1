#include "visionaiflow/annotation/AnnotationDocument.h"

#include <algorithm>

namespace visionaiflow::annotation
{
const std::vector<Annotation> &AnnotationDocument::Annotations() const noexcept { return m_annotations; }
bool AnnotationDocument::CanUndo() const noexcept { return !m_undo.empty(); }
bool AnnotationDocument::CanRedo() const noexcept { return !m_redo.empty(); }
bool AnnotationDocument::IsDirty() const noexcept { return m_revision != m_savedRevision; }

foundation::Result<void> AnnotationDocument::ValidateCollection(const std::vector<Annotation> &annotations) const
{
    for (const Annotation &annotation : annotations)
    {
        const auto validation = ValidateAnnotation(annotation);
        if (!validation.IsSuccess()) return validation;
    }
    for (size_t left = 0; left < annotations.size(); ++left)
    {
        for (size_t right = left + 1U; right < annotations.size(); ++right)
        {
            if (annotations[left].annotationId == annotations[right].annotationId) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "Annotation document contains duplicate annotation ids"));
        }
    }
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationDocument::Commit(std::vector<Annotation> next)
{
    const auto validation = ValidateCollection(next);
    if (!validation.IsSuccess()) return validation;
    m_undo.push_back({m_annotations, m_revision});
    m_annotations = std::move(next);
    m_redo.clear();
    m_revision = m_nextRevision;
    ++m_nextRevision;
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationDocument::Add(const Annotation &annotation)
{
    auto next = m_annotations;
    next.push_back(annotation);
    return Commit(std::move(next));
}

foundation::Result<void> AnnotationDocument::Replace(const QString &annotationId, const Annotation &replacement)
{
    if (annotationId.isEmpty() || annotationId != replacement.annotationId) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Replacement annotation id must match the target id"));
    auto next = m_annotations;
    const auto iterator = std::find_if(next.begin(), next.end(), [&annotationId](const Annotation &annotation) { return annotation.annotationId == annotationId; });
    if (iterator == next.end()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation to replace does not exist"));
    *iterator = replacement;
    return Commit(std::move(next));
}

foundation::Result<void> AnnotationDocument::Remove(const QString &annotationId)
{
    if (annotationId.isEmpty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation id must not be empty"));
    auto next = m_annotations;
    const auto iterator = std::find_if(next.begin(), next.end(), [&annotationId](const Annotation &annotation) { return annotation.annotationId == annotationId; });
    if (iterator == next.end()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Annotation to remove does not exist"));
    next.erase(iterator);
    return Commit(std::move(next));
}

foundation::Result<void> AnnotationDocument::Undo()
{
    if (m_undo.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "No annotation edit is available to undo"));
    m_redo.push_back({m_annotations, m_revision});
    m_annotations = std::move(m_undo.back().annotations);
    m_revision = m_undo.back().revision;
    m_undo.pop_back();
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationDocument::Redo()
{
    if (m_redo.empty()) return foundation::Result<void>::Failure(foundation::Error::Create(foundation::ErrorCode::InvalidState, "No annotation edit is available to redo"));
    m_undo.push_back({m_annotations, m_revision});
    m_annotations = std::move(m_redo.back().annotations);
    m_revision = m_redo.back().revision;
    m_redo.pop_back();
    return foundation::Result<void>::Success();
}

foundation::Result<void> AnnotationDocument::Reset(std::vector<Annotation> annotations)
{
    const auto validation = ValidateCollection(annotations);
    if (!validation.IsSuccess()) return validation;
    m_annotations = std::move(annotations);
    m_undo.clear();
    m_redo.clear();
    m_revision = m_nextRevision;
    ++m_nextRevision;
    m_savedRevision = m_revision;
    return foundation::Result<void>::Success();
}

void AnnotationDocument::MarkSaved() noexcept
{
    m_savedRevision = m_revision;
}
}
