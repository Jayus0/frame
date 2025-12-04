#ifndef EAGLE_CORE_BACKUPMANAGER_H
#define EAGLE_CORE_BACKUPMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

class ConfigManager;

/**
 * @brief 备份类型
 */
enum class BackupType {
    Full,       // 全量备份
    Incremental // 增量备份
};

/**
 * @brief 备份触发方式
 */
enum class BackupTrigger {
    Manual,     // 手动触发
    Scheduled,  // 定时触发
    OnChange    // 配置变更触发
};

/**
 * @brief 备份记录信息
 */
struct BackupInfo {
    QString id;                 // 备份ID（时间戳+随机字符串）
    QString name;              // 备份名称
    BackupType type;            // 备份类型
    BackupTrigger trigger;      // 触发方式
    QDateTime createTime;       // 创建时间
    QString description;        // 描述
    qint64 size;                // 备份文件大小（字节）
    QString filePath;           // 备份文件路径
    QVariantMap metadata;       // 元数据
    
    BackupInfo()
        : type(BackupType::Full)
        , trigger(BackupTrigger::Manual)
        , size(0)
    {
        createTime = QDateTime::currentDateTime();
    }
    
    bool isValid() const {
        return !id.isEmpty() && !filePath.isEmpty();
    }
};

/**
 * @brief 备份策略配置
 */
struct BackupPolicy {
    bool enabled = false;                    // 是否启用自动备份
    BackupType defaultType = BackupType::Full; // 默认备份类型
    int maxBackups = 10;                     // 最大保留备份数
    int scheduleIntervalMinutes = 60;        // 定时备份间隔（分钟）
    bool backupOnChange = true;              // 配置变更时自动备份
    QString backupDir;                       // 备份目录
    QString backupPrefix = "backup";         // 备份文件前缀
    
    bool isValid() const {
        return !backupDir.isEmpty() && maxBackups > 0;
    }
};

/**
 * @brief 备份管理器
 */
class BackupManager : public QObject {
    Q_OBJECT
    
public:
    explicit BackupManager(ConfigManager* configManager, QObject* parent = nullptr);
    ~BackupManager();
    
    // 备份策略配置
    bool setPolicy(const BackupPolicy& policy);
    BackupPolicy getPolicy() const;
    
    // 备份操作
    QString createBackup(BackupType type = BackupType::Full, 
                        const QString& name = QString(),
                        const QString& description = QString());
    bool deleteBackup(const QString& backupId);
    bool deleteOldBackups(int keepCount = -1);  // -1表示使用策略中的maxBackups
    
    // 备份查询
    QList<BackupInfo> listBackups() const;
    BackupInfo getBackupInfo(const QString& backupId) const;
    bool backupExists(const QString& backupId) const;
    
    // 恢复操作
    bool restoreBackup(const QString& backupId, bool verifyBeforeRestore = true);
    bool restoreFromFile(const QString& filePath, bool verifyBeforeRestore = true);
    
    // 备份验证
    bool verifyBackup(const QString& backupId) const;
    bool verifyBackupFile(const QString& filePath) const;
    
    // 自动备份控制
    void setAutoBackupEnabled(bool enabled);
    bool isAutoBackupEnabled() const;
    
    // 手动触发备份（使用当前策略）
    QString triggerScheduledBackup();
    
signals:
    void backupCreated(const QString& backupId, const BackupInfo& info);
    void backupDeleted(const QString& backupId);
    void backupRestored(const QString& backupId, bool success);
    void backupFailed(const QString& backupId, const QString& error);
    
private slots:
    void onConfigChanged(const QString& key);
    void onScheduledBackup();
    
private:
    Q_DISABLE_COPY(BackupManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    QString generateBackupId() const;
    QString generateBackupFilePath(const QString& backupId) const;
    bool saveBackupInfo(const BackupInfo& info) const;
    BackupInfo loadBackupInfo(const QString& backupId) const;
    bool cleanupOldBackups();
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::BackupType)
Q_DECLARE_METATYPE(Eagle::Core::BackupTrigger)
Q_DECLARE_METATYPE(Eagle::Core::BackupInfo)
Q_DECLARE_METATYPE(Eagle::Core::BackupPolicy)

#endif // EAGLE_CORE_BACKUPMANAGER_H
