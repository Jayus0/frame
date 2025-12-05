#include "eagle/core/AuditLog.h"
#include "AuditLog_p.h"
#include "eagle/core/Logger.h"
#include "eagle/core/AlertSystem.h"
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>
#include <QtCore/QCryptographicHash>
#include <algorithm>

namespace Eagle {
namespace Core {

void AuditLogManager::Private::writeToFile(const AuditLogEntry& entry)
{
    if (logFilePath.isEmpty()) {
        return;
    }
    
    // 检查是否需要轮转
    if (rotationEnabled && shouldRotate()) {
        performRotation();
        // 轮转后重置哈希链
        lastEntryHash = QString();
    }
    
    QFile file(logFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        Logger::warning("AuditLog", QString("无法写入审计日志文件: %1").arg(logFilePath));
        return;
    }
    
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    
    // 创建可修改的条目副本，用于添加哈希
    AuditLogEntry entryWithHash = entry;
    
    // 如果启用防篡改，添加前一条日志的哈希值
    if (tamperProtectionEnabled) {
        entryWithHash.previousHash = lastEntryHash;
        // 计算当前条目的哈希值
        entryWithHash.entryHash = entryWithHash.calculateHash();
        // 更新最后一条日志的哈希值
        lastEntryHash = entryWithHash.entryHash;
    }
    
    QJsonObject entryObj;
    entryObj["timestamp"] = entryWithHash.timestamp.toString(Qt::ISODate);
    entryObj["user_id"] = entryWithHash.userId;
    entryObj["action"] = entryWithHash.action;
    entryObj["resource"] = entryWithHash.resource;
    entryObj["level"] = static_cast<int>(entryWithHash.level);
    entryObj["description"] = entryWithHash.description;
    entryObj["success"] = entryWithHash.success;
    entryObj["error_message"] = entryWithHash.errorMessage;
    
    // 添加防篡改字段
    if (tamperProtectionEnabled) {
        entryObj["previous_hash"] = entryWithHash.previousHash;
        entryObj["entry_hash"] = entryWithHash.entryHash;
    }
    
    QJsonObject metaObj;
    for (auto it = entryWithHash.metadata.begin(); it != entryWithHash.metadata.end(); ++it) {
        metaObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    entryObj["metadata"] = metaObj;
    
    QJsonDocument doc(entryObj);
    stream << doc.toJson(QJsonDocument::Compact) << "\n";
    stream.flush();
    file.close();
}

bool AuditLogManager::Private::shouldRotate() const
{
    if (!rotationEnabled || rotationStrategy == RotationStrategy::None) {
        return false;
    }
    
    // 按大小轮转
    if (rotationStrategy == RotationStrategy::Size) {
        QFileInfo fileInfo(logFilePath);
        if (fileInfo.exists() && fileInfo.size() >= maxFileSizeBytes) {
            return true;
        }
    }
    
    // 按时间轮转
    QDateTime currentRotationDate = getCurrentRotationDate();
    if (!lastRotationDate.isValid() || currentRotationDate > lastRotationDate) {
        return true;
    }
    
    return false;
}

void AuditLogManager::Private::performRotation()
{
    if (logFilePath.isEmpty()) {
        return;
    }
    
    QFileInfo currentFile(logFilePath);
    if (!currentFile.exists()) {
        // 文件不存在，不需要轮转
        return;
    }
    
    // 生成新的日志文件名（带时间戳）
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp;
    switch (rotationStrategy) {
        case RotationStrategy::Size:
            timestamp = now.toString("yyyyMMdd_HHmmss");
            break;
        case RotationStrategy::Daily:
            timestamp = now.toString("yyyyMMdd");
            break;
        case RotationStrategy::Weekly:
            // 使用周的开始日期（周一）
            {
                int daysToMonday = (now.date().dayOfWeek() + 5) % 7;
                QDate weekStart = now.date().addDays(-daysToMonday);
                timestamp = weekStart.toString("yyyyMMdd");
            }
            break;
        case RotationStrategy::Monthly:
            timestamp = now.toString("yyyyMM");
            break;
        default:
            timestamp = now.toString("yyyyMMdd");
            break;
    }
    
    QString rotatedFileName = logFileBaseName + "_" + timestamp + ".log";
    QString rotatedFilePath = logDir + "/" + rotatedFileName;
    
    // 如果文件已存在，添加序号
    int counter = 1;
    while (QFile::exists(rotatedFilePath)) {
        rotatedFileName = logFileBaseName + "_" + timestamp + "_" + QString::number(counter) + ".log";
        rotatedFilePath = logDir + "/" + rotatedFileName;
        counter++;
    }
    
    // 重命名当前文件
    if (QFile::rename(logFilePath, rotatedFilePath)) {
        Logger::info("AuditLog", QString("日志文件轮转: %1 -> %2").arg(logFilePath, rotatedFileName));
    } else {
        Logger::warning("AuditLog", QString("日志文件轮转失败: %1").arg(logFilePath));
    }
    
    // 更新当前日志文件路径
    QString newFileName = generateLogFileName(now);
    logFilePath = logDir + "/" + newFileName;
    lastRotationDate = getCurrentRotationDate();
    
    // 清理旧文件
    cleanupOldFiles();
}

void AuditLogManager::Private::cleanupOldFiles()
{
    if (maxFiles <= 0) {
        return;  // 不限制文件数量
    }
    
    QDir dir(logDir);
    if (!dir.exists()) {
        return;
    }
    
    // 获取所有日志文件
    QStringList filters;
    filters << logFileBaseName + "_*.log";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);
    
    // 如果文件数超过限制，删除最旧的
    if (files.size() > maxFiles) {
        int toDelete = files.size() - maxFiles;
        for (int i = 0; i < toDelete; ++i) {
            QFile::remove(files[i].absoluteFilePath());
            Logger::info("AuditLog", QString("删除旧日志文件: %1").arg(files[i].fileName()));
        }
    }
}

QString AuditLogManager::Private::generateLogFileName(const QDateTime& date) const
{
    QString suffix;
    switch (rotationStrategy) {
        case RotationStrategy::Daily:
            suffix = date.toString("yyyyMMdd");
            break;
        case RotationStrategy::Weekly:
            {
                int daysToMonday = (date.date().dayOfWeek() + 5) % 7;
                QDate weekStart = date.date().addDays(-daysToMonday);
                suffix = weekStart.toString("yyyyMMdd");
            }
            break;
        case RotationStrategy::Monthly:
            suffix = date.toString("yyyyMM");
            break;
        default:
            suffix = date.toString("yyyyMMdd");
            break;
    }
    return logFileBaseName + "_" + suffix + ".log";
}

QDateTime AuditLogManager::Private::getCurrentRotationDate() const
{
    QDateTime now = QDateTime::currentDateTime();
    
    switch (rotationStrategy) {
        case RotationStrategy::Daily:
            return QDateTime(now.date(), QTime(0, 0, 0));
        case RotationStrategy::Weekly:
            {
                int daysToMonday = (now.date().dayOfWeek() + 5) % 7;
                QDate weekStart = now.date().addDays(-daysToMonday);
                return QDateTime(weekStart, QTime(0, 0, 0));
            }
        case RotationStrategy::Monthly:
            {
                QDate monthStart(now.date().year(), now.date().month(), 1);
                return QDateTime(monthStart, QTime(0, 0, 0));
            }
        default:
            return now;
    }
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
    
    d->logDir = logDir;
    d->logFileBaseName = "audit";
    d->logFilePath = d->generateLogFileName(QDateTime::currentDateTime());
    d->logFilePath = logDir + "/" + d->logFilePath;
    d->lastRotationDate = d->getCurrentRotationDate();
    d->tamperProtectionEnabled = true;  // 默认启用防篡改
    d->lastEntryHash = QString();  // 初始哈希为空
    
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
    
    // 创建带哈希的条目副本
    AuditLogEntry entryWithHash = entry;
    if (d->tamperProtectionEnabled) {
        entryWithHash.previousHash = d->lastEntryHash;
        entryWithHash.entryHash = entryWithHash.calculateHash();
        d->lastEntryHash = entryWithHash.entryHash;
    }
    
    // 添加到内存列表
    d->entries.append(entryWithHash);
    
    // 如果超过最大条目数，移除最旧的
    while (d->entries.size() > d->maxEntries) {
        d->entries.removeFirst();
    }
    
    // 写入文件
    locker.unlock();
    d->writeToFile(entryWithHash);
    locker.relock();
    
    // 如果启用自动刷新，立即刷新
    if (d->autoFlush) {
        // 文件已经关闭，不需要额外刷新
    }
    
    emit logEntryAdded(entryWithHash);
    
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

// 日志轮转配置
void AuditLogManager::setRotationEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->rotationEnabled = enabled;
    Logger::info("AuditLog", QString("日志轮转%1").arg(enabled ? "启用" : "禁用"));
}

bool AuditLogManager::isRotationEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->rotationEnabled;
}

void AuditLogManager::setRotationStrategy(RotationStrategy strategy)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->rotationStrategy = strategy;
    d->lastRotationDate = d->getCurrentRotationDate();
    
