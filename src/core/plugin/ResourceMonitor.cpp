#include "eagle/core/ResourceMonitor.h"
#include "ResourceMonitor_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegExp>
#ifdef __linux__
#include <unistd.h>
#include <sys/resource.h>
#endif
#include <algorithm>

namespace Eagle {
namespace Core {

ResourceMonitor::ResourceMonitor(QObject* parent)
    : QObject(parent)
    , d(new ResourceMonitor::Private)
{
    // 连接监控定时器
    connect(d->monitoringTimer, &QTimer::timeout,
            this, &ResourceMonitor::onMonitoringTimer, Qt::QueuedConnection);
    
    // 启动监控
    if (d->monitoringEnabled) {
        d->monitoringTimer->start();
    }
    
    Logger::info("ResourceMonitor", "资源监控器初始化完成");
}

ResourceMonitor::~ResourceMonitor()
{
    delete d;
}

bool ResourceMonitor::registerPlugin(const QString& pluginId, const ResourceLimits& limits)
{
    if (pluginId.isEmpty()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->pluginLimits.contains(pluginId)) {
        Logger::warning("ResourceMonitor", QString("插件已注册: %1").arg(pluginId));
        return false;
    }
    
    d->pluginLimits[pluginId] = limits;
    
    ResourceUsage usage;
    usage.pluginId = pluginId;
    d->pluginUsage[pluginId] = usage;
    
    Logger::info("ResourceMonitor", QString("插件已注册资源监控: %1").arg(pluginId));
    return true;
}

bool ResourceMonitor::unregisterPlugin(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginLimits.contains(pluginId)) {
        return false;
    }
    
    d->pluginLimits.remove(pluginId);
    d->pluginUsage.remove(pluginId);
    d->limitExceededEvents.remove(pluginId);
    
    Logger::info("ResourceMonitor", QString("插件已注销资源监控: %1").arg(pluginId));
    return true;
}

void ResourceMonitor::setResourceLimits(const QString& pluginId, const ResourceLimits& limits)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginLimits.contains(pluginId)) {
        // 自动注册
        d->pluginLimits[pluginId] = limits;
        ResourceUsage usage;
        usage.pluginId = pluginId;
        d->pluginUsage[pluginId] = usage;
    } else {
        d->pluginLimits[pluginId] = limits;
    }
    
    Logger::info("ResourceMonitor", QString("设置插件资源限制: %1 (内存: %2MB, CPU: %3%)")
        .arg(pluginId)
        .arg(limits.maxMemoryMB < 0 ? "无限制" : QString::number(limits.maxMemoryMB))
        .arg(limits.maxCpuPercent < 0 ? "无限制" : QString::number(limits.maxCpuPercent)));
}

ResourceLimits ResourceMonitor::getResourceLimits(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginLimits.value(pluginId);
}

ResourceUsage ResourceMonitor::getResourceUsage(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginUsage.value(pluginId);
}

QMap<QString, ResourceUsage> ResourceMonitor::getAllResourceUsage() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginUsage;
}

bool ResourceMonitor::isResourceLimitExceeded(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->pluginLimits.contains(pluginId) || !d->pluginUsage.contains(pluginId)) {
        return false;
    }
    
    ResourceLimits limits = d->pluginLimits[pluginId];
    ResourceUsage usage = d->pluginUsage[pluginId];
    
    locker.unlock();
    return checkResourceLimits(pluginId, usage, limits);
}

void ResourceMonitor::setMonitoringEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->monitoringEnabled = enabled;
    
    if (enabled) {
        d->monitoringTimer->start();
        Logger::info("ResourceMonitor", "资源监控已启用");
    } else {
        d->monitoringTimer->stop();
        Logger::info("ResourceMonitor", "资源监控已禁用");
    }
}

bool ResourceMonitor::isMonitoringEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->monitoringEnabled;
}

void ResourceMonitor::setMonitoringInterval(int intervalMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->monitoringIntervalMs = intervalMs;
    d->monitoringTimer->setInterval(intervalMs);
}

int ResourceMonitor::monitoringInterval() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->monitoringIntervalMs;
}

void ResourceMonitor::setEnforcementEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enforcementEnabled = enabled;
    Logger::info("ResourceMonitor", QString("资源限制强制执行%1").arg(enabled ? "启用" : "禁用"));
}

bool ResourceMonitor::isEnforcementEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enforcementEnabled;
}

QList<ResourceLimitExceeded> ResourceMonitor::getLimitExceededEvents(const QString& pluginId, int limit) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<ResourceLimitExceeded> events = d->limitExceededEvents.value(pluginId);
    
    // 按时间倒序排序
    std::sort(events.begin(), events.end(),
              [](const ResourceLimitExceeded& a, const ResourceLimitExceeded& b) {
                  return a.timestamp > b.timestamp;
              });
    
    if (events.size() > limit) {
        events = events.mid(0, limit);
    }
    
    return events;
}

bool ResourceMonitor::clearLimitExceededEvents(const QString& pluginId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->limitExceededEvents.remove(pluginId);
    return true;
}

