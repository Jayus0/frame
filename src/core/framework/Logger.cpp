#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QMutex>
#include <QtCore/QLoggingCategory>

namespace Eagle {
namespace Core {

static LogLevel s_logLevel = LogLevel::Info;
static QString s_logDir;
static QFile* s_logFile = nullptr;
static QTextStream* s_logStream = nullptr;
static QMutex s_logMutex;

Q_LOGGING_CATEGORY(eagleCore, "eagle.core")
Q_LOGGING_CATEGORY(eaglePlugin, "eagle.plugin")
Q_LOGGING_CATEGORY(eagleService, "eagle.service")

void Logger::initialize(const QString& logDir, LogLevel level)
{
    QMutexLocker locker(&s_logMutex);
    
    s_logLevel = level;
    
    if (logDir.isEmpty()) {
        s_logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    } else {
        s_logDir = logDir;
    }
    
    QDir dir;
    if (!dir.exists(s_logDir)) {
        dir.mkpath(s_logDir);
    }
    
    QString logFilePath = s_logDir + "/eagle_" + 
                         QDateTime::currentDateTime().toString("yyyyMMdd") + ".log";
    
    if (s_logFile) {
        s_logFile->close();
        delete s_logStream;
        delete s_logFile;
    }
    
    s_logFile = new QFile(logFilePath);
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        s_logStream = new QTextStream(s_logFile);
        s_logStream->setCodec("UTF-8");
    }
}

void Logger::shutdown()
{
    QMutexLocker locker(&s_logMutex);
    
    if (s_logStream) {
        s_logStream->flush();
        delete s_logStream;
        s_logStream = nullptr;
    }
    
    if (s_logFile) {
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&s_logMutex);
    s_logLevel = level;
}

LogLevel Logger::logLevel()
{
    QMutexLocker locker(&s_logMutex);
    return s_logLevel;
}

void Logger::log(LogLevel level, const QString& category, const QString& message)
{
    if (level < s_logLevel) {
        return;
    }
    
    QMutexLocker locker(&s_logMutex);
    
    QString levelStr;
    switch (level) {
    case LogLevel::Debug:
        levelStr = "DEBUG";
        break;
    case LogLevel::Info:
        levelStr = "INFO";
        break;
    case LogLevel::Warning:
        levelStr = "WARN";
        break;
    case LogLevel::Error:
        levelStr = "ERROR";
        break;
    case LogLevel::Critical:
        levelStr = "CRITICAL";
        break;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage = QString("[%1] [%2] [%3] %4")
        .arg(timestamp, levelStr, category, message);
    
    // 输出到控制台
    QTextStream console(stdout);
    console << logMessage << Qt::endl;
    
    // 输出到文件
    if (s_logStream) {
        *s_logStream << logMessage << Qt::endl;
        s_logStream->flush();
    }
    
    // 使用Qt日志系统
    switch (level) {
    case LogLevel::Debug:
        qCDebug(eagleCore) << message;
        break;
    case LogLevel::Info:
        qCInfo(eagleCore) << message;
        break;
    case LogLevel::Warning:
        qCWarning(eagleCore) << message;
        break;
    case LogLevel::Error:
    case LogLevel::Critical:
        qCCritical(eagleCore) << message;
        break;
    }
}

void Logger::debug(const QString& category, const QString& message)
{
    log(LogLevel::Debug, category, message);
}

void Logger::info(const QString& category, const QString& message)
{
    log(LogLevel::Info, category, message);
}

void Logger::warning(const QString& category, const QString& message)
{
    log(LogLevel::Warning, category, message);
}

void Logger::error(const QString& category, const QString& message)
{
    log(LogLevel::Error, category, message);
}

void Logger::critical(const QString& category, const QString& message)
{
    log(LogLevel::Critical, category, message);
}

} // namespace Core
} // namespace Eagle
