#ifndef SERVICEREGISTRY_P_H
#define SERVICEREGISTRY_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include "eagle/core/ServiceDescriptor.h"
#include "eagle/core/RetryPolicy.h"
#include "eagle/core/DegradationPolicy.h"

namespace Eagle {
namespace Core {

class ServiceRegistryPrivate {
public:
    QMap<QString, QList<ServiceDescriptor>> services; // serviceName -> versions
    QMap<QString, QObject*> providers; // serviceName+version -> provider
    QMap<QString, CircuitBreaker*> circuitBreakers; // serviceName -> circuitBreaker
    QMap<QString, QPair<int, int>> serviceRateLimits; // serviceName -> (maxRequests, windowMs)
    QMap<QString, RetryPolicyConfig> retryPolicies;    // serviceName -> retryPolicy
    QMap<QString, DegradationPolicyConfig> degradationPolicies;  // serviceName -> degradationPolicy
    int defaultTimeoutMs = 5000;  // 默认超时时间
    bool enableCircuitBreaker = true;  // 是否启用熔断器
    bool enablePermissionCheck = false;  // 是否启用权限检查
    bool enableRateLimit = false;  // 是否启用限流
    bool enableRetry = true;  // 是否启用重试
    bool enableDegradation = true;  // 是否启用降级
    mutable QMutex mutex;  // mutable 允许在 const 函数中锁定
};

} // namespace Core
} // namespace Eagle

#endif // SERVICEREGISTRY_P_H
