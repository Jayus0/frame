#ifndef EAGLE_CORE_AUDITLOG_H
#define EAGLE_CORE_AUDITLOG_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

/**
 * @brief 审计日志级别
 */
enum class AuditLevel {
    Info,      // 信息
    Warning,   // 警告
    Error,     // 错误
    Critical   // 严重
};

/**
 * @brief 审计日志条目
 */
struct AuditLogEntry {
    QDateTime timestamp;        // 时间戳
    QString userId;              // 用户ID
    QString action;              // 操作（如：plugin.load, config.update）
    QString resource;            // 资源（如：pluginId, configKey）
    AuditLevel level;            // 级别
    QString description;         // 描述
    bool success;                // 是否成功
    QString errorMessage;        // 错误信息（如果失败）
    QVariantMap metadata;        // 额外元数据
    
    AuditLogEntry() 
        : level(AuditLevel::Info)
        , success(true)
    {
        timestamp = QDateTime::currentDateTime();
    }
    
    bool isValid() const {
        return !userId.isEmpty() && !action.isEmpty();
    }
};

/**
 * @brief 审计日志管理器
 */
class AuditLogManager : public QObject {
    Q_OBJECT
    
public:
    explicit AuditLogManager(QObject* parent = nullptr);
    ~AuditLogManager();
    
    // 记录审计日志
    void log(const AuditLogEntry& entry);
    void log(const QString& userId, const QString& action, const QString& resource = QString(),
             AuditLevel level = AuditLevel::Info, bool success = true, 
             const QString& errorMessage = QString(), const QVariantMap& metadata = QVariantMap());
    
    // 查询审计日志
    QList<AuditLogEntry> query(const QString& userId = QString(),
                              const QString& action = QString(),
                              const QDateTime& startTime = QDateTime(),
                              const QDateTime& endTime = QDateTime(),
                              AuditLevel minLevel = AuditLevel::Info) const;
    
    // 配置
    void setLogFile(const QString& filePath);
    void setMaxEntries(int maxEntries);
    void setAutoFlush(bool autoFlush);
    void flush();
    
    // 统计
    int getEntryCount() const;
    int getEntryCount(const QString& userId) const;
    int getFailureCount(const QString& userId) const;
    
signals:
    void logEntryAdded(const AuditLogEntry& entry);
    
private:
    Q_DISABLE_COPY(AuditLogManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::AuditLogEntry)
Q_DECLARE_METATYPE(Eagle::Core::AuditLevel)

#endif // EAGLE_CORE_AUDITLOG_H
