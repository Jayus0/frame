#include "eagle/core/PluginManager.h"
#include "PluginManager_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPluginLoader>
#include <QtCore/QStandardPaths>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QDebug>

namespace Eagle {
namespace Core {

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , d_ptr(new PluginManagerPrivate)
{
    auto* d = d_func();
    d->pluginPaths << QDir::currentPath() + "/plugins";
    d->pluginPaths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
}

PluginManager::~PluginManager()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 卸载所有插件
    QStringList loadedPlugins = d->plugins.keys();
    for (const QString& pluginId : loadedPlugins) {
        unloadPlugin(pluginId);
    }
    
    delete d_ptr;
}

void PluginManager::setPluginPaths(const QStringList& paths)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->pluginPaths = paths;
}

QStringList PluginManager::pluginPaths() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginPaths;
}

void PluginManager::setPluginSignatureRequired(bool required)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->signatureRequired = required;
}

bool PluginManager::scanPlugins()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    Logger::info("PluginManager", "开始扫描插件...");
    
    for (const QString& path : d->pluginPaths) {
        QDir dir(path);
        if (!dir.exists()) {
            Logger::warning("PluginManager", QString("插件目录不存在: %1").arg(path));
            continue;
        }
        
        QStringList filters;
#ifdef Q_OS_WIN
        filters << "*.dll";
#elif defined(Q_OS_MAC)
        filters << "*.dylib";
#else
        filters << "*.so";
#endif
        
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& fileInfo : files) {
            QString filePath = fileInfo.absoluteFilePath();
            
            // 尝试加载插件获取元数据
            QPluginLoader loader(filePath);
            QObject* pluginObj = loader.instance();
            if (!pluginObj) {
                Logger::warning("PluginManager", QString("无法加载插件: %1, 错误: %2")
                    .arg(filePath, loader.errorString()));
                continue;
            }
            
            IPlugin* plugin = qobject_cast<IPlugin*>(pluginObj);
            if (!plugin) {
                Logger::warning("PluginManager", QString("插件未实现IPlugin接口: %1").arg(filePath));
                loader.unload();
                continue;
            }
            
            PluginMetadata meta = plugin->metadata();
            if (!meta.isValid()) {
                Logger::warning("PluginManager", QString("插件元数据无效: %1").arg(filePath));
                loader.unload();
                continue;
            }
            
            d->metadata[meta.pluginId] = meta;
            Logger::info("PluginManager", QString("发现插件: %1 (%2)")
                .arg(meta.name, meta.version));
            
            loader.unload();
        }
    }
    
    Logger::info("PluginManager", QString("插件扫描完成，共发现 %1 个插件").arg(d->metadata.size()));
    return true;
}

QStringList PluginManager::availablePlugins() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->metadata.keys();
}

