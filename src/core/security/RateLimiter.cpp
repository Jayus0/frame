#include "eagle/core/RateLimiter.h"
#include "RateLimiter_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QList>
#include <algorithm>

namespace Eagle {
namespace Core {

struct RateLimitRule {
    int maxRequests;
    int windowMs;
    RateLimitAlgorithm algorithm;
    QDateTime lastUpdate;
    
    RateLimitRule() 
        : maxRequests(0)
        , windowMs(0)
        , algorithm(RateLimitAlgorithm::TokenBucket)
    {
        lastUpdate = QDateTime::currentDateTime();
    }
};

struct TokenBucket {
    int tokens;           // 当前令牌数
    int capacity;          // 桶容量
    QDateTime lastRefill;  // 上次补充时间
    int refillRate;        // 补充速率（令牌/秒）
    
    // 默认构造函数（QMap需要）
    TokenBucket()
        : tokens(0)
        , capacity(0)
        , refillRate(0)
    {
        lastRefill = QDateTime::currentDateTime();
    }
    
    TokenBucket(int maxRequests, int windowMs) 
        : tokens(maxRequests)
        , capacity(maxRequests)
        , refillRate(maxRequests * 1000 / windowMs)  // 转换为令牌/秒
    {
        lastRefill = QDateTime::currentDateTime();
    }
    
    bool consume() {
        QDateTime now = QDateTime::currentDateTime();
        qint64 elapsed = lastRefill.msecsTo(now);
        
        // 补充令牌
        int tokensToAdd = (elapsed * refillRate) / 1000;
        if (tokensToAdd > 0) {
            tokens = qMin(capacity, tokens + tokensToAdd);
            lastRefill = now;
        }
        
        // 消费令牌
        if (tokens > 0) {
            tokens--;
            return true;
        }
        return false;
    }
};

struct SlidingWindow {
    QList<QDateTime> requests;  // 请求时间列表
    int maxRequests;
    int windowMs;
    
    // 默认构造函数（QMap需要）
    SlidingWindow()
        : maxRequests(0)
        , windowMs(0)
    {}
    
    SlidingWindow(int maxRequests, int windowMs)
        : maxRequests(maxRequests)
        , windowMs(windowMs)
    {}
    
    bool addRequest() {
        QDateTime now = QDateTime::currentDateTime();
        
        // 移除窗口外的请求
        while (!requests.isEmpty()) {
            qint64 elapsed = requests.first().msecsTo(now);
            if (elapsed < windowMs) {
                break;
            }
            requests.removeFirst();
        }
        
        // 检查是否超过限制
        if (requests.size() >= maxRequests) {
            return false;
        }
        
        requests.append(now);
        return true;
    }
};

// Private类定义在RateLimiter_p.h中

RateLimiter::RateLimiter(QObject* parent)
    : QObject(parent)
    , d(new RateLimiter::Private)
{
    d->cleanupTimer = new QTimer(this);
    d->cleanupTimer->setInterval(60000);  // 每分钟清理一次
    connect(d->cleanupTimer, &QTimer::timeout, this, &RateLimiter::onCleanupTimer);
    d->cleanupTimer->start();
    
    Logger::info("RateLimiter", "限流器初始化完成");
}

RateLimiter::~RateLimiter()
{
    delete d;
}

void RateLimiter::setLimit(const QString& key, int maxRequests, int windowMs, RateLimitAlgorithm algorithm)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    RateLimitRule rule;
    rule.maxRequests = maxRequests;
    rule.windowMs = windowMs;
    rule.algorithm = algorithm;
    rule.lastUpdate = QDateTime::currentDateTime();
    
    d->rules[key] = rule;
    
    // 初始化对应的数据结构
    if (algorithm == RateLimitAlgorithm::TokenBucket) {
        d->tokenBuckets[key] = TokenBucket(maxRequests, windowMs);
    } else {
        d->slidingWindows[key] = SlidingWindow(maxRequests, windowMs);
    }
    
    Logger::info("RateLimiter", QString("设置限流规则: %1 - %2次/%3ms").arg(key).arg(maxRequests).arg(windowMs));
}

void RateLimiter::removeLimit(const QString& key)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->rules.remove(key);
    d->tokenBuckets.remove(key);
    d->slidingWindows.remove(key);
    
    Logger::info("RateLimiter", QString("移除限流规则: %1").arg(key));
}

