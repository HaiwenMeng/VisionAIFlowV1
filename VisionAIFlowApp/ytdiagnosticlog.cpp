#include "ytdiagnosticlog.h"

#ifdef YT_ENABLE_LOG
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <cstdlib>
#include <cstdio>

namespace
{
QMutex &ytLogMutex()
{
    static QMutex mutex;
    return mutex;
}

QString ytLogFilePath()
{
    QString basePath=QCoreApplication::applicationDirPath();
    if(basePath.isEmpty())
    {
        basePath=QDir::currentPath();
    }
    return basePath+"/YtYoloProTrainGrayV2.log";
}

const char *ytMessageTypeName(QtMsgType type)
{
    switch(type)
    {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "CRITICAL";
    case QtFatalMsg:
        return "FATAL";
    }
    return "LOG";
}

void ytMessageHandler(QtMsgType type,const QMessageLogContext &context,const QString &message)
{
    QMutexLocker locker(&ytLogMutex());
    QFile file(ytLogFilePath());
    if(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setCodec("utf-8");
        out<<QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
           <<" ["<<ytMessageTypeName(type)<<"] "
           <<message;
        if(context.file)
        {
            out<<" ("<<context.file<<":"<<context.line<<")";
        }
        out<<"\n";
        file.close();
    }

    QByteArray localMessage=message.toLocal8Bit();
    std::fprintf(stderr,"%s: %s\n",ytMessageTypeName(type),localMessage.constData());
    std::fflush(stderr);

    if(type==QtFatalMsg)
    {
        std::abort();
    }
}
}
#endif

void ytInstallDiagnosticLogHandler()
{
#ifdef YT_ENABLE_LOG
    qInstallMessageHandler(ytMessageHandler);
#endif
}