bool PluginManager::loadPlugin(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->plugins.contains(pluginId)) {
        Logger::warning("PluginManager", QString("插件已加载: %1").arg(pluginId));
        return true;
    }
    
    if (!d->metadata.contains(pluginId)) {
        Logger::error("PluginManager", QString("插件不存在: %1").arg(pluginId));
        emit pluginError(pluginId, "Plugin not found");
        return false;
    }
    
    // 检查依赖
    QStringList missing;
    if (!checkDependencies(pluginId, missing)) {
        QString error = QString("缺少依赖: %1").arg(missing.join(", "));
        Logger::error("PluginManager", error);
        emit pluginError(pluginId, error);
        return false;
    }
    
    // 加载依赖
    PluginMetadata meta = d->metadata[pluginId];
    for (const QString& dep : meta.dependencies) {
        if (!isPluginLoaded(dep)) {
            if (!loadPlugin(dep)) {
                Logger::error("PluginManager", QString("无法加载依赖插件: %1").arg(dep));
                emit pluginError(pluginId, QString("Failed to load dependency: %1").arg(dep));
                return false;
            }
        }
    }
    
    // 查找插件文件
    QString pluginPath;
    for (const QString& path : d->pluginPaths) {
        QDir dir(path);
        QStringList filters;
#ifdef Q_OS_WIN
        filters << "*.dll";
#elif defined(Q_OS_MAC)
        filters << "*.dylib";
#else
        filters << "*.so";
#endif
        
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& fileInfo : files) {
            QPluginLoader testLoader(fileInfo.absoluteFilePath());
            QObject* obj = testLoader.instance();
            if (obj) {
                IPlugin* testPlugin = qobject_cast<IPlugin*>(obj);
                if (testPlugin && testPlugin->metadata().pluginId == pluginId) {
                    pluginPath = fileInfo.absoluteFilePath();
                    testLoader.unload();
                    break;
                }
                testLoader.unload();
            }
        }
        if (!pluginPath.isEmpty()) break;
    }
    
    if (pluginPath.isEmpty()) {
        Logger::error("PluginManager", QString("找不到插件文件: %1").arg(pluginId));
        emit pluginError(pluginId, "Plugin file not found");
        return false;
    }
    
    // 加载插件
    QPluginLoader* loader = new QPluginLoader(pluginPath, this);
    QObject* pluginObj = loader->instance();
    if (!pluginObj) {
        QString error = loader->errorString();
        Logger::error("PluginManager", QString("加载插件失败: %1, 错误: %2").arg(pluginId, error));
        delete loader;
        emit pluginError(pluginId, error);
        return false;
    }
    
    IPlugin* plugin = qobject_cast<IPlugin*>(pluginObj);
    if (!plugin) {
        Logger::error("PluginManager", QString("插件未实现IPlugin接口: %1").arg(pluginId));
        loader->unload();
        delete loader;
        emit pluginError(pluginId, "Plugin does not implement IPlugin interface");
        return false;
    }
    
    // 初始化插件
    PluginContext context;
    context.pluginPath = pluginPath;
    context.configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + pluginId;
    
    // 获取框架实例以传递服务注册中心和事件总线
    // 注意：这里需要框架实例，但为了避免循环依赖，我们通过信号传递
    // 或者插件在初始化后自己获取框架实例
    
    if (!plugin->initialize(context)) {
        Logger::error("PluginManager", QString("插件初始化失败: %1").arg(pluginId));
        loader->unload();
        delete loader;
        emit pluginError(pluginId, "Plugin initialization failed");
        return false;
    }
    
    d->loaders[pluginId] = loader;
    d->plugins[pluginId] = plugin;
    
    Logger::info("PluginManager", QString("插件加载成功: %1").arg(pluginId));
    emit pluginLoaded(pluginId);
    return true;
}

bool PluginManager::unloadPlugin(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->plugins.contains(pluginId)) {
        return true; // 已经卸载
    }
    
    IPlugin* plugin = d->plugins[pluginId];
    plugin->shutdown();
    
    QPluginLoader* loader = d->loaders[pluginId];
    if (!loader->unload()) {
        Logger::warning("PluginManager", QString("卸载插件失败: %1, 错误: %2")
            .arg(pluginId, loader->errorString()));
    }
    
    delete loader;
    d->loaders.remove(pluginId);
    d->plugins.remove(pluginId);
    
    Logger::info("PluginManager", QString("插件卸载成功: %1").arg(pluginId));
    emit pluginUnloaded(pluginId);
    return true;
}

bool PluginManager::reloadPlugin(const QString& pluginId)
{
    if (unloadPlugin(pluginId)) {
        return loadPlugin(pluginId);
    }
    return false;
}

IPlugin* PluginManager::getPlugin(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->plugins.value(pluginId, nullptr);
}

bool PluginManager::isPluginLoaded(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->plugins.contains(pluginId);
}

PluginMetadata PluginManager::getPluginMetadata(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->metadata.value(pluginId);
}

QStringList PluginManager::resolveDependencies(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList result;
    if (!d->metadata.contains(pluginId)) {
        return result;
    }
    
    PluginMetadata meta = d->metadata[pluginId];
    for (const QString& dep : meta.dependencies) {
        result << dep;
        result << resolveDependencies(dep); // 递归解析
    }
    
    return result;
}

bool PluginManager::checkDependencies(const QString& pluginId, QStringList& missing) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    missing.clear();
    if (!d->metadata.contains(pluginId)) {
        missing << pluginId;
        return false;
    }
    
    PluginMetadata meta = d->metadata[pluginId];
    for (const QString& dep : meta.dependencies) {
        if (!d->metadata.contains(dep)) {
            missing << dep;
        }
    }
    
    return missing.isEmpty();
}

bool PluginManager::hotUpdatePlugin(const QString& pluginId)
{
    Logger::info("PluginManager", QString("开始热更新插件: %1").arg(pluginId));
    return reloadPlugin(pluginId);
}

} // namespace Core
} // namespace Eagle
