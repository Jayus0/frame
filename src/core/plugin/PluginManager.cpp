#include "eagle/core/PluginManager.h"
#include "PluginManager_p.h"
#include "eagle/core/PluginSignature.h"
#include "eagle/core/PluginIsolation.h"
#include "eagle/core/Framework.h"
#include "eagle/core/Logger.h"
#include "eagle/core/AlertSystem.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPluginLoader>
#include <QtCore/QStandardPaths>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDebug>
#include <QtCore/QSet>
#include <algorithm>

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
                // 不调用unload，让loader析构时自动处理
                continue;
            }
            
            PluginMetadata meta = plugin->metadata();
            if (!meta.isValid()) {
                Logger::warning("PluginManager", QString("插件元数据无效: %1").arg(filePath));
                // 不调用unload，让loader析构时自动处理
                continue;
            }
            
            // 如果要求签名验证，检查插件签名
            if (d->signatureRequired) {
                QString sigPath = PluginSignatureVerifier::findSignatureFile(filePath);
                if (sigPath.isEmpty()) {
                    Logger::warning("PluginManager", QString("插件缺少签名文件: %1").arg(filePath));
                    continue;
                }
                
                PluginSignature signature = PluginSignatureVerifier::loadFromFile(sigPath);
                if (!signature.isValid()) {
                    Logger::warning("PluginManager", QString("插件签名无效: %1").arg(filePath));
                    continue;
                }
                
                if (!PluginSignatureVerifier::verify(filePath, signature)) {
                    Logger::error("PluginManager", QString("插件签名验证失败: %1").arg(filePath));
                    continue;
                }
                
                Logger::info("PluginManager", QString("插件签名验证通过: %1").arg(meta.name));
            }
            
            d->metadata[meta.pluginId] = meta;
            Logger::info("PluginManager", QString("发现插件: %1 (%2)")
                .arg(meta.name, meta.version));
            
            // 不调用unload，让loader析构时自动处理，避免触发插件析构函数
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
    // 先检查是否已加载（不锁定，避免死锁）
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
    }
    
    // 检查依赖（在锁外进行，避免死锁）
    QStringList missing;
    if (!checkDependencies(pluginId, missing)) {
        QString error = QString("缺少依赖: %1").arg(missing.join(", "));
        Logger::error("PluginManager", error);
        emit pluginError(pluginId, error);
        return false;
    }
    
    // 检查循环依赖
    QStringList cyclePath;
    if (detectCircularDependencies(pluginId, cyclePath)) {
        QString cycleStr = cyclePath.join(" -> ");
        Logger::error("PluginManager", QString("检测到循环依赖: %1").arg(cycleStr));
        emit pluginError(pluginId, QString("Circular dependency detected: %1").arg(cycleStr));
        
        // 发送告警（通过创建告警规则和记录）
        Framework* framework = qobject_cast<Framework*>(parent());
        if (framework && framework->alertSystem()) {
            AlertSystem* alertSystem = framework->alertSystem();
            // 创建临时告警规则（如果不存在）
            AlertRule rule;
            rule.id = "plugin.circular_dependency";
            rule.name = "插件循环依赖";
            rule.metricName = "plugin.circular_dependency";
            rule.level = AlertLevel::Error;
            rule.condition = "==";
            rule.threshold = 1.0;
            rule.enabled = true;
            rule.description = "检测到插件循环依赖";
            
            if (!alertSystem->getRule(rule.id).isValid()) {
                alertSystem->addRule(rule);
            }
            
            // 记录告警（通过Logger，AlertSystem会自动检查规则）
            Logger::error("PluginManager", QString("插件 %1 存在循环依赖: %2").arg(pluginId, cycleStr));
        }
        
        return false;
    }
    
    // 加载依赖（递归调用，但此时没有锁，所以安全）
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        PluginMetadata meta = d->metadata[pluginId];
        locker.unlock();  // 释放锁，避免递归加载时死锁
        
        for (const QString& dep : meta.dependencies) {
            if (!isPluginLoaded(dep)) {
                if (!loadPlugin(dep)) {
                    Logger::error("PluginManager", QString("无法加载依赖插件: %1").arg(dep));
                    emit pluginError(pluginId, QString("Failed to load dependency: %1").arg(dep));
                    return false;
                }
            }
        }
    }
    
    // 现在重新获取锁来加载当前插件
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 再次检查是否已加载（可能在加载依赖时被其他线程加载了）
    if (d->plugins.contains(pluginId)) {
        return true;
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
    
    // 如果要求签名验证，再次验证签名（加载前最后检查）
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        if (d->signatureRequired) {
            QString sigPath = PluginSignatureVerifier::findSignatureFile(pluginPath);
            if (sigPath.isEmpty()) {
                Logger::error("PluginManager", QString("插件缺少签名文件: %1").arg(pluginPath));
                emit pluginError(pluginId, "Plugin signature file not found");
                return false;
            }
            
            PluginSignature signature = PluginSignatureVerifier::loadFromFile(sigPath);
            if (!signature.isValid() || !PluginSignatureVerifier::verify(pluginPath, signature)) {
                Logger::error("PluginManager", QString("插件签名验证失败: %1").arg(pluginPath));
                emit pluginError(pluginId, "Plugin signature verification failed");
                return false;
            }
        }
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
    
    // 在隔离环境中初始化插件
    PluginContext context;
    context.pluginPath = pluginPath;
    context.configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + pluginId;
    
    // 注册异常处理器
    PluginIsolation::registerExceptionHandler(pluginId, [this, pluginId](const QString& error) {
        emit pluginError(pluginId, error);
    });
    
    // 在隔离环境中执行初始化，并记录性能
    QElapsedTimer timer;
    timer.start();
    
    bool initSuccess = false;
    try {
        initSuccess = plugin->initialize(context);
    } catch (const std::exception& e) {
        QString error = QString("插件初始化异常: %1 - %2").arg(pluginId, e.what());
        Logger::error("PluginManager", error);
        PluginIsolation::registerExceptionHandler(pluginId, [this, pluginId](const QString& err) {
            emit pluginError(pluginId, err);
        });
        PluginIsolation::executeIsolated(pluginId, []() -> bool { return false; });
        loader->unload();
        delete loader;
        emit pluginError(pluginId, error);
        return false;
    } catch (...) {
        QString error = QString("插件初始化未知异常: %1").arg(pluginId);
        Logger::error("PluginManager", error);
        loader->unload();
        delete loader;
        emit pluginError(pluginId, error);
        return false;
    }
    
    if (!initSuccess) {
        QString error = QString("插件初始化失败: %1").arg(pluginId);
        Logger::error("PluginManager", error);
        loader->unload();
        delete loader;
        emit pluginError(pluginId, error);
        return false;
    }
    
    // 记录插件加载时间
    qint64 loadTime = timer.elapsed();
    Framework* framework = Framework::instance();
    if (framework && framework->performanceMonitor()) {
        framework->performanceMonitor()->recordPluginLoadTime(pluginId, loadTime);
    }
    
    d->loaders[pluginId] = loader;
    d->plugins[pluginId] = plugin;
    
    // 先释放锁，再发送信号，避免信号槽处理时再次获取锁导致死锁
    locker.unlock();
    
    Logger::info("PluginManager", QString("插件加载成功: %1").arg(pluginId));
    
    // 使用QueuedConnection避免在锁内发送信号导致问题
    // 注意：这里已经释放了锁，可以直接发送信号
    emit pluginLoaded(pluginId);
    
    return true;
}

