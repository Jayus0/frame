#include "eagle/core/LoadBalancer.h"
#include "LoadBalancer_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QCryptographicHash>
#include <QtCore/QRandomGenerator>
#include <QtCore/QVariant>
#include <climits>
#include <algorithm>

namespace Eagle {
namespace Core {

LoadBalancer::LoadBalancer(QObject* parent)
    : QObject(parent)
    , d(new LoadBalancer::Private)
{
    Logger::info("LoadBalancer", "负载均衡器初始化完成");
}

LoadBalancer::~LoadBalancer()
{
    delete d;
}

QString LoadBalancer::generateInstanceId(const ServiceDescriptor& descriptor) const
{
    // 生成实例ID：serviceName@version@provider地址
    QString baseId = QString("%1@%2").arg(descriptor.serviceName, descriptor.version);
    if (descriptor.provider) {
        baseId.append(QString("@%1").arg(reinterpret_cast<quintptr>(descriptor.provider), 0, 16));
    }
    return baseId;
}

QString LoadBalancer::getInstanceIdByProvider(const QString& serviceName, QObject* provider) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->instances.contains(serviceName)) {
        return QString();
    }
    
    for (auto it = d->instances[serviceName].begin(); it != d->instances[serviceName].end(); ++it) {
        if (it->provider == provider) {
            return it.key();
        }
    }
    
    return QString();
}

bool LoadBalancer::registerInstance(const QString& serviceName, const ServiceDescriptor& descriptor, 
                                   QObject* provider, int weight)
{
    if (serviceName.isEmpty() || !descriptor.isValid() || !provider) {
        Logger::error("LoadBalancer", "无效的服务实例参数");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString instanceId = generateInstanceId(descriptor);
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        Logger::warning("LoadBalancer", QString("服务实例已存在: %1/%2").arg(serviceName, instanceId));
        return false;
    }
    
    ServiceInstance instance;
    instance.descriptor = descriptor;
    instance.descriptor.provider = provider;
    instance.provider = provider;
    instance.weight = weight > 0 ? weight : 1;
    instance.healthy = true;
    
    d->instances[serviceName][instanceId] = instance;
    
    // 初始化负载均衡算法（如果未设置）
    if (!d->algorithms.contains(serviceName)) {
        d->algorithms[serviceName] = LoadBalanceAlgorithm::RoundRobin;
    }
    
    Logger::info("LoadBalancer", QString("服务实例注册成功: %1/%2 (权重: %3)")
        .arg(serviceName, instanceId).arg(instance.weight));
    
    emit instanceRegistered(serviceName, instanceId);
    return true;
}

bool LoadBalancer::unregisterInstance(const QString& serviceName, const QString& instanceId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->instances.contains(serviceName)) {
        return false;
    }
    
    if (!d->instances[serviceName].contains(instanceId)) {
        return false;
    }
    
    d->instances[serviceName].remove(instanceId);
    
    // 如果该服务没有实例了，清理相关数据
    if (d->instances[serviceName].isEmpty()) {
        d->instances.remove(serviceName);
        d->algorithms.remove(serviceName);
        d->roundRobinIndices.remove(serviceName);
        d->weightedRoundRobinWeights.remove(serviceName);
        d->ipHashMappings.remove(serviceName);
    }
    
    Logger::info("LoadBalancer", QString("服务实例注销成功: %1/%2").arg(serviceName, instanceId));
    
    emit instanceUnregistered(serviceName, instanceId);
    return true;
}

QList<ServiceInstance*> LoadBalancer::getHealthyInstances(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<ServiceInstance*> healthyInstances;
    
    if (!d->instances.contains(serviceName)) {
        return healthyInstances;
    }
    
    for (auto it = d->instances[serviceName].begin(); it != d->instances[serviceName].end(); ++it) {
        if (it->healthy && it->isValid()) {
            healthyInstances.append(&(*it));
        }
    }
    
    return healthyInstances;
}

