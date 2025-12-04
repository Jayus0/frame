#include "eagle/core/DegradationPolicy.h"
#include "DegradationPolicy_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>

namespace Eagle {
namespace Core {

DegradationPolicy::DegradationPolicy(QObject* parent)
    : QObject(parent)
    , d(new DegradationPolicyPrivate)
{
    Logger::info("DegradationPolicy", "降级策略管理器初始化完成");
}

DegradationPolicy::~DegradationPolicy()
{
    delete d;
}

void DegradationPolicy::setPolicy(const QString& serviceName, const DegradationPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("DegradationPolicy", QString("无效的降级策略配置: %1").arg(serviceName));
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    DegradationPolicyConfig policyConfig = config;
    policyConfig.serviceName = serviceName;
    d->policies[serviceName] = policyConfig;
    
    Logger::info("DegradationPolicy", QString("设置降级策略: %1 - 触发条件: %2, 策略: %3")
        .arg(serviceName)
        .arg(static_cast<int>(policyConfig.trigger))
        .arg(static_cast<int>(policyConfig.strategy)));
}

void DegradationPolicy::removePolicy(const QString& serviceName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->policies.remove(serviceName);
    Logger::info("DegradationPolicy", QString("移除降级策略: %1").arg(serviceName));
}

DegradationPolicyConfig DegradationPolicy::getPolicy(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->policies.contains(serviceName)) {
        return d->policies[serviceName];
    }
    return d->defaultConfig;
}

void DegradationPolicy::setDefaultPolicy(const DegradationPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("DegradationPolicy", "无效的默认降级策略配置");
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->defaultConfig = config;
    Logger::info("DegradationPolicy", "设置默认降级策略");
}

DegradationPolicyConfig DegradationPolicy::getDefaultPolicy() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->defaultConfig;
}

bool DegradationPolicy::shouldDegrade(const QString& serviceName,
                                      DegradationTrigger trigger,
                                      double errorRate) const
{
    if (!isEnabled()) {
        return false;
    }
    
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    DegradationPolicyConfig config = d->policies.value(serviceName, d->defaultConfig);
    
    if (!config.enabled) {
        return false;
    }
    
    return checkTriggerCondition(config, trigger, errorRate);
}

DegradationPolicyConfig DegradationPolicy::getDegradationStrategy(const QString& serviceName,
                                                                 DegradationTrigger trigger,
                                                                 double errorRate) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    DegradationPolicyConfig config = d->policies.value(serviceName, d->defaultConfig);
    
    if (!config.enabled || !checkTriggerCondition(config, trigger, errorRate)) {
        // 返回一个禁用策略
        DegradationPolicyConfig disabledConfig;
        disabledConfig.enabled = false;
        return disabledConfig;
    }
    
    return config;
}

void DegradationPolicy::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("DegradationPolicy", QString("降级策略%1").arg(enabled ? "启用" : "禁用"));
}

bool DegradationPolicy::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void DegradationPolicy::setServiceEnabled(const QString& serviceName, bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->policies.contains(serviceName)) {
        d->policies[serviceName].enabled = enabled;
        Logger::info("DegradationPolicy", QString("服务%1的降级策略%2")
            .arg(serviceName).arg(enabled ? "启用" : "禁用"));
    }
}

bool DegradationPolicy::isServiceEnabled(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->policies.contains(serviceName)) {
        return d->policies[serviceName].enabled;
    }
    return d->defaultConfig.enabled;
}

bool DegradationPolicy::checkTriggerCondition(const DegradationPolicyConfig& config,
                                              DegradationTrigger trigger,
                                              double errorRate) const
{
    if (config.trigger == DegradationTrigger::Always) {
        return true;
    }
    
    if (config.trigger == DegradationTrigger::Manual) {
        // 手动触发需要外部调用，这里不自动触发
        return false;
    }
    
    if (config.trigger == trigger) {
        // 触发条件匹配
        if (trigger == DegradationTrigger::ErrorRate) {
            // 检查错误率是否超过阈值
            return errorRate >= config.errorRateThreshold;
        }
        return true;
    }
    
    return false;
}

} // namespace Core
} // namespace Eagle
