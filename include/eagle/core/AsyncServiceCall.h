#ifndef EAGLE_CORE_ASYNCSERVICECALL_H
#define EAGLE_CORE_ASYNCSERVICECALL_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <functional>

namespace Eagle {
namespace Core {

class ServiceRegistry;

/**
 * @brief 服务调用结果
 */
struct ServiceCallResult {
    bool success;           // 是否成功
    QVariant result;        // 返回结果
    QString error;          // 错误信息
    int elapsedMs;          // 耗时（毫秒）
    
    ServiceCallResult()
        : success(false)
        , elapsedMs(0)
    {}
    
    ServiceCallResult(const QVariant& res)
        : success(true)
        , result(res)
        , elapsedMs(0)
    {}
    
    ServiceCallResult(const QString& err)
        : success(false)
        , error(err)
        , elapsedMs(0)
    {}
};

/**
 * @brief 服务Future类（Promise/Future模式）
 * 
 * 提供异步服务调用的结果获取和链式操作
 */
class ServiceFuture : public QObject {
    Q_OBJECT
    
public:
    explicit ServiceFuture(QObject* parent = nullptr);
    ~ServiceFuture();
    
    /**
     * @brief 等待结果（阻塞）
     * @param timeoutMs 超时时间（毫秒），-1表示无限等待
     * @return 调用结果
     */
    ServiceCallResult wait(int timeoutMs = -1);
    
    /**
     * @brief 检查是否完成
     */
    bool isFinished() const;
    
    /**
     * @brief 检查是否成功
     */
    bool isSuccess() const;
    
    /**
     * @brief 获取结果（非阻塞，如果未完成返回无效值）
     */
    QVariant result() const;
    
    /**
     * @brief 获取错误信息
     */
    QString error() const;
    
    /**
     * @brief 获取耗时
     */
    int elapsedMs() const;
    
    /**
     * @brief 链式操作：成功后执行
     * @param callback 回调函数
     * @return 新的Future
     */
    ServiceFuture* then(std::function<QVariant(const QVariant&)> callback);
    
    /**
     * @brief 链式操作：失败后执行
     * @param callback 错误处理函数
     * @return 新的Future
     */
    ServiceFuture* onError(std::function<void(const QString&)> callback);
    
    /**
     * @brief 链式操作：无论成功失败都执行
     * @param callback 完成回调函数
     * @return 新的Future
     */
    ServiceFuture* finally(std::function<void()> callback);
    
signals:
    void finished(const ServiceCallResult& result);
    void succeeded(const QVariant& result);
    void failed(const QString& error);
    
private:
    friend class AsyncServiceCall;
    
    void setResult(const ServiceCallResult& result);
    
    mutable QMutex mutex;
    mutable QWaitCondition condition;
    ServiceCallResult callResult;
    bool finishedFlag;
    
    Q_DISABLE_COPY(ServiceFuture)
};

/**
 * @brief 异步服务调用器
 * 
 * 提供异步服务调用功能，支持Future/Promise模式和回调机制
 */
class AsyncServiceCall : public QObject {
    Q_OBJECT
    
public:
    explicit AsyncServiceCall(ServiceRegistry* serviceRegistry, QObject* parent = nullptr);
    ~AsyncServiceCall();
    
    /**
     * @brief 异步调用服务（返回Future）
     * @param serviceName 服务名称
     * @param method 方法名
     * @param args 参数列表
     * @param timeout 超时时间（毫秒）
     * @return ServiceFuture对象
     */
    ServiceFuture* callAsync(const QString& serviceName, 
                            const QString& method,
                            const QVariantList& args = QVariantList(),
                            int timeout = 5000);
    
    /**
     * @brief 异步调用服务（使用回调）
     * @param serviceName 服务名称
     * @param method 方法名
     * @param args 参数列表
     * @param onSuccess 成功回调
     * @param onError 错误回调
     * @param timeout 超时时间（毫秒）
     */
    void callAsync(const QString& serviceName,
                  const QString& method,
                  const QVariantList& args,
                  std::function<void(const QVariant&)> onSuccess,
                  std::function<void(const QString&)> onError,
                  int timeout = 5000);
    
    /**
     * @brief 批量异步调用
     * @param calls 调用列表（serviceName, method, args）
     * @return Future列表
     */
    QList<ServiceFuture*> callBatch(const QList<QPair<QString, QPair<QString, QVariantList>>>& calls);
    
    /**
     * @brief 等待所有调用完成
     * @param futures Future列表
     * @param timeoutMs 超时时间（毫秒）
     * @return 是否全部成功
     */
    bool waitForAll(const QList<ServiceFuture*>& futures, int timeoutMs = -1);
    
    /**
     * @brief 等待任意一个调用完成
     * @param futures Future列表
     * @param timeoutMs 超时时间（毫秒）
     * @return 完成的Future，如果超时返回nullptr
     */
    ServiceFuture* waitForAny(const QList<ServiceFuture*>& futures, int timeoutMs = -1);
    
signals:
    void callStarted(const QString& serviceName, const QString& method);
    void callFinished(const QString& serviceName, const QString& method, const ServiceCallResult& result);
    
private slots:
    void onCallFinished();
    
private:
    ServiceRegistry* serviceRegistry;
    QThreadPool* threadPool;
    
    // 执行异步调用
    ServiceCallResult executeCall(const QString& serviceName, 
                                 const QString& method,
                                 const QVariantList& args,
                                 int timeout);
    
    Q_DISABLE_COPY(AsyncServiceCall)
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_ASYNCSERVICECALL_H
