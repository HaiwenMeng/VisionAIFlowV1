#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

#if defined(VISIONAIFLOW_QT_FOUNDATION_LIBRARY)
#define VISIONAIFLOW_QT_FOUNDATION_EXPORT __declspec(dllexport)
#else
#define VISIONAIFLOW_QT_FOUNDATION_EXPORT __declspec(dllimport)
#endif

namespace visionaiflow::qt_foundation
{
class VISIONAIFLOW_QT_FOUNDATION_EXPORT StructuredLogger final
{
public:
    static foundation::Result<void> Initialize(const QString &logDirectory, const QString &processName);
    static void Info(const QString &module, const QString &message, const QString &jobId = {}, const QString &requestId = {});
    static void Error(const QString &module, const foundation::Error &error, const QString &jobId = {}, const QString &requestId = {});
};
}
