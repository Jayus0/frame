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
 * @brief 日志轮转策略
 */
enum class RotationStrategy {
    None,      // 不轮转
    Size,      // 按大小轮转
    Daily,     // 按天轮转
    Weekly,    // 按周轮转
    Monthly    // 按月轮转
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
    
    // 日志轮转配置
    void setRotationEnabled(bool enabled);
    bool isRotationEnabled() const;
    void setRotationStrategy(RotationStrategy strategy);
    RotationStrategy getRotationStrategy() const;
    void setMaxFileSize(qint64 maxSizeBytes);  // 按大小轮转的最大文件大小
    qint64 getMaxFileSize() const;
    void setMaxFiles(int maxFiles);  // 保留的最大文件数
    int getMaxFiles() const;
    void rotateNow();  // 立即执行轮转
    
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
Q_DECLARE_METATYPE(Eagle::Core::RotationStrategy)

#endif // EAGLE_CORE_AUDITLOG_H
