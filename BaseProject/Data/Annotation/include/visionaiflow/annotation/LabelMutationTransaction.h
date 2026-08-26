#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>
#include <QStringList>

#if defined(VISIONAIFLOW_ANNOTATION_LIBRARY)
#define VISIONAIFLOW_ANNOTATION_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_ANNOTATION_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::annotation
{
enum class LabelMutationAction
{
    Delete,
    Rename,
    Merge
};

struct LabelMutationRequest final
{
    LabelMutationAction action{LabelMutationAction::Delete};
    QStringList sourceLabelIds;
    QString targetLabelId;
    QString newName;
};

struct LabelMutationTransactionFile final
{
    QString transactionId;
    QString relativePath;
};

struct LabelMutationApplyResult final
{
    LabelMutationTransactionFile transaction;
    qsizetype changedLabelCount{0};
    qsizetype changedAnnotationCount{0};
};

class VISIONAIFLOW_ANNOTATION_EXPORT LabelMutationTransactionWriter final
{
public:
    foundation::Result<LabelMutationTransactionFile> Create(const QString &projectRoot, const LabelMutationRequest &request) const;
};

class VISIONAIFLOW_ANNOTATION_EXPORT LabelMutationTransactionVerifier final
{
public:
    foundation::Result<void> VerifyRollbackFiles(const QString &projectRoot, const QString &transactionRelativePath) const;
};

class VISIONAIFLOW_ANNOTATION_EXPORT LabelMutationTransactionApplier final
{
public:
    foundation::Result<LabelMutationApplyResult> Apply(const QString &projectRoot, const LabelMutationRequest &request) const;
    foundation::Result<void> Rollback(const QString &projectRoot, const QString &transactionRelativePath) const;
};
}
