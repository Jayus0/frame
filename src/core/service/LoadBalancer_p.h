#ifndef LOADBALANCER_P_H
#define LOADBALANCER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QVariant>
#include "eagle/core/LoadBalancer.h"

namespace Eagle {
namespace Core {

class LoadBalancer::Private {
public:
    // 服务实例管理：serviceName -> instanceId -> ServiceInstance
    QMap<QString, QMap<QString, ServiceInstance>> instances;
    
    // 负载均衡算法：serviceName -> algorithm
    QMap<QString, LoadBalanceAlgorithm> algorithms;
    
    // 轮询索引：serviceName -> currentIndex
    QMap<QString, int> roundRobinIndices;
    
    // 加权轮询当前权重：serviceName -> currentWeight
    QMap<QString, int> weightedRoundRobinWeights;
    
    // IP哈希映射：serviceName -> clientId -> instanceId
    QMap<QString, QMap<QString, QString>> ipHashMappings;
    
    bool enabled;
    mutable QMutex mutex;
    
    Private()
        : enabled(true)
    {}
};

} // namespace Core
} // namespace Eagle

#endif // LOADBALANCER_P_H
