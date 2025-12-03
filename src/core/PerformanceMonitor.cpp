#include "eagle/core/PerformanceMonitor.h"
#include "PerformanceMonitor_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QRegExp>

namespace Eagle {
namespace Core {

PerformanceMonitor::PerformanceMonitor(QObject* parent)
    : QObject(parent)
    , d(new PerformanceMonitor::Private)
{
    d->updateTimer = new QTimer(this);
    connect(d->updateTimer, &QTimer::timeout, this, &PerformanceMonitor::onUpdateTimer);
    d->updateTimer->start(d->updateIntervalMs);
    
    // 初始化系统指标
    updateSystemMetrics();
    
    Logger::info("PerformanceMonitor", "性能监控器初始化完成");
}

PerformanceMonitor::~PerformanceMonitor()
{
    if (d->updateTimer) {
        d->updateTimer->stop();
    }
    delete d;
}

void PerformanceMonitor::updateMetric(const QString& name, double value)
{
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        
        if (!d->metrics.contains(name)) {
            d->metrics[name] = PerformanceMetric();
            d->metrics[name].name = name;
        }
        
        d->metrics[name].update(value);
    }
    
    emit metricUpdated(name, value);
}

void PerformanceMonitor::recordPluginLoadTime(const QString& pluginId, qint64 loadTimeMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->pluginLoadTimes[pluginId] = loadTimeMs;
    
    QString metricName = QString("plugin.load.%1").arg(pluginId);
    updateMetric(metricName, static_cast<double>(loadTimeMs));
    
    Logger::info("PerformanceMonitor", QString("记录插件加载时间: %1 - %2ms").arg(pluginId).arg(loadTimeMs));
}

void PerformanceMonitor::recordServiceCallTime(const QString& serviceName, const QString& method, qint64 callTimeMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->serviceCallTimes[serviceName][method] = callTimeMs;
    
    QString metricName = QString("service.call.%1.%2").arg(serviceName, method);
    updateMetric(metricName, static_cast<double>(callTimeMs));
}

PerformanceMetric PerformanceMonitor::getMetric(const QString& name) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->metrics.value(name);
}

QMap<QString, PerformanceMetric> PerformanceMonitor::getAllMetrics() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->metrics;
}

double PerformanceMonitor::getCpuUsage() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->cpuUsage;
}

qint64 PerformanceMonitor::getMemoryUsage() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->memoryUsage;
}

qint64 PerformanceMonitor::getMemoryUsageMB() const
{
    return getMemoryUsage() / (1024 * 1024);
}

qint64 PerformanceMonitor::getPluginLoadTime(const QString& pluginId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginLoadTimes.value(pluginId, -1);
}

QMap<QString, qint64> PerformanceMonitor::getAllPluginLoadTimes() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->pluginLoadTimes;
}

qint64 PerformanceMonitor::getServiceCallTime(const QString& serviceName, const QString& method) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (d->serviceCallTimes.contains(serviceName)) {
        return d->serviceCallTimes[serviceName].value(method, -1);
    }
    return -1;
}

QMap<QString, qint64> PerformanceMonitor::getServiceCallTimes(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->serviceCallTimes.value(serviceName);
}

void PerformanceMonitor::setUpdateInterval(int intervalMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->updateIntervalMs = intervalMs;
    if (d->updateTimer) {
        d->updateTimer->setInterval(intervalMs);
    }
    Logger::info("PerformanceMonitor", QString("设置更新间隔: %1ms").arg(intervalMs));
}

void PerformanceMonitor::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    if (d->updateTimer) {
        if (enabled) {
            d->updateTimer->start();
        } else {
            d->updateTimer->stop();
        }
    }
    Logger::info("PerformanceMonitor", QString("性能监控%1").arg(enabled ? "启用" : "禁用"));
}

bool PerformanceMonitor::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void PerformanceMonitor::resetMetrics()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    for (auto& metric : d->metrics) {
        metric.reset();
    }
    Logger::info("PerformanceMonitor", "重置所有性能指标");
}

void PerformanceMonitor::resetMetric(const QString& name)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (d->metrics.contains(name)) {
        d->metrics[name].reset();
        Logger::info("PerformanceMonitor", QString("重置性能指标: %1").arg(name));
    }
}

void PerformanceMonitor::onUpdateTimer()
{
    if (!isEnabled()) {
        return;
    }
    
    updateSystemMetrics();
}

void PerformanceMonitor::updateSystemMetrics()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 更新CPU使用率（简化实现，实际应该读取/proc/stat）
#ifdef Q_OS_LINUX
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly)) {
        QTextStream stream(&statFile);
        QString line = stream.readLine();
        if (line.startsWith("cpu ")) {
            QStringList parts = line.split(QRegExp("\\s+"));
            if (parts.size() >= 8) {
                qint64 user = parts[1].toLongLong();
                qint64 nice = parts[2].toLongLong();
                qint64 system = parts[3].toLongLong();
                qint64 idle = parts[4].toLongLong();
                qint64 iowait = parts[5].toLongLong();
                
                qint64 total = user + nice + system + idle + iowait;
                qint64 idleTime = idle;
                
                if (d->lastCpuTotal > 0) {
                    qint64 totalDiff = total - d->lastCpuTotal;
                    qint64 idleDiff = idleTime - d->lastCpuIdle;
                    if (totalDiff > 0) {
                        d->cpuUsage = 100.0 * (1.0 - static_cast<double>(idleDiff) / totalDiff);
                    }
                }
                
                d->lastCpuTotal = total;
                d->lastCpuIdle = idleTime;
            }
        }
        statFile.close();
    }
#else
    // Windows/macOS 简化实现
    d->cpuUsage = 0.0;  // 占位实现
#endif
    
    // 更新内存使用（简化实现）
    QProcess process;
#ifdef Q_OS_LINUX
    process.start("ps", QStringList() << "-o" << "rss=" << "-p" << QString::number(QCoreApplication::applicationPid()));
    if (process.waitForFinished(1000)) {
        QByteArray output = process.readAllStandardOutput();
        d->memoryUsage = output.trimmed().toLongLong() * 1024;  // KB转字节
    }
#else
    // Windows/macOS 简化实现
    d->memoryUsage = 0;  // 占位实现
#endif
    
    locker.unlock();
    
    // 更新指标（在锁外，避免递归锁定）
    updateMetric("system.cpu.usage", d->cpuUsage);
    updateMetric("system.memory.usage", static_cast<double>(d->memoryUsage));
    
    emit cpuUsageChanged(d->cpuUsage);
    emit memoryUsageChanged(d->memoryUsage);
}

} // namespace Core
} // namespace Eagle
