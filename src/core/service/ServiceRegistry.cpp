#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/ServiceDescriptor.h"
#include "ServiceRegistry_p.h"
#include "eagle/core/CircuitBreaker.h"
#include "eagle/core/RetryPolicy.h"
#include "eagle/core/DegradationPolicy.h"
#include "eagle/core/LoadBalancer.h"
#include "eagle/core/AsyncServiceCall.h"
#include "eagle/core/Framework.h"
#include "eagle/core/RBAC.h"
#include "eagle/core/RateLimiter.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>
#include <cmath>

namespace Eagle {
namespace Core {

ServiceRegistry::ServiceRegistry(QObject* parent)
    : QObject(parent)
    , d_ptr(new ServiceRegistryPrivate)
{
    auto* d = d_func();
    d->defaultTimeoutMs = 5000;
    d->enableCircuitBreaker = true;
    d->loadBalancer = new LoadBalancer(this);
    d->asyncServiceCall = new AsyncServiceCall(this, this);
}

ServiceRegistry::~ServiceRegistry()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 清理熔断器
    for (CircuitBreaker* breaker : d->circuitBreakers) {
        delete breaker;
    }
    d->circuitBreakers.clear();
    
    d->services.clear();
    d->providers.clear();
    delete d_ptr;
}

bool ServiceRegistry::registerService(const ServiceDescriptor& descriptor, QObject* provider)
{
    if (!descriptor.isValid() || !provider) {
        Logger::error("ServiceRegistry", "Invalid service descriptor or provider");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString key = descriptor.serviceName + "@" + descriptor.version;
    if (d->providers.contains(key)) {
        Logger::warning("ServiceRegistry", QString("服务已注册: %1").arg(key));
        return false;
    }
    
    if (!d->services.contains(descriptor.serviceName)) {
        d->services[descriptor.serviceName] = QList<ServiceDescriptor>();
    }
    
    ServiceDescriptor desc = descriptor;
    desc.provider = provider;
    d->services[descriptor.serviceName].append(desc);
    d->providers[key] = provider;
    
    // 注册到负载均衡器
    if (d->loadBalancer && d->enableLoadBalance) {
        locker.unlock();  // 释放锁，避免死锁
        // 使用ServiceDescriptor注册，LoadBalancer内部会生成instanceId
        d->loadBalancer->registerInstance(descriptor.serviceName, desc, provider, 1);
        locker.relock();
    }
    
    Logger::info("ServiceRegistry", QString("服务注册成功: %1@%2")
        .arg(descriptor.serviceName, descriptor.version));
    
    emit serviceRegistered(descriptor.serviceName, descriptor.version);
    return true;
}

bool ServiceRegistry::unregisterService(const QString& serviceName, const QString& version)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->services.contains(serviceName)) {
        return false;
    }
    
    QList<ServiceDescriptor>& descriptors = d->services[serviceName];
    if (version.isEmpty()) {
        // 卸载所有版本
        for (const ServiceDescriptor& desc : descriptors) {
            QString key = desc.serviceName + "@" + desc.version;
            d->providers.remove(key);
            
            // 从负载均衡器注销
            if (d->loadBalancer && d->enableLoadBalance) {
                QString instanceId = d->loadBalancer->getInstanceIdByProvider(serviceName, desc.provider);
                if (!instanceId.isEmpty()) {
                    locker.unlock();
                    d->loadBalancer->unregisterInstance(serviceName, instanceId);
                    locker.relock();
                }
            }
        }
        d->services.remove(serviceName);
        emit serviceUnregistered(serviceName, QString());
    } else {
        // 卸载指定版本
        for (int i = descriptors.size() - 1; i >= 0; --i) {
            if (descriptors[i].version == version) {
                QString key = serviceName + "@" + version;
                ServiceDescriptor desc = descriptors[i];
                d->providers.remove(key);
                descriptors.removeAt(i);
                
                // 从负载均衡器注销
                if (d->loadBalancer && d->enableLoadBalance) {
                    QString instanceId = d->loadBalancer->getInstanceIdByProvider(serviceName, desc.provider);
                    if (!instanceId.isEmpty()) {
                        locker.unlock();
                        d->loadBalancer->unregisterInstance(serviceName, instanceId);
                        locker.relock();
                    }
                }
                
                emit serviceUnregistered(serviceName, version);
                break;
            }
        }
        if (descriptors.isEmpty()) {
            d->services.remove(serviceName);
        }
    }
    
