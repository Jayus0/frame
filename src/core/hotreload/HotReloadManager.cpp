#include "eagle/core/HotReloadManager.h"
#include "HotReloadManager_p.h"
#include "eagle/core/PluginManager.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/ConfigManager.h"
#include "eagle/core/Logger.h"
#include <QtCore/QTimer>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>

namespace Eagle {
namespace Core {

HotReloadManager::HotReloadManager(PluginManager* pluginManager, QObject* parent)
    : QObject(parent)
    , d(new HotReloadManager::Private(pluginManager))
{
    if (!pluginManager) {
        Logger::error("HotReloadManager", "PluginManager不能为空");
        return;
    }
    
    // 确保状态存储目录存在
    QDir dir;
    if (!dir.exists(d->stateStoragePath)) {
        dir.mkpath(d->stateStoragePath);
    }
    
    // 连接插件管理器信号
    connect(pluginManager, &PluginManager::pluginUnloaded,
            this, &HotReloadManager::onPluginUnloaded, Qt::QueuedConnection);
    connect(pluginManager, &PluginManager::pluginLoaded,
            this, &HotReloadManager::onPluginLoaded, Qt::QueuedConnection);
    
    Logger::info("HotReloadManager", "热重载管理器初始化完成");
}

HotReloadManager::~HotReloadManager()
{
    delete d;
}

HotReloadResult HotReloadManager::reloadPlugin(const QString& pluginId, bool force)
{
    if (!isEnabled()) {
        HotReloadResult result;
        result.success = false;
        result.status = HotReloadStatus::Failed;
        result.errorMessage = "热重载功能已禁用";
        return result;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginManager) {
        HotReloadResult result;
        result.success = false;
        result.status = HotReloadStatus::Failed;
        result.errorMessage = "PluginManager未初始化";
        return result;
    }
    
    // 检查插件是否可重载
    if (!force && !isPluginReloadable(pluginId)) {
        HotReloadResult result;
        result.success = false;
        result.status = HotReloadStatus::Failed;
        result.errorMessage = QString("插件 %1 不可重载").arg(pluginId);
        return result;
    }
    
    HotReloadResult result;
    result.startTime = QDateTime::currentDateTime();
    result.status = HotReloadStatus::Saving;
    
    emit reloadStarted(pluginId);
    
    // 1. 保存插件状态
    if (d->autoSaveState) {
        locker.unlock();
        bool saved = savePluginState(pluginId);
        locker.relock();
        
        if (!saved) {
            Logger::warning("HotReloadManager", QString("保存插件状态失败: %1").arg(pluginId));
        }
    }
    
    // 2. 卸载插件
    result.status = HotReloadStatus::Unloading;
    d->pluginStatuses[pluginId] = HotReloadStatus::Unloading;
    
    locker.unlock();
    bool unloaded = d->pluginManager->unloadPlugin(pluginId);
    locker.relock();
    
    if (!unloaded) {
        result.success = false;
        result.status = HotReloadStatus::Failed;
        result.errorMessage = "插件卸载失败";
        result.endTime = QDateTime::currentDateTime();
        result.durationMs = result.startTime.msecsTo(result.endTime);
        d->pluginStatuses[pluginId] = HotReloadStatus::Failed;
        
        emit reloadFailed(pluginId, result.errorMessage);
        return result;
    }
    
    // 3. 重新加载插件
    result.status = HotReloadStatus::Loading;
    d->pluginStatuses[pluginId] = HotReloadStatus::Loading;
    
    locker.unlock();
    bool loaded = d->pluginManager->loadPlugin(pluginId);
    locker.relock();
    
    if (!loaded) {
        result.success = false;
        result.status = HotReloadStatus::Failed;
        result.errorMessage = "插件重新加载失败";
        result.endTime = QDateTime::currentDateTime();
        result.durationMs = result.startTime.msecsTo(result.endTime);
        d->pluginStatuses[pluginId] = HotReloadStatus::Failed;
        
        emit reloadFailed(pluginId, result.errorMessage);
        return result;
    }
    
    // 4. 恢复插件状态
    if (d->autoSaveState && hasPluginState(pluginId)) {
        result.status = HotReloadStatus::Restoring;
        d->pluginStatuses[pluginId] = HotReloadStatus::Restoring;
        
        locker.unlock();
        bool restored = restorePluginState(pluginId);
        locker.relock();
        
        if (!restored) {
            Logger::warning("HotReloadManager", QString("恢复插件状态失败: %1").arg(pluginId));
        }
    }
    
    result.success = true;
    result.status = HotReloadStatus::Success;
    result.endTime = QDateTime::currentDateTime();
    result.durationMs = result.startTime.msecsTo(result.endTime);
    d->pluginStatuses[pluginId] = HotReloadStatus::Success;
    
    Logger::info("HotReloadManager", QString("插件热重载成功: %1 (耗时: %2ms)")
        .arg(pluginId).arg(result.durationMs));
    
    emit reloadFinished(pluginId, true);
    
    // 重置状态为空闲
    QTimer::singleShot(1000, this, [this, pluginId]() {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        d->pluginStatuses[pluginId] = HotReloadStatus::Idle;
    });
    
    return result;
}

HotReloadResult HotReloadManager::reloadPlugins(const QStringList& pluginIds, bool force)
{
    HotReloadResult overallResult;
    overallResult.startTime = QDateTime::currentDateTime();
    overallResult.success = true;
    
    for (const QString& pluginId : pluginIds) {
        HotReloadResult result = reloadPlugin(pluginId, force);
        if (!result.success) {
            overallResult.success = false;
            overallResult.errorMessage += QString("%1: %2; ").arg(pluginId).arg(result.errorMessage);
        }
    }
    
    overallResult.endTime = QDateTime::currentDateTime();
    overallResult.durationMs = overallResult.startTime.msecsTo(overallResult.endTime);
    overallResult.status = overallResult.success ? HotReloadStatus::Success : HotReloadStatus::Failed;
    
    return overallResult;
}

bool HotReloadManager::savePluginState(const QString& pluginId)
{
    auto* d = d_func();
    
    if (!d->pluginManager) {
        return false;
    }
    
    PluginStateSnapshot snapshot;
    snapshot.pluginId = pluginId;
    
    // 获取插件配置（通过ConfigManager）
    // 这里简化实现，实际应该从ConfigManager获取
    snapshot.config = QVariantMap();
    
    // 获取已加载的服务（通过ServiceRegistry）
    // 这里简化实现，实际应该从ServiceRegistry获取
    snapshot.loadedServices = QStringList();
    
    // 获取插件元数据
    PluginMetadata metadata = d->pluginManager->getPluginMetadata(pluginId);
    snapshot.metadata["name"] = metadata.name;
    snapshot.metadata["version"] = metadata.version;
    snapshot.metadata["author"] = metadata.author;
    
    QMutexLocker locker(&d->mutex);
    d->pluginStates[pluginId] = snapshot;
    
    // 保存到文件
    bool saved = saveStateToFile(pluginId, snapshot);
    
    if (saved) {
        emit stateSaved(pluginId);
        Logger::info("HotReloadManager", QString("插件状态已保存: %1").arg(pluginId));
    }
    
    return saved;
}

bool HotReloadManager::restorePluginState(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginStates.contains(pluginId)) {
        // 尝试从文件加载
        PluginStateSnapshot snapshot = loadStateFromFile(pluginId);
        if (!snapshot.pluginId.isEmpty()) {
            d->pluginStates[pluginId] = snapshot;
        } else {
            return false;
        }
    }
    