ServiceInstance* LoadBalancer::selectInstance(const QString& serviceName, const QString& clientId)
{
    if (!isEnabled()) {
        return nullptr;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->instances.contains(serviceName) || d->instances[serviceName].isEmpty()) {
        return nullptr;
    }
    
    LoadBalanceAlgorithm algorithm = d->algorithms.value(serviceName, LoadBalanceAlgorithm::RoundRobin);
    locker.unlock();
    
    ServiceInstance* selected = nullptr;
    
    switch (algorithm) {
    case LoadBalanceAlgorithm::RoundRobin:
        selected = selectRoundRobin(serviceName);
        break;
    case LoadBalanceAlgorithm::WeightedRoundRobin:
        selected = selectWeightedRoundRobin(serviceName);
        break;
    case LoadBalanceAlgorithm::LeastConnections:
        selected = selectLeastConnections(serviceName);
        break;
    case LoadBalanceAlgorithm::Random:
        selected = selectRandom(serviceName);
        break;
    case LoadBalanceAlgorithm::IPHash:
        selected = selectIPHash(serviceName, clientId);
        break;
    default:
        selected = selectRoundRobin(serviceName);
        break;
    }
    
    return selected;
}

ServiceInstance* LoadBalancer::selectRoundRobin(const QString& serviceName)
{
    QList<ServiceInstance*> healthyInstances = getHealthyInstances(serviceName);
    if (healthyInstances.isEmpty()) {
        return nullptr;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    int& currentIndex = d->roundRobinIndices[serviceName];
    currentIndex = (currentIndex + 1) % healthyInstances.size();
    
    return healthyInstances[currentIndex];
}

ServiceInstance* LoadBalancer::selectWeightedRoundRobin(const QString& serviceName)
{
    QList<ServiceInstance*> healthyInstances = getHealthyInstances(serviceName);
    if (healthyInstances.isEmpty()) {
        return nullptr;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 计算总权重
    int totalWeight = 0;
    for (ServiceInstance* instance : healthyInstances) {
        totalWeight += instance->weight;
    }
    
    if (totalWeight == 0) {
        return healthyInstances.first();
    }
    
    // 加权轮询算法
    int& currentWeight = d->weightedRoundRobinWeights[serviceName];
    currentWeight = (currentWeight + 1) % totalWeight;
    
    int accumulatedWeight = 0;
    for (ServiceInstance* instance : healthyInstances) {
        accumulatedWeight += instance->weight;
        if (currentWeight < accumulatedWeight) {
            return instance;
        }
    }
    
    return healthyInstances.first();
}

ServiceInstance* LoadBalancer::selectLeastConnections(const QString& serviceName)
{
    QList<ServiceInstance*> healthyInstances = getHealthyInstances(serviceName);
    if (healthyInstances.isEmpty()) {
        return nullptr;
    }
    
    // 找到连接数最少的实例
    ServiceInstance* selected = nullptr;
    int minConnections = INT_MAX;
    
    for (ServiceInstance* instance : healthyInstances) {
        if (instance->activeConnections < minConnections) {
            minConnections = instance->activeConnections;
            selected = instance;
        }
    }
    
    return selected;
}

ServiceInstance* LoadBalancer::selectRandom(const QString& serviceName)
{
    QList<ServiceInstance*> healthyInstances = getHealthyInstances(serviceName);
    if (healthyInstances.isEmpty()) {
        return nullptr;
    }
    
    QRandomGenerator* rng = QRandomGenerator::global();
    int index = rng->bounded(healthyInstances.size());
    
    return healthyInstances[index];
}

ServiceInstance* LoadBalancer::selectIPHash(const QString& serviceName, const QString& clientId)
{
    QList<ServiceInstance*> healthyInstances = getHealthyInstances(serviceName);
    if (healthyInstances.isEmpty()) {
        return nullptr;
    }
    
    if (clientId.isEmpty()) {
        // 如果没有clientId，回退到轮询
        return selectRoundRobin(serviceName);
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 检查是否已有映射
    if (d->ipHashMappings.contains(serviceName) && 
        d->ipHashMappings[serviceName].contains(clientId)) {
        QString instanceId = d->ipHashMappings[serviceName][clientId];
        
        // 验证实例是否仍然存在且健康
        if (d->instances.contains(serviceName) && 
            d->instances[serviceName].contains(instanceId)) {
            ServiceInstance& instance = d->instances[serviceName][instanceId];
            if (instance.healthy && instance.isValid()) {
                return &instance;
            }
        }
    }
    
    // 使用IP哈希选择实例
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(clientId.toUtf8());
    QByteArray hashValue = hash.result();
    
    // 将哈希值转换为整数索引
    quint32 hashInt = 0;
    for (int i = 0; i < qMin(4, hashValue.size()); ++i) {
        hashInt = (hashInt << 8) | static_cast<unsigned char>(hashValue[i]);
    }
    
    int index = hashInt % healthyInstances.size();
    ServiceInstance* selected = healthyInstances[index];
    
    // 保存映射（会话保持）
    QString instanceId = generateInstanceId(selected->descriptor);
    d->ipHashMappings[serviceName][clientId] = instanceId;
    
    return selected;
}

void LoadBalancer::onServiceCallStart(const QString& serviceName, const QString& instanceId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        ServiceInstance& instance = d->instances[serviceName][instanceId];
        instance.activeConnections++;
        instance.totalRequests++;
    }
}

void LoadBalancer::onServiceCallEnd(const QString& serviceName, const QString& instanceId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        ServiceInstance& instance = d->instances[serviceName][instanceId];
        if (instance.activeConnections > 0) {
            instance.activeConnections--;
        }
    }
}

void LoadBalancer::setInstanceHealth(const QString& serviceName, const QString& instanceId, bool healthy)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        ServiceInstance& instance = d->instances[serviceName][instanceId];
        bool oldHealth = instance.healthy;
        instance.healthy = healthy;
        
        if (oldHealth != healthy) {
            locker.unlock();
            emit instanceHealthChanged(serviceName, instanceId, healthy);
            Logger::info("LoadBalancer", QString("服务实例健康状态变更: %1/%2 -> %3")
                .arg(serviceName, instanceId, healthy ? "健康" : "不健康"));
        }
    }
}

