#include "eagle/core/FailoverManager.h"
#include "FailoverManager_p.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/Logger.h"
#include <algorithm>
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

FailoverManager::FailoverManager(ServiceRegistry* serviceRegistry, QObject* parent)
    : QObject(parent)
    , d(new FailoverManager::Private(serviceRegistry))
{
    if (!serviceRegistry) {
        Logger::error("FailoverManager", "ServiceRegistry不能为空");
        return;
    }
    
    // 连接定时器
    connect(d->healthCheckTimer, &QTimer::timeout,
            this, &FailoverManager::onHealthCheckTimer, Qt::QueuedConnection);
    connect(d->stateSyncTimer, &QTimer::timeout,
            this, &FailoverManager::onStateSyncTimer, Qt::QueuedConnection);
    
    Logger::info("FailoverManager", "故障转移管理器初始化完成");
}

FailoverManager::~FailoverManager()
{
    delete d;
}

bool FailoverManager::registerService(const FailoverConfig& config)
{
    if (!config.isValid()) {
        Logger::error("FailoverManager", "无效的故障转移配置");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->serviceConfigs.contains(config.serviceName)) {
        Logger::warning("FailoverManager", QString("服务已注册: %1").arg(config.serviceName));
        return false;
    }
    
    d->serviceConfigs[config.serviceName] = config;
    d->serviceEnabled[config.serviceName] = true;
    
    // 初始化节点
    d->serviceNodes[config.serviceName] = QMap<QString, ServiceNode>();
    
    // 设置当前主节点
    if (!config.primaryNodes.isEmpty()) {
        d->currentPrimaries[config.serviceName] = config.primaryNodes.first();
    }
    
    // 启动健康检查定时器（如果还未启动）
    if (!d->healthCheckTimer->isActive() && d->enabled) {
        d->healthCheckTimer->start();
    }
    
    // 启动状态同步定时器（如果启用）
    if (config.enableStateSync && !d->stateSyncTimer->isActive() && d->enabled) {
        d->stateSyncTimer->start();
    }
    
    Logger::info("FailoverManager", QString("服务已注册: %1 (主节点: %2, 备节点: %3)")
        .arg(config.serviceName)
        .arg(config.primaryNodes.size())
        .arg(config.standbyNodes.size()));
    
    return true;
}

bool FailoverManager::unregisterService(const QString& serviceName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceConfigs.contains(serviceName)) {
        return false;
    }
    
    d->serviceConfigs.remove(serviceName);
    d->serviceNodes.remove(serviceName);
    d->currentPrimaries.remove(serviceName);
    d->failoverHistory.remove(serviceName);
    d->serviceEnabled.remove(serviceName);
    
    Logger::info("FailoverManager", QString("服务已注销: %1").arg(serviceName));
    return true;
}

FailoverConfig FailoverManager::getServiceConfig(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->serviceConfigs.value(serviceName);
}

QStringList FailoverManager::getRegisteredServices() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->serviceConfigs.keys();
}

bool FailoverManager::addNode(const QString& serviceName, const ServiceNode& node)
{
    if (!node.isValid()) {
        Logger::error("FailoverManager", "无效的服务节点");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceConfigs.contains(serviceName)) {
        Logger::error("FailoverManager", QString("服务未注册: %1").arg(serviceName));
        return false;
    }
    
    d->serviceNodes[serviceName][node.id] = node;
    
    Logger::info("FailoverManager", QString("节点已添加: %1/%2").arg(serviceName).arg(node.id));
    return true;
}

bool FailoverManager::removeNode(const QString& serviceName, const QString& nodeId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName) || !d->serviceNodes[serviceName].contains(nodeId)) {
        return false;
    }
    
    d->serviceNodes[serviceName].remove(nodeId);
    
    // 如果移除的是主节点，触发故障转移
    if (d->currentPrimaries.value(serviceName) == nodeId) {
        locker.unlock();
        triggerFailover(serviceName, QString("主节点被移除: %1").arg(nodeId));
    }
    
    Logger::info("FailoverManager", QString("节点已移除: %1/%2").arg(serviceName).arg(nodeId));
    return true;
}

ServiceNode FailoverManager::getNode(const QString& serviceName, const QString& nodeId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName)) {
        return ServiceNode();
    }
    
    return d->serviceNodes[serviceName].value(nodeId);
}

QList<ServiceNode> FailoverManager::getNodes(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName)) {
        return QList<ServiceNode>();
    }
    
    return d->serviceNodes[serviceName].values();
}

