#ifndef EAGLE_CORE_CONFIGVERSION_H
#define EAGLE_CORE_CONFIGVERSION_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QMap>

namespace Eagle {
namespace Core {

/**
 * @brief 配置版本信息
 */
struct ConfigVersion {
    int version;                    // 版本号
    QDateTime timestamp;            // 创建时间
    QString author;                 // 创建者
    QString description;            // 变更说明
    QVariantMap config;             // 配置内容
    QString configHash;             // 配置哈希值（用于快速对比）
    
    ConfigVersion()
        : version(0)
    {}
    
    bool isValid() const {
        return version > 0 && !timestamp.isNull();
    }
};

/**
 * @brief 配置版本差异
 */
struct ConfigDiff {
    QString key;                    // 配置键
    QVariant oldValue;              // 旧值
    QVariant newValue;               // 新值
    QString changeType;             // 变更类型：added, removed, modified
    
    ConfigDiff()
        : changeType("unknown")
    {}
};

/**
 * @brief 配置版本管理器
 * 
 * 负责管理配置的版本历史、版本回滚和版本对比
 */
class ConfigVersionManager : public QObject {
    Q_OBJECT
    
public:
    explicit ConfigVersionManager(QObject* parent = nullptr);
    ~ConfigVersionManager();
    
    /**
     * @brief 创建新版本
     * @param config 配置内容
     * @param author 创建者
     * @param description 变更说明
     * @param version 版本号（如果为0，则自动递增）
     * @return 创建的版本号，失败返回0
     */
    int createVersion(const QVariantMap& config, const QString& author = QString(), 
                     const QString& description = QString(), int version = 0);
    
    /**
     * @brief 获取当前版本号
     */
    int currentVersion() const;
    
    /**
     * @brief 获取版本列表
     * @param limit 限制返回数量（0表示不限制）
     * @return 版本列表（按版本号降序）
     */
    QList<ConfigVersion> getVersions(int limit = 0) const;
    
    /**
     * @brief 获取指定版本
     */
    ConfigVersion getVersion(int version) const;
    
    /**
     * @brief 回滚到指定版本
     * @param version 目标版本号
     * @return 回滚后的配置内容
     */
    QVariantMap rollbackToVersion(int version);
    
    /**
     * @brief 对比两个版本的差异
     * @param version1 版本1
     * @param version2 版本2
     * @return 差异列表
     */
    QList<ConfigDiff> compareVersions(int version1, int version2) const;
    
    /**
     * @brief 对比当前配置与指定版本的差异
     * @param version 版本号
     * @param currentConfig 当前配置
     * @return 差异列表
     */
    QList<ConfigDiff> compareWithVersion(int version, const QVariantMap& currentConfig) const;
    
    /**
     * @brief 删除指定版本
     * @param version 版本号
     * @return 是否成功
     */
    bool deleteVersion(int version);
    
    /**
     * @brief 清理旧版本（保留最近N个版本）
     * @param keepCount 保留的版本数量
     * @return 删除的版本数量
     */
    int cleanupOldVersions(int keepCount = 10);
    
    /**
     * @brief 设置版本存储路径
     */
    void setStoragePath(const QString& path);
    QString storagePath() const;
    
    /**
     * @brief 启用/禁用版本管理
     */
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
signals:
    void versionCreated(int version, const QString& author);
    void versionRolledBack(int fromVersion, int toVersion);
    void versionDeleted(int version);
    
private:
    Q_DISABLE_COPY(ConfigVersionManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    // 辅助方法
    QString calculateConfigHash(const QVariantMap& config) const;
    void saveVersionToFile(const ConfigVersion& version) const;
    ConfigVersion loadVersionFromFile(int version) const;
    QList<int> getAllVersionNumbers() const;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGVERSION_H
