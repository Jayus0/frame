#include "eagle/core/RetryPolicy.h"
#include "RetryPolicy_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtCore/QElapsedTimer>
#include <cmath>

namespace Eagle {
namespace Core {

RetryPolicy::RetryPolicy(QObject* parent)
    : QObject(parent)
    , d(new RetryPolicyPrivate)
{
    Logger::info("RetryPolicy", "重试策略管理器初始化完成");
}

RetryPolicy::~RetryPolicy()
{
    delete d;
}

void RetryPolicy::setPolicy(const QString& key, const RetryPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("RetryPolicy", QString("无效的重试策略配置: %1").arg(key));
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->policies[key] = config;
    
    Logger::info("RetryPolicy", QString("设置重试策略: %1 - 最大重试次数: %2, 策略: %3")
        .arg(key)
        .arg(config.maxRetries)
        .arg(static_cast<int>(config.strategy)));
}

void RetryPolicy::removePolicy(const QString& key)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->policies.remove(key);
    Logger::info("RetryPolicy", QString("移除重试策略: %1").arg(key));
}

RetryPolicyConfig RetryPolicy::getPolicy(const QString& key) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->policies.contains(key)) {
        return d->policies[key];
    }
    return d->defaultConfig;
}

void RetryPolicy::setDefaultPolicy(const RetryPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("RetryPolicy", "无效的默认重试策略配置");
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->defaultConfig = config;
    Logger::info("RetryPolicy", "设置默认重试策略");
}

RetryPolicyConfig RetryPolicy::getDefaultPolicy() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->defaultConfig;
}

bool RetryPolicy::shouldRetry(const QString& key, int attemptCount, const QString& error) const
{
    if (!isEnabled()) {
        return false;
    }
    
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    RetryPolicyConfig config = d->policies.value(key, d->defaultConfig);
    
    // 检查是否超过最大重试次数
    if (attemptCount >= config.maxRetries) {
        emit const_cast<RetryPolicy*>(this)->retryExhausted(key, config.maxRetries, error);
        return false;
    }
    
    // 检查错误是否可重试
    if (!isRetryableError(key, error)) {
        return false;
    }
    
    emit const_cast<RetryPolicy*>(this)->retryTriggered(key, attemptCount, error);
    return true;
}

int RetryPolicy::getRetryDelay(const QString& key, int attemptCount) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    RetryPolicyConfig config = d->policies.value(key, d->defaultConfig);
    return calculateDelay(config, attemptCount);
}

bool RetryPolicy::isRetryableError(const QString& key, const QString& error) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    RetryPolicyConfig config = d->policies.value(key, d->defaultConfig);
    
    // 如果指定了不可重试的错误列表，检查是否在列表中
    if (!config.nonRetryableErrors.isEmpty()) {
        for (const QString& nonRetryable : config.nonRetryableErrors) {
            if (error.contains(nonRetryable, Qt::CaseInsensitive)) {
                return false;
            }
        }
    }
    
    // 如果指定了可重试的错误列表，检查是否在列表中
    if (!config.retryableErrors.isEmpty()) {
        for (const QString& retryable : config.retryableErrors) {
            if (error.contains(retryable, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;  // 不在可重试列表中，不可重试
    }
    
    // 默认：所有错误都可重试（除非在不可重试列表中）
    return true;
}

void RetryPolicy::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("RetryPolicy", QString("重试策略%1").arg(enabled ? "启用" : "禁用"));
}

bool RetryPolicy::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

int RetryPolicy::calculateDelay(const RetryPolicyConfig& config, int attemptCount) const
{
    int delay = 0;
    
    switch (config.strategy) {
        case RetryStrategy::Fixed:
            // 固定延迟
            delay = config.initialDelayMs;
            break;
            
        case RetryStrategy::Exponential:
            // 指数退避：delay = initialDelay * (multiplier ^ attemptCount)
            delay = static_cast<int>(config.initialDelayMs * std::pow(config.backoffMultiplier, attemptCount));
            break;
            
        case RetryStrategy::Linear:
            // 线性增长：delay = initialDelay * (1 + attemptCount)
            delay = config.initialDelayMs * (1 + attemptCount);
            break;
    }
    
    // 限制在最大延迟范围内
    return qMin(delay, config.maxDelayMs);
}

} // namespace Core
} // namespace Eagle