bool PluginManager::unloadPlugin(const QString& pluginId)
{
    IPlugin* plugin = nullptr;
    QPluginLoader* loader = nullptr;
    
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        
        if (!d->plugins.contains(pluginId)) {
            return true; // 已经卸载
        }
        
        plugin = d->plugins[pluginId];
        loader = d->loaders[pluginId];
        
        // 从映射中移除，但先不删除对象
        d->loaders.remove(pluginId);
        d->plugins.remove(pluginId);
    }
    
    // 在隔离环境中执行关闭操作
    if (plugin) {
        try {
            plugin->shutdown();
        } catch (const std::exception& e) {
            Logger::error("PluginManager", QString("插件关闭异常: %1 - %2").arg(pluginId, e.what()));
        } catch (...) {
            Logger::error("PluginManager", QString("插件关闭未知异常: %1").arg(pluginId));
        }
    }
    
    if (loader) {
        // 先调用shutdown，再unload，避免析构函数中再次调用shutdown
        if (!loader->unload()) {
            Logger::warning("PluginManager", QString("卸载插件失败: %1, 错误: %2")
                .arg(pluginId, loader->errorString()));
        }
        // 注意：不要delete loader，unload后Qt会自动管理
        // 但因为我们用new创建，需要delete
        delete loader;
    }
    
    Logger::info("PluginManager", QString("插件卸载成功: %1").arg(pluginId));
    
    // 注意：这里已经释放了锁，可以直接发送信号
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