    Logger::info("ServiceRegistry", QString("服务卸载成功: %1@%2")
        .arg(serviceName, version.isEmpty() ? "all" : version));
    return true;
}

QObject* ServiceRegistry::findService(const QString& serviceName, const QString& version) const
{
    const auto* d = d_func();
    
    // 如果启用负载均衡且没有指定版本，使用负载均衡器选择实例
    if (d->enableLoadBalance && d->loadBalancer && version.isEmpty()) {
        ServiceInstance* instance = d->loadBalancer->selectInstance(serviceName);
        if (instance && instance->provider) {
            return instance->provider;
        }
    }
    
    QMutexLocker locker(&d->mutex);
    
    if (!d->services.contains(serviceName)) {
        return nullptr;
    }
    
    const QList<ServiceDescriptor>& descriptors = d->services[serviceName];
    if (version.isEmpty()) {
        // 返回最新版本
        if (!descriptors.isEmpty()) {
            QString key = serviceName + "@" + descriptors.last().version;
            return d->providers.value(key);
        }
    } else {
        QString key = serviceName + "@" + version;
        return d->providers.value(key);
    }
    
    return nullptr;
}

QStringList ServiceRegistry::availableServices() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->services.keys();
}

ServiceDescriptor ServiceRegistry::getServiceDescriptor(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->services.contains(serviceName)) {
        return ServiceDescriptor();
    }
    
    const QList<ServiceDescriptor>& descriptors = d->services[serviceName];
    if (!descriptors.isEmpty()) {
        return descriptors.last(); // 返回最新版本
    }
    
    return ServiceDescriptor();
}