    // 更新当前日志文件路径
    d->logFilePath = d->logDir + "/" + d->generateLogFileName(QDateTime::currentDateTime());
    
    Logger::info("AuditLog", QString("设置日志轮转策略: %1").arg(static_cast<int>(strategy)));
}

RotationStrategy AuditLogManager::getRotationStrategy() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->rotationStrategy;
}

void AuditLogManager::setMaxFileSize(qint64 maxSizeBytes)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxFileSizeBytes = qMax(1024LL, maxSizeBytes);  // 最小1KB
    Logger::info("AuditLog", QString("设置最大日志文件大小: %1字节").arg(d->maxFileSizeBytes));
}

qint64 AuditLogManager::getMaxFileSize() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->maxFileSizeBytes;
}

void AuditLogManager::setMaxFiles(int maxFiles)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxFiles = qMax(0, maxFiles);  // 0表示不限制
    Logger::info("AuditLog", QString("设置最大日志文件数: %1").arg(d->maxFiles));
    
    // 立即清理旧文件
    locker.unlock();
    d->cleanupOldFiles();
}

int AuditLogManager::getMaxFiles() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->maxFiles;
}

void AuditLogManager::rotateNow()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->performRotation();
    Logger::info("AuditLog", "手动执行日志轮转");
}

} // namespace Core
} // namespace Eagle