bool PluginManager::detectCircularDependencies(const QString& pluginId, QStringList& cyclePath) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->metadata.contains(pluginId)) {
        return false;
    }
    
    // 使用DFS检测循环依赖
    QMap<QString, int> visited;  // 0: 未访问, 1: 正在访问, 2: 已访问
    QStringList path;
    
    std::function<bool(const QString&)> dfs = [&](const QString& current) -> bool {
        if (visited.value(current) == 1) {
            // 发现循环，构建循环路径
            int startIndex = path.indexOf(current);
            if (startIndex >= 0) {
                cyclePath = path.mid(startIndex);
                cyclePath.append(current);  // 形成闭环
            } else {
                cyclePath = path;
                cyclePath.append(current);
            }
            return true;
        }
        
        if (visited.value(current) == 2) {
            // 已访问过，无循环
            return false;
        }
        
        visited[current] = 1;  // 标记为正在访问
        path.append(current);
        
        if (d->metadata.contains(current)) {
            PluginMetadata meta = d->metadata[current];
            for (const QString& dep : meta.dependencies) {
                if (dfs(dep)) {
                    return true;  // 发现循环
                }
            }
        }
        
        path.removeLast();
        visited[current] = 2;  // 标记为已访问
        return false;
    };
    
    return dfs(pluginId);
}

QList<QStringList> PluginManager::detectAllCircularDependencies() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<QStringList> allCycles;
    QSet<QString> processedCycles;  // 用于去重
    
    // 对每个插件检测循环依赖
    for (const QString& pluginId : d->metadata.keys()) {
        QStringList cyclePath;
        if (detectCircularDependencies(pluginId, cyclePath)) {
            // 标准化循环路径（从最小ID开始）
            if (!cyclePath.isEmpty()) {
                // 找到最小ID的位置
                QString minId = cyclePath.first();
                int minIndex = 0;
                for (int i = 1; i < cyclePath.size(); ++i) {
                    if (cyclePath[i] < minId) {
                        minId = cyclePath[i];
                        minIndex = i;
                    }
                }
                
                // 重新排列路径
                QStringList normalizedPath;
                for (int i = 0; i < cyclePath.size(); ++i) {
                    normalizedPath.append(cyclePath[(minIndex + i) % cyclePath.size()]);
                }
                
                // 去重：使用排序后的路径作为key
                QStringList sortedPath = normalizedPath;
                std::sort(sortedPath.begin(), sortedPath.end());
                QString cycleKey = sortedPath.join("->");
                
                if (!processedCycles.contains(cycleKey)) {
                    processedCycles.insert(cycleKey);
                    allCycles.append(normalizedPath);
                }
            }
        }
    }
    
    return allCycles;
}

bool PluginManager::hasCircularDependencies() const
{
    QStringList dummy;
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 检查所有插件是否有循环依赖
    for (const QString& pluginId : d->metadata.keys()) {
        if (detectCircularDependencies(pluginId, dummy)) {
            return true;
        }
    }
    
    return false;
}

} // namespace Core
} // namespace Eagle