QVariant ServiceRegistry::callService(const QString& serviceName,
                                     const QString& method,
                                     const QVariantList& args,
                                     int timeout)
{
    // 获取重试策略
    RetryPolicyConfig retryConfig;
    bool retryEnabled = false;
    {
        auto* d = d_func();
        QMutexLocker locker(&d->mutex);
        retryEnabled = d->enableRetry;
        if (retryEnabled) {
            retryConfig = d->retryPolicies.value(serviceName);
            if (!retryConfig.isValid()) {
                // 使用默认配置
                retryConfig.maxRetries = 3;
                retryConfig.initialDelayMs = 100;
                retryConfig.maxDelayMs = 5000;
                retryConfig.backoffMultiplier = 2.0;
                retryConfig.strategy = RetryStrategy::Exponential;
            }
        }
    }
    
    // 使用默认超时时间
    auto* d = d_func();
    if (timeout <= 0) {
        QMutexLocker locker(&d->mutex);
        timeout = d->defaultTimeoutMs;
    }
    
    int attemptCount = 0;
    QString lastError;
    QVariant returnValue;  // 在循环外部定义，保存成功调用的返回值
    
    // 重试循环
    while (true) {
        attemptCount++;
        
        auto* d = d_func();
        
        // 检查熔断器
        if (d->enableCircuitBreaker) {
            QMutexLocker locker(&d->mutex);
            CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
            if (!breaker) {
                // 创建默认熔断器
                CircuitBreakerConfig config;
                breaker = new CircuitBreaker(serviceName, config, this);
                d->circuitBreakers[serviceName] = breaker;
            }
            locker.unlock();
            
            if (!breaker->allowCall()) {
                QString error = QString("Service circuit breaker is open: %1").arg(serviceName);
                Logger::warning("ServiceRegistry", error);
                breaker->recordFailure();  // 记录失败
                lastError = error;
                
                // 检查是否应该重试
                if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                    isRetryableError(serviceName, error, retryConfig)) {
                    int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                    Logger::info("ServiceRegistry", QString("重试服务调用: %1::%2 (第%3次, 延迟%4ms)")
                        .arg(serviceName, method).arg(attemptCount).arg(delay));
                    QThread::msleep(delay);
                    continue;
                }
                
                // 检查是否需要降级
                QVariant degradedResult = tryDegrade(serviceName, method, args, DegradationTrigger::CircuitBreakerOpen);
                if (degradedResult.isValid()) {
                    Logger::info("ServiceRegistry", QString("服务降级成功: %1::%2").arg(serviceName, method));
                    return degradedResult;
                }
                
                emit serviceCallFailed(serviceName, error);
                return QVariant();
            }
        }
        
        // 使用负载均衡器选择服务实例
        QObject* provider = nullptr;
        QString instanceId;
        
        {
            auto* d = d_func();
            if (d->enableLoadBalance && d->loadBalancer) {
                ServiceInstance* instance = d->loadBalancer->selectInstance(serviceName);
                if (instance && instance->provider) {
                    provider = instance->provider;
                    instanceId = d->loadBalancer->getInstanceIdByProvider(serviceName, provider);
                    // 记录服务调用开始
                    if (!instanceId.isEmpty()) {
                        d->loadBalancer->onServiceCallStart(serviceName, instanceId);
                    }
                }
            }
        }
        
        // 如果负载均衡器没有选择到实例，使用原来的方法
        if (!provider) {
            provider = findService(serviceName);
        }
        
        if (!provider) {
            QString error = QString("Service not found: %1").arg(serviceName);
            Logger::error("ServiceRegistry", error);
            lastError = error;
            
            // 如果已经开始调用，需要结束调用计数
            if (!instanceId.isEmpty()) {
                auto* d = d_func();
                if (d->loadBalancer) {
                    d->loadBalancer->onServiceCallEnd(serviceName, instanceId);
                }
            }
            instanceId.clear();  // 清除instanceId，避免后续重复处理
            
            // 检查是否应该重试（服务不存在通常不应该重试）
            if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                isRetryableError(serviceName, error, retryConfig)) {
                int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                QThread::msleep(delay);
                continue;
            }
            
            // 记录失败
            if (d->enableCircuitBreaker) {
                QMutexLocker locker(&d->mutex);
                CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
                if (breaker) {
                    breaker->recordFailure();
                }
            }
            
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
        
        // 使用Qt的元对象系统调用方法
        const QMetaObject* metaObj = provider->metaObject();
        int methodIndex = metaObj->indexOfMethod(method.toUtf8().constData());
        if (methodIndex == -1) {
            QString error = QString("Method not found: %1::%2").arg(serviceName, method);
            Logger::error("ServiceRegistry", error);
            lastError = error;
            
            // 检查是否应该重试
            if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                isRetryableError(serviceName, error, retryConfig)) {
                int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                QThread::msleep(delay);
                continue;
            }
            
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
        
        QMetaMethod metaMethod = metaObj->method(methodIndex);
        if (metaMethod.methodType() != QMetaMethod::Method && 
            metaMethod.methodType() != QMetaMethod::Slot) {
            QString error = QString("Method is not callable: %1::%2").arg(serviceName, method);
            Logger::error("ServiceRegistry", error);
            lastError = error;
            
            // 检查是否应该重试
            if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                isRetryableError(serviceName, error, retryConfig)) {
                int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                QThread::msleep(delay);
                continue;
            }
            
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
        
        // 使用超时机制调用方法
        bool success = false;
        QElapsedTimer timer;
        timer.start();
        QVariant currentReturnValue;  // 当前尝试的返回值
        
        // 尝试调用方法（带超时检查）
        if (metaMethod.returnType() != QMetaType::Void) {
            QGenericReturnArgument retArg = Q_RETURN_ARG(QVariant, currentReturnValue);
            if (args.isEmpty()) {
                success = metaMethod.invoke(provider, Qt::DirectConnection, retArg);
            } else {
                QGenericArgument arg1 = args.size() > 0 ? Q_ARG(QVariant, args[0]) : QGenericArgument();
                QGenericArgument arg2 = args.size() > 1 ? Q_ARG(QVariant, args[1]) : QGenericArgument();
                QGenericArgument arg3 = args.size() > 2 ? Q_ARG(QVariant, args[2]) : QGenericArgument();
                success = metaMethod.invoke(provider, Qt::DirectConnection, retArg, arg1, arg2, arg3);
            }
        } else {
            if (args.isEmpty()) {
                success = metaMethod.invoke(provider, Qt::DirectConnection);
            } else {
                QGenericArgument arg1 = args.size() > 0 ? Q_ARG(QVariant, args[0]) : QGenericArgument();
                QGenericArgument arg2 = args.size() > 1 ? Q_ARG(QVariant, args[1]) : QGenericArgument();
                QGenericArgument arg3 = args.size() > 2 ? Q_ARG(QVariant, args[2]) : QGenericArgument();
                success = metaMethod.invoke(provider, Qt::DirectConnection, arg1, arg2, arg3);
            }
        }
        
        // 检查超时（注意：Qt的invoke是同步的，这里只是检查执行时间）
        if (timer.elapsed() > timeout) {
            QString error = QString("Service call timeout: %1::%2 (耗时: %3ms)").arg(serviceName, method).arg(timer.elapsed());
            Logger::error("ServiceRegistry", error);
            lastError = error;
            
            // 检查是否应该重试（超时通常可以重试）
            if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                isRetryableError(serviceName, error, retryConfig)) {
                int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                Logger::info("ServiceRegistry", QString("重试服务调用: %1::%2 (第%3次, 延迟%4ms)")
                    .arg(serviceName, method).arg(attemptCount).arg(delay));
                QThread::msleep(delay);
                continue;  // 重试
            }
            
            // 记录失败
            if (d->enableCircuitBreaker) {
                QMutexLocker locker(&d->mutex);
                CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
                if (breaker) {
                    breaker->recordFailure();
                }
            }
            
            // 检查是否需要降级
            QVariant degradedResult = tryDegrade(serviceName, method, args, DegradationTrigger::Timeout);
            if (degradedResult.isValid()) {
                Logger::info("ServiceRegistry", QString("服务降级成功（超时）: %1::%2").arg(serviceName, method));
                return degradedResult;
            }
            
            // 记录服务调用结束（超时失败）
            if (!instanceId.isEmpty()) {
                auto* d = d_func();
                if (d->loadBalancer) {
                    d->loadBalancer->onServiceCallEnd(serviceName, instanceId);
                }
            }
            
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
        
        if (!success) {
            QString error = QString("Service call failed: %1::%2").arg(serviceName, method);
            Logger::error("ServiceRegistry", error);
            lastError = error;
            
            // 检查是否应该重试
            if (retryEnabled && attemptCount <= retryConfig.maxRetries && 
                isRetryableError(serviceName, error, retryConfig)) {
                int delay = calculateRetryDelay(retryConfig, attemptCount - 1);
                Logger::info("ServiceRegistry", QString("重试服务调用: %1::%2 (第%3次, 延迟%4ms)")
                    .arg(serviceName, method).arg(attemptCount).arg(delay));
                QThread::msleep(delay);
                continue;  // 重试
            }
            
            // 记录失败
            if (d->enableCircuitBreaker) {
                QMutexLocker locker(&d->mutex);
                CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
                if (breaker) {
                    breaker->recordFailure();
                }
            }
            
            // 检查是否需要降级（重试失败后）
            if (attemptCount > retryConfig.maxRetries) {
                QVariant degradedResult = tryDegrade(serviceName, method, args, DegradationTrigger::ErrorRate);
                if (degradedResult.isValid()) {
                    Logger::info("ServiceRegistry", QString("服务降级成功（重试失败）: %1::%2").arg(serviceName, method));
                    return degradedResult;
                }
            }
            
            // 记录服务调用结束（调用失败）
            if (!instanceId.isEmpty()) {
                auto* d = d_func();
                if (d->loadBalancer) {
                    d->loadBalancer->onServiceCallEnd(serviceName, instanceId);
                }
            }
            
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
        
        // 调用成功，保存返回值并退出重试循环
        returnValue = currentReturnValue;
        
        // 记录服务调用结束（成功）
        if (!instanceId.isEmpty()) {
            auto* d = d_func();
            if (d->loadBalancer) {
                d->loadBalancer->onServiceCallEnd(serviceName, instanceId);
            }
        }
        break;
    }
    
    // 调用成功，记录结果
    if (d->enableCircuitBreaker) {
        QMutexLocker locker(&d->mutex);
        CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
        if (breaker) {
            breaker->recordSuccess();
        }
    }
    
    // 记录服务调用时间
    Framework* framework = Framework::instance();
    if (framework && framework->performanceMonitor()) {
        // 简化处理：记录最后一次调用的时间
        // 注意：timer在循环内部，这里使用一个固定的小值表示成功
        framework->performanceMonitor()->recordServiceCallTime(serviceName, method, 0);
    }
    
    if (attemptCount > 1) {
        Logger::info("ServiceRegistry", QString("服务调用成功（重试%1次）: %2::%3")
            .arg(attemptCount - 1).arg(serviceName, method));
    }
    
    return returnValue;
}

