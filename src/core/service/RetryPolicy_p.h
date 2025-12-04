#ifndef EAGLE_CORE_RETRYPOLICY_P_H
#define EAGLE_CORE_RETRYPOLICY_P_H

#include "eagle/core/RetryPolicy.h"
#include <QtCore/QMap>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

class RetryPolicyPrivate {
public:
    RetryPolicyPrivate()
        : enabled(true)
    {
        // 设置默认策略
        defaultConfig.maxRetries = 3;
        defaultConfig.initialDelayMs = 100;
        defaultConfig.maxDelayMs = 5000;
        defaultConfig.backoffMultiplier = 2.0;
        defaultConfig.strategy = RetryStrategy::Exponential;
    }
    
    QMap<QString, RetryPolicyConfig> policies;  // 服务名 -> 重试策略
    RetryPolicyConfig defaultConfig;             // 默认重试策略
    bool enabled;                                 // 是否启用重试
    mutable QMutex mutex;                         // 线程安全
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_RETRYPOLICY_P_H
