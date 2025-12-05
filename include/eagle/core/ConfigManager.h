#ifndef EAGLE_CORE_CONFIGMANAGER_H
#define EAGLE_CORE_CONFIGMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include "ConfigVersion.h"
#include "ConfigFormat.h"

namespace Eagle {
namespace Core {

class ConfigManagerPrivate;

/**
 * @brief 配置管理器
 * 
 * 支持多级配置、热更新和配置验证
 */
class ConfigManager : public QObject {
    Q_OBJECT
    
public:
    enum ConfigLevel {
        Global,     // 全局配置
        User,       // 用户配置
        Plugin      // 插件配置
    };
    
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    
    // 配置加载
    bool loadFromFile(const QString& filePath, ConfigLevel level = Global, ConfigFormat format = ConfigFormat::JSON);
    bool loadFromJson(const QByteArray& json, ConfigLevel level = Global);
    bool loadFromYaml(const QByteArray& yaml, ConfigLevel level = Global);
    bool loadFromIni(const QByteArray& ini, ConfigLevel level = Global);
    void loadFromEnvironment();
    
    // 配置访问
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value, ConfigLevel level = Global);
    QVariantMap getAll() const;
    QVariantMap getByPrefix(const QString& prefix) const;
    
    // 配置更新
    bool updateConfig(const QVariantMap& config, ConfigLevel level = Global);
    bool saveToFile(const QString& filePath, ConfigLevel level = Global, ConfigFormat format = ConfigFormat::JSON);
    
    // 配置验证
    bool validateConfig(const QVariantMap& config, const QString& schemaPath) const;
    
    // Schema管理
    void setSchemaPath(const QString& schemaPath);
    QString schemaPath() const;
    
    // 版本管理
    ConfigVersionManager* versionManager() const;
    int createConfigVersion(const QString& author = QString(), const QString& description = QString());
    bool rollbackConfig(int version);
    QList<ConfigVersion> getConfigVersions(int limit = 0) const;
    ConfigVersion getConfigVersion(int version) const;
    QList<ConfigDiff> compareConfigVersions(int version1, int version2) const;
    
    // 配置监听
    void watchConfig(const QString& key, QObject* receiver, const char* method);
    
    // 加密配置
    void setEncryptionEnabled(bool enabled);
    void setSensitiveKeys(const QStringList& keys);
    void setEncryptionKey(const QString& key);
    
signals:
    void configChanged(const QString& key, const QVariant& oldValue, const QVariant& newValue);
    void configReloaded();
    
private:
    Q_DISABLE_COPY(ConfigManager)
    ConfigManagerPrivate* d_ptr;
    
    inline ConfigManagerPrivate* d_func() { return d_ptr; }
    inline const ConfigManagerPrivate* d_func() const { return d_ptr; }
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGMANAGER_H