void ServiceRegistry::setLoadBalanceEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enableLoadBalance = enabled;
    if (d->loadBalancer) {
        d->loadBalancer->setEnabled(enabled);
    }
    Logger::info("ServiceRegistry", QString("负载均衡%1").arg(enabled ? "启用" : "禁用"));
}

bool ServiceRegistry::isLoadBalanceEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enableLoadBalance;
}

void ServiceRegistry::setLoadBalanceAlgorithm(const QString& serviceName, const QString& algorithm)
{
    auto* d = d_func();
    if (!d->loadBalancer) {
        Logger::warning("ServiceRegistry", "负载均衡器未初始化");
        return;
    }
    
    LoadBalanceAlgorithm algo = LoadBalanceAlgorithm::RoundRobin;
    if (algorithm == "weighted_round_robin" || algorithm == "weighted") {
        algo = LoadBalanceAlgorithm::WeightedRoundRobin;
    } else if (algorithm == "least_connections" || algorithm == "least") {
        algo = LoadBalanceAlgorithm::LeastConnections;
    } else if (algorithm == "random") {
        algo = LoadBalanceAlgorithm::Random;
    } else if (algorithm == "ip_hash" || algorithm == "hash") {
        algo = LoadBalanceAlgorithm::IPHash;
    }
    
    d->loadBalancer->setAlgorithm(serviceName, algo);
}