bool FailoverManager::performFailover(const QString& serviceName, const QString& targetNodeId)
{
    auto* d = d_func();
    
    if (!d->enabled || !d->serviceEnabled.value(serviceName, false)) {
        Logger::warning("FailoverManager", QString("故障转移已禁用: %1").arg(serviceName));
        return false;
    }
    
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceConfigs.contains(serviceName)) {
        Logger::error("FailoverManager", QString("服务未注册: %1").arg(serviceName));
        return false;
    }
    
    FailoverConfig config = d->serviceConfigs[serviceName];
    QString currentPrimaryId = d->currentPrimaries.value(serviceName);
    
    // 确定目标节点
    QString newPrimaryId = targetNodeId;
    if (newPrimaryId.isEmpty()) {
        // 自动选择第一个备节点
        QList<ServiceNode> standbyNodes = getStandbyNodes(serviceName);
        if (standbyNodes.isEmpty()) {
            Logger::error("FailoverManager", QString("没有可用的备节点: %1").arg(serviceName));
            return false;
        }
        
        // 选择最健康的备节点
        ServiceNode bestNode = standbyNodes.first();
        for (const ServiceNode& node : standbyNodes) {
            if (node.status == ServiceStatus::Healthy && 
                node.consecutiveFailures < bestNode.consecutiveFailures) {
                bestNode = node;
            }
        }
        newPrimaryId = bestNode.id;
    }
    
    if (newPrimaryId == currentPrimaryId) {
        Logger::warning("FailoverManager", QString("目标节点已是主节点: %1").arg(serviceName));
        return true;
    }
    
    locker.unlock();
    
    // 执行切换
    bool success = switchToPrimary(serviceName, newPrimaryId);
    
    if (success && !currentPrimaryId.isEmpty()) {
        switchToStandby(serviceName, currentPrimaryId);
    }
    
    // 记录事件
    FailoverEvent event;
    event.serviceName = serviceName;
    event.fromNodeId = currentPrimaryId;
    event.toNodeId = newPrimaryId;
    event.reason = targetNodeId.isEmpty() ? "自动故障转移" : "手动故障转移";
    event.success = success;
    
    recordFailoverEvent(event);
    
    if (success) {
        emit failoverCompleted(serviceName, true);
        Logger::info("FailoverManager", QString("故障转移成功: %1 (%2 -> %3)")
            .arg(serviceName).arg(currentPrimaryId).arg(newPrimaryId));
    } else {
        emit failoverCompleted(serviceName, false);
        Logger::error("FailoverManager", QString("故障转移失败: %1").arg(serviceName));
    }
    
    return success;
}

bool FailoverManager::switchToPrimary(const QString& serviceName, const QString& nodeId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName) || !d->serviceNodes[serviceName].contains(nodeId)) {
        return false;
    }
    
    ServiceNode& node = d->serviceNodes[serviceName][nodeId];
    node.role = ServiceRole::Primary;
    d->currentPrimaries[serviceName] = nodeId;
    
    emit failoverTriggered(serviceName, QString(), nodeId);
    
    return true;
}

bool FailoverManager::switchToStandby(const QString& serviceName, const QString& nodeId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName) || !d->serviceNodes[serviceName].contains(nodeId)) {
        return false;
    }
    
    ServiceNode& node = d->serviceNodes[serviceName][nodeId];
    node.role = ServiceRole::Standby;
    
    return true;
}

ServiceNode FailoverManager::getCurrentPrimary(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString primaryId = d->currentPrimaries.value(serviceName);
    if (primaryId.isEmpty() || !d->serviceNodes.contains(serviceName)) {
        return ServiceNode();
    }
    
    return d->serviceNodes[serviceName].value(primaryId);
}

QList<ServiceNode> FailoverManager::getStandbyNodes(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->serviceNodes.contains(serviceName)) {
        return QList<ServiceNode>();
    }
    
    QList<ServiceNode> standbyNodes;
    for (const ServiceNode& node : d->serviceNodes[serviceName].values()) {
        if (node.role == ServiceRole::Standby) {
            standbyNodes.append(node);
        }
    }
    
    return standbyNodes;
}

ServiceStatus FailoverManager::getServiceStatus(const QString& serviceName) const
{
    ServiceNode primary = getCurrentPrimary(serviceName);
    if (primary.id.isEmpty()) {
        return ServiceStatus::Failed;
    }
    return primary.status;
}

QList<FailoverEvent> FailoverManager::getFailoverHistory(const QString& serviceName, int limit) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<FailoverEvent> events = d->failoverHistory.value(serviceName);
    
    // 按时间倒序排序，返回最近的N条
    std::sort(events.begin(), events.end(),
              [](const FailoverEvent& a, const FailoverEvent& b) {
                  return a.timestamp > b.timestamp;
              });
    
    if (events.size() > limit) {
        events = events.mid(0, limit);
    }
    
    return events;
}

void FailoverManager::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    
    if (enabled) {
        d->healthCheckTimer->start();
        d->stateSyncTimer->start();
    } else {
        d->healthCheckTimer->stop();
        d->stateSyncTimer->stop();
    }
    
    Logger::info("FailoverManager", QString("故障转移功能%1").arg(enabled ? "启用" : "禁用"));
}

bool FailoverManager::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void FailoverManager::setServiceEnabled(const QString& serviceName, bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->serviceEnabled[serviceName] = enabled;
}

bool FailoverManager::isServiceEnabled(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->serviceEnabled.value(serviceName, false);
}