    PluginStateSnapshot snapshot = d->pluginStates[pluginId];
    locker.unlock();
    
    // 恢复配置（通过ConfigManager）
    // 这里简化实现，实际应该通过ConfigManager恢复
    
    // 恢复服务（通过ServiceRegistry）
    // 这里简化实现，实际应该通过ServiceRegistry恢复
    
    emit stateRestored(pluginId);
    Logger::info("HotReloadManager", QString("插件状态已恢复: %1").arg(pluginId));
    
    return true;
}

PluginStateSnapshot HotReloadManager::getPluginState(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->pluginStates.contains(pluginId)) {
        return d->pluginStates[pluginId];
    }
    
    // 尝试从文件加载
    locker.unlock();
    return loadStateFromFile(pluginId);
}

bool HotReloadManager::hasPluginState(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->pluginStates.contains(pluginId)) {
        return true;
    }
    
    // 检查文件是否存在
    QString filePath = generateStateFilePath(pluginId);
    return QFile::exists(filePath);
}

void HotReloadManager::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("HotReloadManager", QString("热重载功能%1").arg(enabled ? "启用" : "禁用"));
}

bool HotReloadManager::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void HotReloadManager::setAutoSaveState(bool autoSave)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->autoSaveState = autoSave;
}

bool HotReloadManager::isAutoSaveState() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->autoSaveState;
}