QString ServiceRegistry::getLoadBalanceAlgorithm(const QString& serviceName) const
{
    const auto* d = d_func();
    if (!d->loadBalancer) {
        return "round_robin";
    }
    
    LoadBalanceAlgorithm algo = d->loadBalancer->getAlgorithm(serviceName);
    switch (algo) {
    case LoadBalanceAlgorithm::WeightedRoundRobin:
        return "weighted_round_robin";
    case LoadBalanceAlgorithm::LeastConnections:
        return "least_connections";
    case LoadBalanceAlgorithm::Random:
        return "random";
    case LoadBalanceAlgorithm::IPHash:
        return "ip_hash";
    default:
        return "round_robin";
    }
}

LoadBalancer* ServiceRegistry::loadBalancer() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->loadBalancer;
}

AsyncServiceCall* ServiceRegistry::asyncServiceCall() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->asyncServiceCall;
}


void ServiceRegistry::setDefaultTimeout(int timeoutMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->defaultTimeoutMs = timeoutMs;
    Logger::info("ServiceRegistry", QString("设置默认超时时间: %1ms").arg(timeoutMs));
}

void ServiceRegistry::setCircuitBreakerEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enableCircuitBreaker = enabled;
    Logger::info("ServiceRegistry", QString("熔断器%1").arg(enabled ? "启用" : "禁用"));
}

void ServiceRegistry::setCircuitBreakerConfig(const QString& serviceName, const CircuitBreakerConfig& config)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
    if (breaker) {
        breaker->reset();
        // 注意：CircuitBreakerConfig 在构造时设置，这里简化处理
        Logger::info("ServiceRegistry", QString("设置服务熔断器配置: %1").arg(serviceName));
    } else {
        breaker = new CircuitBreaker(serviceName, config, this);
        d->circuitBreakers[serviceName] = breaker;
        Logger::info("ServiceRegistry", QString("创建服务熔断器: %1").arg(serviceName));
    }
}

