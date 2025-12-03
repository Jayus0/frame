#include "eagle/core/AuditLog.h"
#include "AuditLog_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

namespace Eagle {
namespace Core {

void AuditLogManager::Private::writeToFile(const AuditLogEntry& entry)
{
    if (logFilePath.isEmpty()) {
        return;
    }
    
    QFile file(logFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        Logger::warning("AuditLog", QString("无法写入审计日志文件: %1").arg(logFilePath));
        return;
    }
    
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    
    QJsonObject entryObj;
    entryObj["timestamp"] = entry.timestamp.toString(Qt::ISODate);
    entryObj["user_id"] = entry.userId;
    entryObj["action"] = entry.action;
    entryObj["resource"] = entry.resource;
    entryObj["level"] = static_cast<int>(entry.level);
    entryObj["description"] = entry.description;
    entryObj["success"] = entry.success;
    entryObj["error_message"] = entry.errorMessage;
    
    QJsonObject metaObj;
    for (auto it = entry.metadata.begin(); it != entry.metadata.end(); ++it) {
        metaObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    entryObj["metadata"] = metaObj;
    
    QJsonDocument doc(entryObj);
    stream << doc.toJson(QJsonDocument::Compact) << "\n";
    stream.flush();
    file.close();
}

AuditLogManager::AuditLogManager(QObject* parent)
    : QObject(parent)
    , d(new AuditLogManager::Private)
{
    // 设置默认日志文件路径
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/audit";
    QDir dir;
    if (!dir.exists(logDir)) {
        dir.mkpath(logDir);
    }
    d->logFilePath = logDir + "/audit_" + QDateTime::currentDateTime().toString("yyyyMMdd") + ".log";
    
    Logger::info("AuditLog", QString("审计日志管理器初始化，日志文件: %1").arg(d->logFilePath));
}

AuditLogManager::~AuditLogManager()
{
    flush();
    delete d;
}

void AuditLogManager::log(const AuditLogEntry& entry)
{
    if (!entry.isValid()) {
        Logger::warning("AuditLog", "无效的审计日志条目");
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 添加到内存列表
    d->entries.append(entry);
    
    // 如果超过最大条目数，移除最旧的
    while (d->entries.size() > d->maxEntries) {
        d->entries.removeFirst();
    }
    
    // 写入文件
    d->writeToFile(entry);
    
    // 如果启用自动刷新，立即刷新
    if (d->autoFlush) {
        // 文件已经关闭，不需要额外刷新
    }
    
    emit logEntryAdded(entry);
    
    // 记录到系统日志
    QString logMsg = QString("[%1] %2: %3 - %4")
        .arg(entry.userId, entry.action, entry.resource, entry.description);
    if (!entry.success) {
        Logger::error("AuditLog", logMsg);
    } else {
        Logger::info("AuditLog", logMsg);
    }
}

void AuditLogManager::log(const QString& userId, const QString& action, const QString& resource,
                          AuditLevel level, bool success, const QString& errorMessage,
                          const QVariantMap& metadata)
{
    AuditLogEntry entry;
    entry.userId = userId;
    entry.action = action;
    entry.resource = resource;
    entry.level = level;
    entry.success = success;
    entry.errorMessage = errorMessage;
    entry.metadata = metadata;
    entry.description = QString("%1 %2").arg(action, resource);
    
    log(entry);
}

QList<AuditLogEntry> AuditLogManager::query(const QString& userId, const QString& action,
                                            const QDateTime& startTime, const QDateTime& endTime,
                                            AuditLevel minLevel) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<AuditLogEntry> results;
    
    for (const AuditLogEntry& entry : d->entries) {
        // 过滤用户
        if (!userId.isEmpty() && entry.userId != userId) {
            continue;
        }
        
        // 过滤操作
        if (!action.isEmpty() && entry.action != action) {
            continue;
        }
        
        // 过滤时间
        if (startTime.isValid() && entry.timestamp < startTime) {
            continue;
        }
        if (endTime.isValid() && entry.timestamp > endTime) {
            continue;
        }
        
        // 过滤级别
        if (entry.level < minLevel) {
            continue;
        }
        
        results.append(entry);
    }
    
    return results;
}

void AuditLogManager::setLogFile(const QString& filePath)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->logFilePath = filePath;
    Logger::info("AuditLog", QString("设置审计日志文件: %1").arg(filePath));
}

void AuditLogManager::setMaxEntries(int maxEntries)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxEntries = maxEntries;
    Logger::info("AuditLog", QString("设置最大审计日志条目数: %1").arg(maxEntries));
}

void AuditLogManager::setAutoFlush(bool autoFlush)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->autoFlush = autoFlush;
}

void AuditLogManager::flush()
{
    // 文件写入是同步的，不需要额外刷新
    Logger::debug("AuditLog", "审计日志已刷新");
}

int AuditLogManager::getEntryCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->entries.size();
}

int AuditLogManager::getEntryCount(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    int count = 0;
    for (const AuditLogEntry& entry : d->entries) {
        if (entry.userId == userId) {
            count++;
        }
    }
    return count;
}

int AuditLogManager::getFailureCount(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    int count = 0;
    for (const AuditLogEntry& entry : d->entries) {
        if (entry.userId == userId && !entry.success) {
            count++;
        }
    }
    return count;
}

} // namespace Core
} // namespace Eagle