void RateLimiter::clearLimits()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->rules.clear();
    d->tokenBuckets.clear();
    d->slidingWindows.clear();
    
    Logger::info("RateLimiter", "清空所有限流规则");
}

bool RateLimiter::allowRequest(const QString& key)
{
    if (!isEnabled()) {
        return true;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->rules.contains(key)) {
        return true;  // 没有限流规则，允许通过
    }
    
    const RateLimitRule& rule = d->rules[key];
    
    bool allowed = false;
    if (rule.algorithm == RateLimitAlgorithm::TokenBucket) {
        allowed = checkTokenBucket(key, rule.maxRequests, rule.windowMs);
    } else {
        allowed = checkSlidingWindow(key, rule.maxRequests, rule.windowMs);
    }
    
    if (!allowed) {
        emit rateLimitExceeded(key, rule.maxRequests, rule.windowMs);
        Logger::warning("RateLimiter", QString("限流触发: %1 - %2次/%3ms").arg(key).arg(rule.maxRequests).arg(rule.windowMs));
    }
    
    return allowed;
}

bool RateLimiter::allowRequest(const QString& key, int maxRequests, int windowMs)
{
    if (!isEnabled()) {
        return true;
    }
    
    // 临时限流检查（使用滑动窗口算法）
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->slidingWindows.contains(key)) {
        d->slidingWindows[key] = SlidingWindow(maxRequests, windowMs);
    }
    
    return d->slidingWindows[key].addRequest();
}

int RateLimiter::getRemainingRequests(const QString& key) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->rules.contains(key)) {
        return -1;  // 无限制
    }
    
    const RateLimitRule& rule = d->rules[key];
    
    if (rule.algorithm == RateLimitAlgorithm::TokenBucket) {
        if (d->tokenBuckets.contains(key)) {
            return d->tokenBuckets[key].tokens;
        }
    } else {
        if (d->slidingWindows.contains(key)) {
            const SlidingWindow& window = d->slidingWindows[key];
            return qMax(0, rule.maxRequests - window.requests.size());
        }
    }
    
    return 0;
}

QDateTime RateLimiter::getResetTime(const QString& key) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->rules.contains(key)) {
        return QDateTime();
    }
    
    const RateLimitRule& rule = d->rules[key];
    QDateTime resetTime = rule.lastUpdate.addMSecs(rule.windowMs);
    
    if (rule.algorithm == RateLimitAlgorithm::SlidingWindow) {
        if (d->slidingWindows.contains(key)) {
            const SlidingWindow& window = d->slidingWindows[key];
            if (!window.requests.isEmpty()) {
                resetTime = window.requests.first().addMSecs(rule.windowMs);
            }
        }
    }
    
    return resetTime;
}

void RateLimiter::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("RateLimiter", QString("限流器%1").arg(enabled ? "启用" : "禁用"));
}

bool RateLimiter::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void RateLimiter::onCleanupTimer()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QDateTime now = QDateTime::currentDateTime();
    QStringList keysToRemove;
    
    // 清理长时间未使用的限流规则（超过1小时）
    for (auto it = d->rules.begin(); it != d->rules.end(); ++it) {
        qint64 elapsed = it.value().lastUpdate.msecsTo(now);
        if (elapsed > 3600000) {  // 1小时
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString& key : keysToRemove) {
        d->rules.remove(key);
        d->tokenBuckets.remove(key);
        d->slidingWindows.remove(key);
    }
    
    if (!keysToRemove.isEmpty()) {
        Logger::debug("RateLimiter", QString("清理了 %1 个过期限流规则").arg(keysToRemove.size()));
    }
}

bool RateLimiter::checkTokenBucket(const QString& key, int maxRequests, int windowMs)
{
    auto* d = d_func();
    
    if (!d->tokenBuckets.contains(key)) {
        d->tokenBuckets[key] = TokenBucket(maxRequests, windowMs);
    }
    
    return d->tokenBuckets[key].consume();
}

bool RateLimiter::checkSlidingWindow(const QString& key, int maxRequests, int windowMs)
{
    auto* d = d_func();
    
    if (!d->slidingWindows.contains(key)) {
        d->slidingWindows[key] = SlidingWindow(maxRequests, windowMs);
    }
    
    return d->slidingWindows[key].addRequest();
}

} // namespace Core
} // namespace Eagle
