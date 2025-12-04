#ifndef EAGLE_CORE_LOADBALANCER_H
#define EAGLE_CORE_LOADBALANCER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include "ServiceDescriptor.h"

namespace Eagle {
namespace Core {

/**
 * @brief 负载均衡算法类型
 */
enum class LoadBalanceAlgorithm {
    RoundRobin,      // 轮询
    WeightedRoundRobin,  // 加权轮询
    LeastConnections,    // 最少连接数
    Random,          // 随机
    IPHash          // IP哈希（会话保持）
};

/**
 * @brief 服务实例信息
 */
struct ServiceInstance {
    ServiceDescriptor descriptor;  // 服务描述符
    int weight;                    // 权重（用于加权轮询）
    int activeConnections;         // 当前活跃连接数
    int totalRequests;             // 总请求数
    bool healthy;                  // 是否健康
    QObject* provider;             // 服务提供者对象
    
    ServiceInstance()
        : weight(1)
        , activeConnections(0)
        , totalRequests(0)
        , healthy(true)
        , provider(nullptr)
    {}
    
    bool isValid() const {
        return provider != nullptr && descriptor.isValid();
    }
};

/**
 * @brief 负载均衡器
 * 
 * 负责在多个服务实例之间分配请求
 */
class LoadBalancer : public QObject {
    Q_OBJECT
    
public:
    explicit LoadBalancer(QObject* parent = nullptr);
    ~LoadBalancer();
    
    /**
     * @brief 注册服务实例
     */
    bool registerInstance(const QString& serviceName, const ServiceDescriptor& descriptor, 
                        QObject* provider, int weight = 1);
    
    /**
     * @brief 注销服务实例
     */
    bool unregisterInstance(const QString& serviceName, const QString& instanceId);
    
    /**
     * @brief 选择服务实例
     */
    ServiceInstance* selectInstance(const QString& serviceName, const QString& clientId = QString());
    
    /**
     * @brief 记录服务调用开始
     */
    void onServiceCallStart(const QString& serviceName, const QString& instanceId);
    
    /**
     * @brief 记录服务调用结束
     */
    void onServiceCallEnd(const QString& serviceName, const QString& instanceId);
    
    /**
     * @brief 设置服务实例健康状态
     */
    void setInstanceHealth(const QString& serviceName, const QString& instanceId, bool healthy);
    
    /**
     * @brief 设置负载均衡算法
     */
    void setAlgorithm(const QString& serviceName, LoadBalanceAlgorithm algorithm);
    
    /**
     * @brief 获取负载均衡算法
     */
    LoadBalanceAlgorithm getAlgorithm(const QString& serviceName) const;
    
    /**
     * @brief 获取服务实例列表
     */
    QList<ServiceInstance> getInstances(const QString& serviceName) const;
    
    /**
     * @brief 获取服务实例统计信息
     */
    QMap<QString, QVariant> getInstanceStats(const QString& serviceName, const QString& instanceId) const;
    
    /**
     * @brief 设置实例权重
     */
    void setInstanceWeight(const QString& serviceName, const QString& instanceId, int weight);
    
    /**
     * @brief 生成实例ID
     */
    QString generateInstanceId(const ServiceDescriptor& descriptor) const;
    
    /**
     * @brief 根据provider获取实例ID
     */
    QString getInstanceIdByProvider(const QString& serviceName, QObject* provider) const;
    
    /**
     * @brief 启用/禁用负载均衡
     */
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
signals:
    void instanceRegistered(const QString& serviceName, const QString& instanceId);
    void instanceUnregistered(const QString& serviceName, const QString& instanceId);
    void instanceHealthChanged(const QString& serviceName, const QString& instanceId, bool healthy);
    
private:
    Q_DISABLE_COPY(LoadBalancer)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    // 负载均衡算法实现
    ServiceInstance* selectRoundRobin(const QString& serviceName);
    ServiceInstance* selectWeightedRoundRobin(const QString& serviceName);
    ServiceInstance* selectLeastConnections(const QString& serviceName);
    ServiceInstance* selectRandom(const QString& serviceName);
    ServiceInstance* selectIPHash(const QString& serviceName, const QString& clientId);
    
    // 辅助方法
    QString generateInstanceId(const ServiceDescriptor& descriptor) const;
    QList<ServiceInstance*> getHealthyInstances(const QString& serviceName) const;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_LOADBALANCER_H
