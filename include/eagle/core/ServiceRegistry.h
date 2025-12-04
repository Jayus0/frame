#ifndef EAGLE_CORE_SERVICEREGISTRY_H
#define EAGLE_CORE_SERVICEREGISTRY_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include "ServiceDescriptor.h"
#include "CircuitBreaker.h"
#include "RetryPolicy.h"
#include "DegradationPolicy.h"

namespace Eagle {
namespace Core {

class ServiceRegistryPrivate;

/**
 * @brief 服务注册中心
 * 
 * 提供服务的注册、发现和调用功能
 */
class ServiceRegistry : public QObject {
    Q_OBJECT
    
public:
    explicit ServiceRegistry(QObject* parent = nullptr);
    ~ServiceRegistry();
    
    // 服务注册
    bool registerService(const ServiceDescriptor& descriptor, QObject* provider);
    bool unregisterService(const QString& serviceName, const QString& version = QString());
    
    // 服务发现
    QObject* findService(const QString& serviceName, const QString& version = QString()) const;
    QStringList availableServices() const;
    ServiceDescriptor getServiceDescriptor(const QString& serviceName) const;
    
    // 服务调用
    QVariant callService(const QString& serviceName, 
                        const QString& method,
                        const QVariantList& args = QVariantList(),
                        int timeout = 5000);
    
    // 带用户ID的服务调用（支持权限检查和限流）
    QVariant callService(const QString& userId, const QString& serviceName, 
                        const QString& method, const QVariantList& args = QVariantList());
    
    // 配置
    void setDefaultTimeout(int timeoutMs);
    void setCircuitBreakerEnabled(bool enabled);
    void setCircuitBreakerConfig(const QString& serviceName, const CircuitBreakerConfig& config);
    
    // 权限和限流配置
    void setPermissionCheckEnabled(bool enabled);
    bool isPermissionCheckEnabled() const;
    void setRateLimitEnabled(bool enabled);
    bool isRateLimitEnabled() const;
    void setServiceRateLimit(const QString& serviceName, int maxRequests, int windowMs);
    
    // 重试策略配置
    void setRetryEnabled(bool enabled);
    bool isRetryEnabled() const;
    void setRetryPolicy(const QString& serviceName, const RetryPolicyConfig& config);
    RetryPolicyConfig getRetryPolicy(const QString& serviceName) const;
    
    // 降级策略配置
    void setDegradationEnabled(bool enabled);
    bool isDegradationEnabled() const;
    void setDegradationPolicy(const QString& serviceName, const DegradationPolicyConfig& config);
    DegradationPolicyConfig getDegradationPolicy(const QString& serviceName) const;
    
    // 健康检查
    bool checkServiceHealth(const QString& serviceName) const;
    
private:
    // 重试辅助函数
    bool isRetryableError(const QString& serviceName, const QString& error, const RetryPolicyConfig& config) const;
    int calculateRetryDelay(const RetryPolicyConfig& config, int attemptCount) const;
    
    // 降级辅助函数（非const，因为需要调用非const的callService）
    QVariant tryDegrade(const QString& serviceName, const QString& method, 
                       const QVariantList& args, DegradationTrigger trigger);
    
public:
signals:
    void serviceRegistered(const QString& serviceName, const QString& version);
    void serviceUnregistered(const QString& serviceName, const QString& version);
    void serviceCallFailed(const QString& serviceName, const QString& error);
    
private:
    Q_DISABLE_COPY(ServiceRegistry)
    ServiceRegistryPrivate* d_ptr;
    
    inline ServiceRegistryPrivate* d_func() { return d_ptr; }
    inline const ServiceRegistryPrivate* d_func() const { return d_ptr; }
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_SERVICEREGISTRY_H