void FailoverManager::onHealthCheckTimer()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList services = d->serviceConfigs.keys();
    locker.unlock();
    
    for (const QString& serviceName : services) {
        if (!d->serviceEnabled.value(serviceName, false)) {
            continue;
        }
        
        FailoverConfig config = getServiceConfig(serviceName);
        QList<ServiceNode> nodes = getNodes(serviceName);
        
        for (const ServiceNode& node : nodes) {
            bool healthy = performHealthCheck(serviceName, node.id);
            
            if (!healthy) {
                emit healthCheckFailed(serviceName, node.id);
                
                // 如果是主节点且达到失败阈值，触发故障转移
                if (node.role == ServiceRole::Primary && 
                    config.mode == FailoverMode::Automatic) {
                    ServiceNode updatedNode = getNode(serviceName, node.id);
                    if (updatedNode.consecutiveFailures >= config.failureThreshold) {
                        triggerFailover(serviceName, QString("主节点健康检查失败: %1").arg(node.id));
                    }
                }
            }
        }
    }
}

void FailoverManager::onStateSyncTimer()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList services = d->serviceConfigs.keys();
    locker.unlock();
    
    for (const QString& serviceName : services) {
        if (!d->serviceEnabled.value(serviceName, false)) {
            continue;
        }
        
        FailoverConfig config = getServiceConfig(serviceName);
        if (!config.enableStateSync) {
            continue;
        }
        
        ServiceNode primary = getCurrentPrimary(serviceName);
        if (primary.id.isEmpty()) {
            continue;
        }
        
        QList<ServiceNode> standbyNodes = getStandbyNodes(serviceName);
        for (const ServiceNode& standby : standbyNodes) {
            if (standby.status == ServiceStatus::Healthy) {
                syncState(serviceName, primary, standby);
            }
        }
    }
}

bool FailoverManager::performHealthCheck(const QString& serviceName, const QString& nodeId)
{
    ServiceNode node = getNode(serviceName, nodeId);
    if (node.id.isEmpty()) {
        return false;
    }
    
    bool healthy = checkNodeHealth(node);
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    ServiceNode& updatedNode = d->serviceNodes[serviceName][nodeId];
    updatedNode.lastHealthCheck = QDateTime::currentDateTime();
    
    if (healthy) {
        updatedNode.consecutiveFailures = 0;
        if (updatedNode.status != ServiceStatus::Healthy) {
            updatedNode.status = ServiceStatus::Healthy;
            locker.unlock();
            updateNodeStatus(serviceName, nodeId, ServiceStatus::Healthy);
            locker.relock();
        }
    } else {
        updatedNode.consecutiveFailures++;
        if (updatedNode.status == ServiceStatus::Healthy) {
            updatedNode.status = ServiceStatus::Degraded;
            locker.unlock();
            updateNodeStatus(serviceName, nodeId, ServiceStatus::Degraded);
            locker.relock();
        }
        
        FailoverConfig config = d->serviceConfigs[serviceName];
        if (updatedNode.consecutiveFailures >= config.failureThreshold) {
            updatedNode.status = ServiceStatus::Failed;
            locker.unlock();
            updateNodeStatus(serviceName, nodeId, ServiceStatus::Failed);
            locker.relock();
        }
    }
    
    return healthy;
}

bool FailoverManager::checkNodeHealth(const ServiceNode& node) const
{
    // 简化实现：检查端点是否可访问
    // 实际应该调用服务的健康检查接口
    QUrl url(node.endpoint + "/health");
    if (!url.isValid()) {
        return false;
    }
    
    // 使用QNetworkAccessManager进行HTTP健康检查
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Eagle-Framework/1.0");
    
    QNetworkReply* reply = manager.get(request);
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(3000);  // 3秒超时
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    
    timer.start();
    loop.exec();
    
    bool healthy = false;
    if (reply->error() == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        healthy = (statusCode >= 200 && statusCode < 300);
    }
    
    reply->deleteLater();
    return healthy;
}

void FailoverManager::updateNodeStatus(const QString& serviceName, const QString& nodeId, ServiceStatus status)
{
    emit nodeStatusChanged(serviceName, nodeId, status);
}

void FailoverManager::triggerFailover(const QString& serviceName, const QString& reason)
{
    FailoverConfig config = getServiceConfig(serviceName);
    if (config.mode != FailoverMode::Automatic) {
        Logger::info("FailoverManager", QString("故障转移模式为手动，不自动切换: %1").arg(serviceName));
        return;
    }
    
    Logger::warning("FailoverManager", QString("触发故障转移: %1, 原因: %2").arg(serviceName).arg(reason));
    performFailover(serviceName);
}

bool FailoverManager::syncState(const QString& serviceName, const ServiceNode& from, const ServiceNode& to)
{
    // 简化实现：状态同步逻辑
    // 实际应该根据服务类型实现具体的同步逻辑
    Q_UNUSED(serviceName);
    Q_UNUSED(from);
    Q_UNUSED(to);
    
    // TODO: 实现状态同步
    return true;
}

void FailoverManager::recordFailoverEvent(const FailoverEvent& event)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->failoverHistory[event.serviceName].append(event);
    
    // 限制历史记录数量
    if (d->failoverHistory[event.serviceName].size() > 100) {
        d->failoverHistory[event.serviceName].removeFirst();
    }
}

} // namespace Core
} // namespace Eagle