void LoadBalancer::setAlgorithm(const QString& serviceName, LoadBalanceAlgorithm algorithm)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->algorithms[serviceName] = algorithm;
    Logger::info("LoadBalancer", QString("设置负载均衡算法: %1 -> %2")
        .arg(serviceName, QString::number(static_cast<int>(algorithm))));
}

LoadBalanceAlgorithm LoadBalancer::getAlgorithm(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->algorithms.value(serviceName, LoadBalanceAlgorithm::RoundRobin);
}

QList<ServiceInstance> LoadBalancer::getInstances(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<ServiceInstance> result;
    if (d->instances.contains(serviceName)) {
        for (auto it = d->instances[serviceName].begin(); it != d->instances[serviceName].end(); ++it) {
            result.append(*it);
        }
    }
    return result;
}

QMap<QString, QVariant> LoadBalancer::getInstanceStats(const QString& serviceName, const QString& instanceId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QMap<QString, QVariant> stats;
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        const ServiceInstance& instance = d->instances[serviceName][instanceId];
        stats["instanceId"] = instanceId;
        stats["weight"] = instance.weight;
        stats["activeConnections"] = instance.activeConnections;
        stats["totalRequests"] = instance.totalRequests;
        stats["healthy"] = instance.healthy;
        stats["serviceName"] = instance.descriptor.serviceName;
        stats["version"] = instance.descriptor.version;
    }
    
    return stats;
}

void LoadBalancer::setInstanceWeight(const QString& serviceName, const QString& instanceId, int weight)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->instances.contains(serviceName) && d->instances[serviceName].contains(instanceId)) {
        ServiceInstance& instance = d->instances[serviceName][instanceId];
        instance.weight = weight > 0 ? weight : 1;
        Logger::info("LoadBalancer", QString("设置实例权重: %1/%2 -> %3")
            .arg(serviceName, instanceId).arg(instance.weight));
    }
}

void LoadBalancer::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("LoadBalancer", QString("负载均衡%1").arg(enabled ? "启用" : "禁用"));
}

bool LoadBalancer::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

} // namespace Core
} // namespace Eagle