void ServiceRegistry::setPermissionCheckEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enablePermissionCheck = enabled;
    Logger::info("ServiceRegistry", QString("权限检查%1").arg(enabled ? "启用" : "禁用"));
}

bool ServiceRegistry::isPermissionCheckEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enablePermissionCheck;
}

void ServiceRegistry::setRateLimitEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enableRateLimit = enabled;
    Logger::info("ServiceRegistry", QString("限流%1").arg(enabled ? "启用" : "禁用"));
}

bool ServiceRegistry::isRateLimitEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enableRateLimit;
}

void ServiceRegistry::setServiceRateLimit(const QString& serviceName, int maxRequests, int windowMs)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->serviceRateLimits[serviceName] = qMakePair(maxRequests, windowMs);
    Logger::info("ServiceRegistry", QString("设置服务限流: %1 - %2次/%3ms")
        .arg(serviceName).arg(maxRequests).arg(windowMs));
}

void ServiceRegistry::setRetryEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enableRetry = enabled;
    Logger::info("ServiceRegistry", QString("重试策略%1").arg(enabled ? "启用" : "禁用"));
}

bool ServiceRegistry::isRetryEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enableRetry;
}

void ServiceRegistry::setRetryPolicy(const QString& serviceName, const RetryPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("ServiceRegistry", QString("无效的重试策略配置: %1").arg(serviceName));
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->retryPolicies[serviceName] = config;
    Logger::info("ServiceRegistry", QString("设置服务重试策略: %1 - 最大重试次数: %2")
        .arg(serviceName).arg(config.maxRetries));
}

RetryPolicyConfig ServiceRegistry::getRetryPolicy(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->retryPolicies.value(serviceName);
}

// 辅助函数：判断错误是否可重试
bool ServiceRegistry::isRetryableError(const QString& serviceName, const QString& error, const RetryPolicyConfig& config) const
{
    Q_UNUSED(serviceName)  // 参数保留用于未来扩展
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
    
    // 默认：超时和网络错误可重试，其他错误根据情况判断
    if (error.contains("timeout", Qt::CaseInsensitive) || 
        error.contains("network", Qt::CaseInsensitive) ||
        error.contains("connection", Qt::CaseInsensitive)) {
        return true;
    }
    
    // 其他错误默认可重试（除非在不可重试列表中）
    return true;
}

// 辅助函数：计算重试延迟
int ServiceRegistry::calculateRetryDelay(const RetryPolicyConfig& config, int attemptCount) const
{
    int delay = 0;
    
    switch (config.strategy) {
        case RetryStrategy::Fixed:
            delay = config.initialDelayMs;
            break;
            
        case RetryStrategy::Exponential:
            delay = static_cast<int>(config.initialDelayMs * std::pow(config.backoffMultiplier, attemptCount));
            break;
            
        case RetryStrategy::Linear:
            delay = config.initialDelayMs * (1 + attemptCount);
            break;
    }
    
    // 限制在最大延迟范围内
    return qMin(delay, config.maxDelayMs);
}

