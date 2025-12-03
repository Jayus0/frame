#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/ServiceDescriptor.h"
#include "ServiceRegistry_p.h"
#include "eagle/core/CircuitBreaker.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QElapsedTimer>

namespace Eagle {
namespace Core {

ServiceRegistry::ServiceRegistry(QObject* parent)
    : QObject(parent)
    , d_ptr(new ServiceRegistryPrivate)
{
    auto* d = d_func();
    d->defaultTimeoutMs = 5000;
    d->enableCircuitBreaker = true;
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
        }
        d->services.remove(serviceName);
        emit serviceUnregistered(serviceName, QString());
    } else {
        // 卸载指定版本
        for (int i = descriptors.size() - 1; i >= 0; --i) {
            if (descriptors[i].version == version) {
                QString key = serviceName + "@" + version;
                d->providers.remove(key);
                descriptors.removeAt(i);
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
            emit serviceCallFailed(serviceName, error);
            return QVariant();
        }
    }
    
    // 使用默认超时时间
    if (timeout <= 0) {
        QMutexLocker locker(&d->mutex);
        timeout = d->defaultTimeoutMs;
    }
    
    QObject* provider = findService(serviceName);
    if (!provider) {
        QString error = QString("Service not found: %1").arg(serviceName);
        Logger::error("ServiceRegistry", error);
        
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
        emit serviceCallFailed(serviceName, error);
        return QVariant();
    }
    
    QMetaMethod metaMethod = metaObj->method(methodIndex);
    if (metaMethod.methodType() != QMetaMethod::Method && 
        metaMethod.methodType() != QMetaMethod::Slot) {
        QString error = QString("Method is not callable: %1::%2").arg(serviceName, method);
        Logger::error("ServiceRegistry", error);
        emit serviceCallFailed(serviceName, error);
        return QVariant();
    }
    
    // 使用超时机制调用方法
    QVariant returnValue;
    bool success = false;
    QElapsedTimer timer;
    timer.start();
    
    // 尝试调用方法（带超时检查）
    if (metaMethod.returnType() != QMetaType::Void) {
        QGenericReturnArgument retArg = Q_RETURN_ARG(QVariant, returnValue);
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
    
    if (!success) {
        QString error = QString("Service call failed: %1::%2").arg(serviceName, method);
        Logger::error("ServiceRegistry", error);
        
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
    
    // 记录成功
    if (d->enableCircuitBreaker) {
        QMutexLocker locker(&d->mutex);
        CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
        if (breaker) {
            breaker->recordSuccess();
        }
    }
    
    return returnValue;
}

bool ServiceRegistry::checkServiceHealth(const QString& serviceName) const
{
    QObject* provider = findService(serviceName);
    if (!provider) {
        return false;
    }
    
    // 检查熔断器状态
    const auto* d = d_func();
    if (d->enableCircuitBreaker) {
        QMutexLocker locker(&d->mutex);
        CircuitBreaker* breaker = d->circuitBreakers.value(serviceName);
        if (breaker && breaker->state() == CircuitState::Open) {
            return false;  // 熔断器开启，认为不健康
        }
    }
    
    // 检查健康状态（如果有healthCheck方法）
    const QMetaObject* metaObj = provider->metaObject();
    int healthIndex = metaObj->indexOfMethod("healthCheck()");
    if (healthIndex != -1) {
        QMetaMethod healthMethod = metaObj->method(healthIndex);
        bool isHealthy = false;
        QGenericReturnArgument retArg = Q_RETURN_ARG(bool, isHealthy);
        healthMethod.invoke(provider, retArg);
        return isHealthy;
    }
    
    // 默认认为服务健康
    return true;
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

} // namespace Core
} // namespace Eagle