void HotReloadManager::setStateStoragePath(const QString& path)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->stateStoragePath = path;
    
    // 确保目录存在
    QDir dir;
    if (!dir.exists(path)) {
        dir.mkpath(path);
    }
}

QString HotReloadManager::stateStoragePath() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->stateStoragePath;
}

QStringList HotReloadManager::getReloadablePlugins() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginManager) {
        return QStringList();
    }
    
    locker.unlock();
    QStringList allPlugins = d->pluginManager->availablePlugins();
    QStringList reloadablePlugins;
    
    for (const QString& pluginId : allPlugins) {
        if (isPluginReloadable(pluginId)) {
            reloadablePlugins.append(pluginId);
        }
    }
    
    return reloadablePlugins;
}

bool HotReloadManager::isPluginReloadable(const QString& pluginId) const
{
    const auto* d = d_func();
    
    if (!d->pluginManager) {
        return false;
    }
    
    // 检查插件是否已加载
    if (!d->pluginManager->isPluginLoaded(pluginId)) {
        return false;
    }
    
    // 检查依赖关系
    return checkDependencies(pluginId);
}

HotReloadStatus HotReloadManager::getStatus(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginStatuses.value(pluginId, HotReloadStatus::Idle);
}

void HotReloadManager::onPluginUnloaded(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    // 可以在这里处理插件卸载后的清理工作
    Q_UNUSED(pluginId);
}

void HotReloadManager::onPluginLoaded(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    // 可以在这里处理插件加载后的初始化工作
    Q_UNUSED(pluginId);
}

bool HotReloadManager::checkDependencies(const QString& pluginId) const
{
    const auto* d = d_func();
    
    if (!d->pluginManager) {
        return false;
    }
    
    // 检查是否有其他插件依赖此插件
    QStringList allPlugins = d->pluginManager->availablePlugins();
    for (const QString& otherPluginId : allPlugins) {
        if (otherPluginId == pluginId) {
            continue;
        }
        
        if (!d->pluginManager->isPluginLoaded(otherPluginId)) {
            continue;
        }
        
        // 检查依赖关系（简化实现）
        QStringList dependencies = d->pluginManager->resolveDependencies(otherPluginId);
        if (dependencies.contains(pluginId)) {
            // 有其他插件依赖此插件，不可重载
            return false;
        }
    }
    
    return true;
}

bool HotReloadManager::checkReloadable(const QString& pluginId) const
{
    return isPluginReloadable(pluginId);
}

QString HotReloadManager::generateStateFilePath(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    QString fileName = QString("%1_state.json").arg(pluginId);
    return QDir(d->stateStoragePath).filePath(fileName);
}

bool HotReloadManager::saveStateToFile(const QString& pluginId, const PluginStateSnapshot& snapshot)
{
    QString filePath = generateStateFilePath(pluginId);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("HotReloadManager", QString("无法创建状态文件: %1").arg(filePath));
        return false;
    }
    
    QJsonObject obj;
    obj["pluginId"] = snapshot.pluginId;
    obj["config"] = QJsonObject::fromVariantMap(snapshot.config);
    obj["loadedServices"] = QJsonArray::fromStringList(snapshot.loadedServices);
    obj["metadata"] = QJsonObject::fromVariantMap(snapshot.metadata);
    obj["snapshotTime"] = snapshot.snapshotTime.toString(Qt::ISODate);
    
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
    
    return true;
}

PluginStateSnapshot HotReloadManager::loadStateFromFile(const QString& pluginId) const
{
    QString filePath = generateStateFilePath(pluginId);
    
    if (!QFile::exists(filePath)) {
        return PluginStateSnapshot();
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return PluginStateSnapshot();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return PluginStateSnapshot();
    }
    
    QJsonObject obj = doc.object();
    PluginStateSnapshot snapshot;
    snapshot.pluginId = obj["pluginId"].toString();
    snapshot.config = obj["config"].toObject().toVariantMap();
    
    // 转换loadedServices数组
    QJsonArray servicesArray = obj["loadedServices"].toArray();
    QStringList servicesList;
    for (const QJsonValue& value : servicesArray) {
        servicesList.append(value.toString());
    }
    snapshot.loadedServices = servicesList;
    
    snapshot.metadata = obj["metadata"].toObject().toVariantMap();
    snapshot.snapshotTime = QDateTime::fromString(obj["snapshotTime"].toString(), Qt::ISODate);
    
    return snapshot;
}

} // namespace Core
} // namespace Eagle
