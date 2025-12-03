#ifndef EAGLE_CORE_PLUGINMANAGER_H
#define EAGLE_CORE_PLUGINMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QPluginLoader>
#include "IPlugin.h"

namespace Eagle {
namespace Core {

class PluginManagerPrivate;

/**
 * @brief 插件管理器
 * 
 * 负责插件的发现、加载、卸载和生命周期管理
 */
class PluginManager : public QObject {
    Q_OBJECT
    
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager();
    
    // 配置
    void setPluginPaths(const QStringList& paths);
    QStringList pluginPaths() const;  // 获取当前配置的插件路径
    void setPluginSignatureRequired(bool required);
    
    // 插件发现
    bool scanPlugins();
    QStringList availablePlugins() const;
    
    // 插件加载
    bool loadPlugin(const QString& pluginId);
    bool unloadPlugin(const QString& pluginId);
    bool reloadPlugin(const QString& pluginId);
    
    // 插件查询
    IPlugin* getPlugin(const QString& pluginId) const;
    bool isPluginLoaded(const QString& pluginId) const;
    PluginMetadata getPluginMetadata(const QString& pluginId) const;
    
    // 依赖管理
    QStringList resolveDependencies(const QString& pluginId) const;
    bool checkDependencies(const QString& pluginId, QStringList& missing) const;
    
    // 热更新
    bool hotUpdatePlugin(const QString& pluginId);
    
signals:
    void pluginLoaded(const QString& pluginId);
    void pluginUnloaded(const QString& pluginId);
    void pluginError(const QString& pluginId, const QString& error);
    
private:
    Q_DISABLE_COPY(PluginManager)
    PluginManagerPrivate* d_ptr;
    
    // Q_DECLARE_PRIVATE 需要前置声明，这里直接使用指针
    inline PluginManagerPrivate* d_func() { return d_ptr; }
    inline const PluginManagerPrivate* d_func() const { return d_ptr; }
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_PLUGINMANAGER_H
