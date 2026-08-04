#pragma once

#include "visionaiflow/foundation/Result.h"

#include <QString>

namespace visionaiflow::qt_foundation
{
class StructuredLogger final
{
public:
    static foundation::Result<void> Initialize(const QString &logDirectory, const QString &processName);
    static void Info(const QString &module, const QString &message, const QString &jobId = {}, const QString &requestId = {});
    static void Error(const QString &module, const foundation::Error &error, const QString &jobId = {}, const QString &requestId = {});
};
}