void ResourceMonitor::onMonitoringTimer()
{
    if (!isMonitoringEnabled()) {
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList pluginIds = d->pluginLimits.keys();
    locker.unlock();
    
    for (const QString& pluginId : pluginIds) {
        updateResourceUsage(pluginId);
    }
}

void ResourceMonitor::updateResourceUsage(const QString& pluginId)
{
    ResourceUsage usage;
    usage.pluginId = pluginId;
    usage.memoryBytes = getPluginMemoryUsage(pluginId);
    usage.cpuPercent = getPluginCpuUsage(pluginId);
    usage.threadCount = getPluginThreadCount(pluginId);
    usage.lastUpdate = QDateTime::currentDateTime();
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->pluginUsage[pluginId] = usage;
    
    // 检查资源限制
    if (d->pluginLimits.contains(pluginId)) {
        ResourceLimits limits = d->pluginLimits[pluginId];
        bool exceeded = checkResourceLimits(pluginId, usage, limits);
        
        if (exceeded && d->enforcementEnabled) {
            // 强制执行限制（可以采取行动，如暂停插件）
            Logger::warning("ResourceMonitor", QString("插件 %1 超出资源限制，强制执行中").arg(pluginId));
        }
    }
    
    emit resourceUsageUpdated(pluginId, usage);
}

bool ResourceMonitor::checkResourceLimits(const QString& pluginId, const ResourceUsage& usage, const ResourceLimits& limits) const
{
    bool exceeded = false;
    
    // 检查内存限制
    if (limits.maxMemoryMB > 0) {
        qint64 maxMemoryBytes = limits.maxMemoryMB * 1024 * 1024;
        if (usage.memoryBytes > maxMemoryBytes) {
            exceeded = true;
            // 在const方法中使用const_cast调用非const方法，因为记录事件不影响逻辑上的const性
            const_cast<ResourceMonitor*>(this)->handleResourceLimitExceeded(pluginId, "memory", 
                                       static_cast<double>(usage.memoryBytes) / 1024 / 1024,
                                       static_cast<double>(limits.maxMemoryMB));
        }
    }
    
    // 检查CPU限制
    if (limits.maxCpuPercent > 0) {
        if (usage.cpuPercent > limits.maxCpuPercent) {
            exceeded = true;
            const_cast<ResourceMonitor*>(this)->handleResourceLimitExceeded(pluginId, "cpu", usage.cpuPercent, static_cast<double>(limits.maxCpuPercent));
        }
    }
    
    // 检查线程限制
    if (limits.maxThreads > 0) {
        if (usage.threadCount > limits.maxThreads) {
            exceeded = true;
            const_cast<ResourceMonitor*>(this)->handleResourceLimitExceeded(pluginId, "threads", 
                                       static_cast<double>(usage.threadCount),
                                       static_cast<double>(limits.maxThreads));
        }
    }
    
    return exceeded;
}

void ResourceMonitor::handleResourceLimitExceeded(const QString& pluginId, const QString& resourceType, 
                                                   double currentValue, double limitValue)
{
    ResourceLimitExceeded event;
    event.pluginId = pluginId;
    event.resourceType = resourceType;
    event.currentValue = currentValue;
    event.limitValue = limitValue;
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->limitExceededEvents[pluginId].append(event);
    
    // 限制事件数量
    if (d->limitExceededEvents[pluginId].size() > 100) {
        d->limitExceededEvents[pluginId].removeFirst();
    }
    
    emit resourceLimitExceeded(pluginId, resourceType, currentValue, limitValue);
    
    Logger::warning("ResourceMonitor", QString("插件 %1 超出%2限制: 当前值=%3, 限制值=%4")
        .arg(pluginId).arg(resourceType).arg(currentValue).arg(limitValue));
}

qint64 ResourceMonitor::getPluginMemoryUsage(const QString& pluginId) const
{
    Q_UNUSED(pluginId);
    // 简化实现：获取进程内存使用
    // 实际应该获取插件特定的内存使用（需要插件进程分离或内存跟踪）
    
#ifdef __linux__
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&statusFile);
        QString content = stream.readAll();
        statusFile.close();
        
        QStringList lines = content.split('\n');
        for (const QString& line : lines) {
            if (line.startsWith("VmRSS:")) {
                QString value = line.split(QRegExp("\\s+")).value(1);
                return value.toLongLong() * 1024;  // KB to bytes
            }
        }
    }
#endif
    
    // 其他平台或无法获取时，返回0
    return 0;
}

double ResourceMonitor::getPluginCpuUsage(const QString& pluginId) const
{
    Q_UNUSED(pluginId);
    // 简化实现：获取进程CPU使用率
    // 实际应该获取插件特定的CPU使用（需要插件进程分离或CPU跟踪）
    
    // 这里简化实现，实际应该跟踪CPU时间
    // 可以通过/proc/self/stat获取进程CPU时间，然后计算使用率
    return 0.0;
}

qint64 ResourceMonitor::getPluginThreadCount(const QString& pluginId) const
{
    Q_UNUSED(pluginId);
    // 简化实现：获取进程线程数
    // 实际应该获取插件特定的线程数（需要线程跟踪）
    
#ifdef __linux__
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&statusFile);
        QString content = stream.readAll();
        statusFile.close();
        
        QStringList lines = content.split('\n');
        for (const QString& line : lines) {
            if (line.startsWith("Threads:")) {
                QString value = line.split(QRegExp("\\s+")).value(1);
                return value.toLongLong();
            }
        }
    }
#endif
    
    return 0;
}

} // namespace Core
} // namespace Eagle