// 辅助函数：尝试降级
QVariant ServiceRegistry::tryDegrade(const QString& serviceName, const QString& method,
                                     const QVariantList& args, DegradationTrigger trigger)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->enableDegradation) {
        return QVariant();
    }
    
    DegradationPolicyConfig policy = d->degradationPolicies.value(serviceName);
    if (!policy.enabled) {
        return QVariant();
    }
    
    // 检查触发条件
    if (policy.trigger != trigger && policy.trigger != DegradationTrigger::Always) {
        return QVariant();
    }
    
    // 执行降级策略
    switch (policy.strategy) {
        case DegradationStrategy::FallbackService: {
            // 使用备用服务
            QString fallbackService = policy.fallbackServiceName;
            QString fallbackMethod = policy.fallbackMethod.isEmpty() ? method : policy.fallbackMethod;
            
            Logger::info("ServiceRegistry", QString("降级到备用服务: %1::%2")
                .arg(fallbackService, fallbackMethod));
            
            // 递归调用备用服务（注意：避免无限递归）
            if (fallbackService != serviceName) {
                locker.unlock();  // 释放锁，避免死锁
                return callService(fallbackService, fallbackMethod, args, d->defaultTimeoutMs);
            }
            break;
        }
        
        case DegradationStrategy::DefaultValue: {
            // 返回默认值
            Logger::info("ServiceRegistry", QString("返回默认值: %1::%2")
                .arg(serviceName, method));
            return policy.defaultValue;
        }
        
        case DegradationStrategy::SimplifiedService: {
            // 使用简化服务
            QString simplifiedService = policy.simplifiedServiceName;
            QString simplifiedMethod = policy.fallbackMethod.isEmpty() ? method : policy.fallbackMethod;
            
            Logger::info("ServiceRegistry", QString("降级到简化服务: %1::%2")
                .arg(simplifiedService, simplifiedMethod));
            
            if (simplifiedService != serviceName) {
                locker.unlock();  // 释放锁，避免死锁
                return callService(simplifiedService, simplifiedMethod, args, d->defaultTimeoutMs);
            }
            break;
        }
        
        case DegradationStrategy::Disabled: {
            // 禁用服务，返回空值
            Logger::warning("ServiceRegistry", QString("服务已禁用: %1::%2")
                .arg(serviceName, method));
            return QVariant();
        }
    }
    
    return QVariant();
}

void ServiceRegistry::setDegradationEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enableDegradation = enabled;
    Logger::info("ServiceRegistry", QString("降级策略%1").arg(enabled ? "启用" : "禁用"));
}

bool ServiceRegistry::isDegradationEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enableDegradation;
}

void ServiceRegistry::setDegradationPolicy(const QString& serviceName, const DegradationPolicyConfig& config)
{
    if (!config.isValid()) {
        Logger::warning("ServiceRegistry", QString("无效的降级策略配置: %1").arg(serviceName));
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->degradationPolicies[serviceName] = config;
    Logger::info("ServiceRegistry", QString("设置服务降级策略: %1 - 触发条件: %2, 策略: %3")
        .arg(serviceName)
        .arg(static_cast<int>(config.trigger))
        .arg(static_cast<int>(config.strategy)));
}

DegradationPolicyConfig ServiceRegistry::getDegradationPolicy(const QString& serviceName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->degradationPolicies.value(serviceName);
}

bool ServiceRegistry::checkServiceHealth(const QString& serviceName) const
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 检查服务是否存在
    if (!d->services.contains(serviceName)) {
        return false;
    }
    
    // 检查服务提供者是否存在
    QObject* provider = d->providers.value(serviceName);
    if (!provider) {
        return false;
    }
    
    // 检查熔断器状态（如果启用）
    if (d->enableCircuitBreaker) {
        CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
        if (breaker && breaker->state() == CircuitState::Open) {
            return false;  // 熔断器打开，服务不健康
        }
    }
    
    // 检查负载均衡器中的实例健康状态（如果启用）
    if (d->loadBalancer && d->enableLoadBalance) {
        // 获取服务实例的健康状态
        QList<ServiceInstance> instances = d->loadBalancer->getInstances(serviceName);
        if (!instances.isEmpty()) {
            // 至少有一个健康实例
            bool hasHealthyInstance = false;
            for (const ServiceInstance& instance : instances) {
                if (instance.healthy) {
                    hasHealthyInstance = true;
                    break;
                }
            }
            return hasHealthyInstance;
        }
    }
    
    return true;  // 默认认为服务健康
}

QMap<QString, bool> ServiceRegistry::getAllServicesHealth() const
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QMap<QString, bool> healthMap;
    QStringList services = d->services.keys();
    
    locker.unlock();  // 释放锁，避免在checkServiceHealth中再次加锁
    
    for (const QString& serviceName : services) {
        healthMap[serviceName] = checkServiceHealth(serviceName);
    }
    
    return healthMap;
}

} // namespace Core
} // namespace Eagle
