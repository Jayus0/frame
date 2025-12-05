#ifndef AUDITLOG_P_H
#define AUDITLOG_P_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include "eagle/core/AuditLog.h"

namespace Eagle {
namespace Core {

class AuditLogManager::Private {
public:
    QList<AuditLogEntry> entries;
    QString logFilePath;
    QString logDir;                    // 日志目录
    QString logFileBaseName;            // 日志文件基础名称（不含日期后缀）
    int maxEntries = 10000;
    bool autoFlush = true;
    mutable QMutex mutex;
    
    // 日志轮转配置
    bool rotationEnabled = true;
    RotationStrategy rotationStrategy = RotationStrategy::Daily;
    qint64 maxFileSizeBytes = 10 * 1024 * 1024;  // 默认10MB
    int maxFiles = 30;                            // 默认保留30个文件
    QDateTime lastRotationDate;                   // 上次轮转日期
    
    // 日志防篡改
    bool tamperProtectionEnabled = true;          // 是否启用防篡改
    QString lastEntryHash;                        // 最后一条日志的哈希值（用于链式哈希）
    
    void writeToFile(const AuditLogEntry& entry);
    bool shouldRotate() const;
    void performRotation();
    void cleanupOldFiles();
    QString generateLogFileName(const QDateTime& date) const;
    QDateTime getCurrentRotationDate() const;
};

} // namespace Core
} // namespace Eagle

#endif // AUDITLOG_P_H
