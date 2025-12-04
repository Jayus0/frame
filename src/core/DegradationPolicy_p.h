#ifndef EAGLE_CORE_DEGRADATIONPOLICY_P_H
#define EAGLE_CORE_DEGRADATIONPOLICY_P_H

#include "eagle/core/DegradationPolicy.h"
#include <QtCore/QMap>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

class DegradationPolicyPrivate {
public:
    DegradationPolicyPrivate()
        : enabled(true)
    {
        // 设置默认策略
        defaultConfig.trigger = DegradationTrigger::CircuitBreakerOpen;
        defaultConfig.strategy = DegradationStrategy::DefaultValue;
        defaultConfig.enabled = true;
    }
    
    QMap<QString, DegradationPolicyConfig> policies;  // 服务名 -> 降级策略
    DegradationPolicyConfig defaultConfig;             // 默认降级策略
    bool enabled;                                      // 是否启用降级
    mutable QMutex mutex;                              // 线程安全
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_DEGRADATIONPOLICY_P_H
