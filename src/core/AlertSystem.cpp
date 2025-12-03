#include "eagle/core/AlertSystem.h"
#include "AlertSystem_p.h"
#include "eagle/core/PerformanceMonitor.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>
#include <QtCore/QCoreApplication>

namespace Eagle {
namespace Core {

AlertSystem::AlertSystem(PerformanceMonitor* monitor, QObject* parent)
    : QObject(parent)
    , d(new AlertSystem::Private)
{
    d->monitor = monitor;
    
    if (d->monitor) {
        connect(d->monitor, &PerformanceMonitor::metricUpdated,
                this, &AlertSystem::onMetricUpdated, Qt::QueuedConnection);
    }
    
    Logger::info("AlertSystem", "告警系统初始化完成");
}

AlertSystem::~AlertSystem()
{
    delete d;
}

bool AlertSystem::addRule(const AlertRule& rule)
{
    if (!rule.isValid()) {
        Logger::error("AlertSystem", "无效的告警规则");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->rules.contains(rule.id)) {
        Logger::warning("AlertSystem", QString("告警规则已存在: %1").arg(rule.id));
        return false;
    }
    
    d->rules[rule.id] = rule;
    Logger::info("AlertSystem", QString("添加告警规则: %1").arg(rule.name));
    return true;
}

bool AlertSystem::removeRule(const QString& ruleId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->rules.contains(ruleId)) {
        return false;
    }
    
    // 解决该规则的所有告警
    resolveAlertsByRule(ruleId);
    
    d->rules.remove(ruleId);
    d->ruleTriggerTimes.remove(ruleId);
    Logger::info("AlertSystem", QString("移除告警规则: %1").arg(ruleId));
    return true;
}

bool AlertSystem::updateRule(const AlertRule& rule)
{
    if (!rule.isValid()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->rules.contains(rule.id)) {
        return false;
    }
    
    d->rules[rule.id] = rule;
    Logger::info("AlertSystem", QString("更新告警规则: %1").arg(rule.name));
    return true;
}

AlertRule AlertSystem::getRule(const QString& ruleId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->rules.value(ruleId);
}

QStringList AlertSystem::getAllRuleIds() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->rules.keys();
}

QList<AlertRecord> AlertSystem::getActiveAlerts() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->activeAlerts.values();
}

QList<AlertRecord> AlertSystem::getAlerts(const QString& ruleId, AlertLevel minLevel,
                                         const QDateTime& startTime, const QDateTime& endTime) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QList<AlertRecord> results;
    
    // 检查活动告警
    for (const AlertRecord& alert : d->activeAlerts.values()) {
        if (!ruleId.isEmpty() && alert.ruleId != ruleId) continue;
        if (alert.level < minLevel) continue;
        if (startTime.isValid() && alert.triggerTime < startTime) continue;
        if (endTime.isValid() && alert.triggerTime > endTime) continue;
        results.append(alert);
    }
    
    // 检查历史告警
    for (const AlertRecord& alert : d->alertHistory) {
        if (!ruleId.isEmpty() && alert.ruleId != ruleId) continue;
        if (alert.level < minLevel) continue;
        if (startTime.isValid() && alert.triggerTime < startTime) continue;
        if (endTime.isValid() && alert.triggerTime > endTime) continue;
        results.append(alert);
    }
    
    return results;
}

bool AlertSystem::resolveAlert(const QString& alertId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->activeAlerts.contains(alertId)) {
        return false;
    }
    
    AlertRecord alert = d->activeAlerts[alertId];
    alert.resolved = true;
    alert.resolveTime = QDateTime::currentDateTime();
    
    d->activeAlerts.remove(alertId);
    d->alertHistory.append(alert);
    
    // 限制历史记录数量
    if (d->alertHistory.size() > 1000) {
        d->alertHistory.removeFirst();
    }
    
    Logger::info("AlertSystem", QString("解决告警: %1").arg(alertId));
    emit alertResolved(alertId);
    return true;
}

bool AlertSystem::resolveAlertsByRule(const QString& ruleId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList toResolve;
    for (auto it = d->activeAlerts.begin(); it != d->activeAlerts.end(); ++it) {
        if (it.value().ruleId == ruleId) {
            toResolve.append(it.key());
        }
    }
    
    locker.unlock();
    
    for (const QString& alertId : toResolve) {
        resolveAlert(alertId);
    }
    
    return !toResolve.isEmpty();
}

void AlertSystem::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("AlertSystem", QString("告警系统%1").arg(enabled ? "启用" : "禁用"));
}

bool AlertSystem::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void AlertSystem::registerAlertHandler(std::function<void(const AlertRecord&)> handler)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->alertHandler = handler;
    Logger::info("AlertSystem", "注册告警处理器");
}

void AlertSystem::onMetricUpdated(const QString& name, double value)
{
    if (!isEnabled()) {
        return;
    }
    
    checkRules(name, value);
}

void AlertSystem::checkRules(const QString& metricName, double value)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    for (auto it = d->rules.begin(); it != d->rules.end(); ++it) {
        const AlertRule& rule = it.value();
        
        if (!rule.enabled || rule.metricName != metricName) {
            continue;
        }
        
        if (rule.check(value)) {
            // 检查持续时间
            if (rule.durationMs > 0) {
                QDateTime now = QDateTime::currentDateTime();
                if (!d->ruleTriggerTimes.contains(rule.id)) {
                    d->ruleTriggerTimes[rule.id] = now;
                    continue;  // 第一次触发，记录时间但不告警
                }
                
                QDateTime triggerTime = d->ruleTriggerTimes[rule.id];
                if (triggerTime.msecsTo(now) < rule.durationMs) {
                    continue;  // 持续时间未到
                }
            }
            
            // 检查是否已有活动告警
            bool hasActiveAlert = false;
            for (const AlertRecord& alert : d->activeAlerts.values()) {
                if (alert.ruleId == rule.id && !alert.resolved) {
                    hasActiveAlert = true;
                    break;
                }
            }
            
            if (!hasActiveAlert) {
                // 创建新告警
                AlertRecord alert;
                alert.id = generateAlertId();
                alert.ruleId = rule.id;
                alert.metricName = metricName;
                alert.level = rule.level;
                alert.value = value;
                alert.threshold = rule.threshold;
                alert.triggerTime = QDateTime::currentDateTime();
                alert.message = QString("%1: %2 %3 %4 (当前值: %5)")
                    .arg(rule.name, metricName, rule.condition, QString::number(rule.threshold), QString::number(value));
                
                d->activeAlerts[alert.id] = alert;
                
                Logger::warning("AlertSystem", QString("告警触发: %1").arg(alert.message));
                
                // 调用处理器
                if (d->alertHandler) {
                    d->alertHandler(alert);
                }
                
                emit alertTriggered(alert);
            }
        } else {
            // 条件不满足，清除触发时间
            d->ruleTriggerTimes.remove(rule.id);
            
            // 解决相关告警
            QStringList toResolve;
            for (auto alertIt = d->activeAlerts.begin(); alertIt != d->activeAlerts.end(); ++alertIt) {
                if (alertIt.value().ruleId == rule.id && !alertIt.value().resolved) {
                    toResolve.append(alertIt.key());
                }
            }
            
            for (const QString& alertId : toResolve) {
                AlertRecord alert = d->activeAlerts[alertId];
                alert.resolved = true;
                alert.resolveTime = QDateTime::currentDateTime();
                d->activeAlerts.remove(alertId);
                d->alertHistory.append(alert);
                emit alertResolved(alertId);
            }
        }
    }
}

QString AlertSystem::generateAlertId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace Core
} // namespace Eagle
