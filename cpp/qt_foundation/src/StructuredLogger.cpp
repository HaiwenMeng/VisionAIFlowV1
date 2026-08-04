#include "visionaiflow/qt_foundation/StructuredLogger.h"

#include <mutex>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace visionaiflow::qt_foundation
{
namespace
{
std::shared_ptr<spdlog::logger> g_logger;
std::mutex g_loggerMutex;

void Write(const spdlog::level::level_enum level, const QString &module, const QString &message, const QString &jobId, const QString &requestId, const QString &errorCode)
{
    std::scoped_lock lock(g_loggerMutex);
    if (!g_logger)
    {
        return;
    }
    QJsonObject record;
    record.insert(QStringLiteral("process"), QString::fromStdString(g_logger->name()));
    record.insert(QStringLiteral("module"), module);
    record.insert(QStringLiteral("jobId"), jobId);
    record.insert(QStringLiteral("requestId"), requestId);
    record.insert(QStringLiteral("errorCode"), errorCode);
    record.insert(QStringLiteral("message"), message);
    g_logger->log(level, "{}", QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString());
    g_logger->flush();
}
}

foundation::Result<void> StructuredLogger::Initialize(const QString &logDirectory, const QString &processName)
{
    if (logDirectory.isEmpty() || processName.isEmpty())
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::InvalidArgument, "Structured logger requires non-empty log directory and process name"));
    }
    try
    {
        QDir directory(logDirectory);
        if (!directory.mkpath(QStringLiteral(".")))
        {
            return foundation::Result<void>::Failure(
                foundation::Error::Create(foundation::ErrorCode::IoFailure, "Failed to create structured log directory"));
        }
        const std::string path = directory.filePath(processName + QStringLiteral(".log")).toLocal8Bit().toStdString();
        std::scoped_lock lock(g_loggerMutex);
        spdlog::drop(processName.toStdString());
        g_logger = spdlog::rotating_logger_mt(processName.toStdString(), path, 10U * 1024U * 1024U, 10U);
        g_logger->set_pattern("%Y-%m-%dT%H:%M:%S.%eZ [%P:%t] %v");
        g_logger->flush();
        return foundation::Result<void>::Success();
    }
    catch (const std::exception &exception)
    {
        return foundation::Result<void>::Failure(
            foundation::Error::Create(foundation::ErrorCode::IoFailure, std::string("Failed to initialize rotating logger: ") + exception.what()));
    }
}

void StructuredLogger::Info(const QString &module, const QString &message, const QString &jobId, const QString &requestId)
{
    Write(spdlog::level::info, module, message, jobId, requestId, {});
}

void StructuredLogger::Error(const QString &module, const foundation::Error &error, const QString &jobId, const QString &requestId)
{
    Write(spdlog::level::err, module, QString::fromStdString(error.message), jobId, requestId, QString::fromLatin1(foundation::ToString(error.code)));
}
}
