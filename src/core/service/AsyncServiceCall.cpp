#include "eagle/core/AsyncServiceCall.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/Logger.h"
#include <QtCore/QThreadPool>
#include <QtCore/QMutexLocker>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <exception>

namespace Eagle {
namespace Core {

// ==================== ServiceFuture ====================

ServiceFuture::ServiceFuture(QObject* parent)
    : QObject(parent)
    , finishedFlag(false)
{
}

ServiceFuture::~ServiceFuture()
{
}

ServiceCallResult ServiceFuture::wait(int timeoutMs)
{
    QMutexLocker locker(&mutex);
    
    if (finishedFlag) {
        return callResult;
    }
    
    if (timeoutMs < 0) {
        // 无限等待
        condition.wait(&mutex);
    } else {
        // 超时等待
        bool timedOut = !condition.wait(&mutex, timeoutMs);
        if (timedOut) {
            ServiceCallResult timeoutResult;
            timeoutResult.success = false;
            timeoutResult.error = "Timeout waiting for service call result";
            timeoutResult.elapsedMs = timeoutMs;
            return timeoutResult;
        }
    }
    
    return callResult;
}

bool ServiceFuture::isFinished() const
{
    QMutexLocker locker(&mutex);
    return finishedFlag;
}

bool ServiceFuture::isSuccess() const
{
    QMutexLocker locker(&mutex);
    return finishedFlag && callResult.success;
}

QVariant ServiceFuture::result() const
{
    QMutexLocker locker(&mutex);
    if (finishedFlag && callResult.success) {
        return callResult.result;
    }
    return QVariant();
}

QString ServiceFuture::error() const
{
    QMutexLocker locker(&mutex);
    if (finishedFlag && !callResult.success) {
        return callResult.error;
    }
    return QString();
}

int ServiceFuture::elapsedMs() const
{
    QMutexLocker locker(&mutex);
    return callResult.elapsedMs;
}

ServiceFuture* ServiceFuture::then(std::function<QVariant(const QVariant&)> callback)
{
    ServiceFuture* nextFuture = new ServiceFuture(this);
    
    connect(this, &ServiceFuture::succeeded, [nextFuture, callback](const QVariant& result) {
        try {
            QVariant newResult = callback(result);
            ServiceCallResult callResult(newResult);
            nextFuture->setResult(callResult);
        } catch (const std::exception& e) {
            ServiceCallResult errorResult(QString("Exception in then callback: %1").arg(e.what()));
            nextFuture->setResult(errorResult);
        }
    });
    
    connect(this, &ServiceFuture::failed, [nextFuture](const QString& error) {
        ServiceCallResult errorResult(error);
        nextFuture->setResult(errorResult);
    });
    
    return nextFuture;
}

ServiceFuture* ServiceFuture::onError(std::function<void(const QString&)> callback)
{
    connect(this, &ServiceFuture::failed, callback);
    return this;
}

ServiceFuture* ServiceFuture::finally(std::function<void()> callback)
{
    connect(this, &ServiceFuture::finished, [callback](const ServiceCallResult&) {
        callback();
    });
    return this;
}

void ServiceFuture::setResult(const ServiceCallResult& result)
{
    QMutexLocker locker(&mutex);
    callResult = result;
    finishedFlag = true;
    locker.unlock();
    condition.wakeAll();
    
    emit finished(result);
    if (result.success) {
        emit succeeded(result.result);
    } else {
        emit failed(result.error);
    }
}

// ==================== AsyncServiceCall ====================

AsyncServiceCall::AsyncServiceCall(ServiceRegistry* serviceRegistry, QObject* parent)
    : QObject(parent)
    , serviceRegistry(serviceRegistry)
    , threadPool(QThreadPool::globalInstance())
{
    if (!threadPool) {
        threadPool = new QThreadPool(this);
        threadPool->setMaxThreadCount(10);
    }
}

AsyncServiceCall::~AsyncServiceCall()
{
}

ServiceFuture* AsyncServiceCall::callAsync(const QString& serviceName, 
                                          const QString& method,
                                          const QVariantList& args,
                                          int timeout)
{
    ServiceFuture* future = new ServiceFuture(this);
    
    emit callStarted(serviceName, method);
    
    // 使用QtConcurrent在后台线程执行
    QFuture<ServiceCallResult> qtFuture = QtConcurrent::run(threadPool, [=]() {
        return executeCall(serviceName, method, args, timeout);
    });
    
    // 使用QFutureWatcher监控完成
    QFutureWatcher<ServiceCallResult>* watcher = new QFutureWatcher<ServiceCallResult>(this);
    connect(watcher, &QFutureWatcher<ServiceCallResult>::finished, [=]() {
        ServiceCallResult result = qtFuture.result();
        future->setResult(result);
        emit callFinished(serviceName, method, result);
        watcher->deleteLater();
    });
    
    watcher->setFuture(qtFuture);
    
    return future;
}

void AsyncServiceCall::callAsync(const QString& serviceName,
                                 const QString& method,
                                 const QVariantList& args,
                                 std::function<void(const QVariant&)> onSuccess,
                                 std::function<void(const QString&)> onError,
                                 int timeout)
{
    ServiceFuture* future = callAsync(serviceName, method, args, timeout);
    
    connect(future, &ServiceFuture::succeeded, [onSuccess](const QVariant& result) {
        if (onSuccess) {
            onSuccess(result);
        }
    });
    
    connect(future, &ServiceFuture::failed, [onError](const QString& error) {
        if (onError) {
            onError(error);
        }
    });
}

QList<ServiceFuture*> AsyncServiceCall::callBatch(const QList<QPair<QString, QPair<QString, QVariantList>>>& calls)
{
    QList<ServiceFuture*> futures;
    
    for (const auto& call : calls) {
        const QString& serviceName = call.first;
        const QString& method = call.second.first;
        const QVariantList& args = call.second.second;
        
        ServiceFuture* future = callAsync(serviceName, method, args);
        futures.append(future);
    }
    
    return futures;
}

bool AsyncServiceCall::waitForAll(const QList<ServiceFuture*>& futures, int timeoutMs)
{
    if (futures.isEmpty()) {
        return true;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    for (ServiceFuture* future : futures) {
        int remainingTime = timeoutMs < 0 ? -1 : qMax(0, timeoutMs - static_cast<int>(timer.elapsed()));
        ServiceCallResult result = future->wait(remainingTime);
        
        if (!result.success) {
            return false;
        }
        
        if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
            return false;
        }
    }
    
    return true;
}

ServiceFuture* AsyncServiceCall::waitForAny(const QList<ServiceFuture*>& futures, int timeoutMs)
{
    if (futures.isEmpty()) {
        return nullptr;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    while (true) {
        for (ServiceFuture* future : futures) {
            if (future->isFinished()) {
                return future;
            }
        }
        
        if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
            return nullptr;
        }
        
        QThread::msleep(10); // 短暂休眠，避免CPU占用过高
    }
}

ServiceCallResult AsyncServiceCall::executeCall(const QString& serviceName, 
                                                const QString& method,
                                                const QVariantList& args,
                                                int timeout)
{
    QElapsedTimer timer;
    timer.start();
    
    if (!serviceRegistry) {
        return ServiceCallResult(QString("ServiceRegistry not available"));
    }
    
    try {
        QVariant result = serviceRegistry->callService(serviceName, method, args, timeout);
        int elapsed = static_cast<int>(timer.elapsed());
        
        ServiceCallResult callResult(result);
        callResult.elapsedMs = elapsed;
        return callResult;
    } catch (const std::exception& e) {
        int elapsed = static_cast<int>(timer.elapsed());
        ServiceCallResult errorResult(QString("Exception: %1").arg(e.what()));
        errorResult.elapsedMs = elapsed;
        return errorResult;
    } catch (...) {
        int elapsed = static_cast<int>(timer.elapsed());
        ServiceCallResult errorResult(QString("Unknown exception occurred"));
        errorResult.elapsedMs = elapsed;
        return errorResult;
    }
}

} // namespace Core
} // namespace Eagle
