#ifndef EAGLE_CORE_LOGGER_H
#define EAGLE_CORE_LOGGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QLoggingCategory>

namespace Eagle {
namespace Core {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

/**
 * @brief 日志管理器
 */
class Logger {
public:
    static void initialize(const QString& logDir = QString(), LogLevel level = LogLevel::Info);
    static void shutdown();
    
    static void setLogLevel(LogLevel level);
    static LogLevel logLevel();
    
    static void log(LogLevel level, const QString& category, const QString& message);
    static void debug(const QString& category, const QString& message);
    static void info(const QString& category, const QString& message);
    static void warning(const QString& category, const QString& message);
    static void error(const QString& category, const QString& message);
    static void critical(const QString& category, const QString& message);
};

// 便捷宏
#define EAGLE_DEBUG(category, message) Eagle::Core::Logger::debug(category, message)
#define EAGLE_INFO(category, message) Eagle::Core::Logger::info(category, message)
#define EAGLE_WARNING(category, message) Eagle::Core::Logger::warning(category, message)
#define EAGLE_ERROR(category, message) Eagle::Core::Logger::error(category, message)
#define EAGLE_CRITICAL(category, message) Eagle::Core::Logger::critical(category, message)

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_LOGGER_H
