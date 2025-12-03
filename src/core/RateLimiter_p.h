#ifndef RATELIMITER_P_H
#define RATELIMITER_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include "eagle/core/RateLimiter.h"

namespace Eagle {
namespace Core {

// 前向声明
struct RateLimitRule;
struct TokenBucket;
struct SlidingWindow;

class RateLimiter::Private {
public:
    QMap<QString, RateLimitRule> rules;
    QMap<QString, TokenBucket> tokenBuckets;
    QMap<QString, SlidingWindow> slidingWindows;
    QTimer* cleanupTimer;
    bool enabled = true;
    mutable QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // RATELIMITER_P_H
