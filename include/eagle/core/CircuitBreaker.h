#ifndef EAGLE_CORE_CIRCUITBREAKER_H
#define EAGLE_CORE_CIRCUITBREAKER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

/**
 * @brief 熔断器状态
 */
enum class CircuitState {
    Closed,    // 关闭状态（正常）
    Open,      // 开启状态（熔断）
    HalfOpen   // 半开状态（尝试恢复）
};

/**
 * @brief 熔断器配置
 */
struct CircuitBreakerConfig {
    int failureThreshold = 5;        // 失败阈值
    int successThreshold = 2;          // 半开状态下的成功阈值
    int timeoutMs = 60000;            // 熔断超时时间（毫秒）
    int halfOpenTimeoutMs = 5000;     // 半开状态超时时间（毫秒）
    
    CircuitBreakerConfig() = default;
};

/**
 * @brief 熔断器
 * 
 * 用于服务调用的熔断保护
 */
class CircuitBreaker : public QObject {
    Q_OBJECT
    
public:
    explicit CircuitBreaker(const QString& serviceName, 
                           const CircuitBreakerConfig& config = CircuitBreakerConfig(),
                           QObject* parent = nullptr);
    
    /**
     * @brief 尝试调用（检查是否允许调用）
     * @return 是否允许调用
     */
    bool allowCall();
    
    /**
     * @brief 记录成功调用
     */
    void recordSuccess();
    
    /**
     * @brief 记录失败调用
     */
    void recordFailure();
    
    /**
     * @brief 获取当前状态
     */
    CircuitState state() const;
    
    /**
     * @brief 获取服务名称
     */
    QString serviceName() const;
    
    /**
     * @brief 重置熔断器
     */
    void reset();
    
signals:
    void stateChanged(const QString& serviceName, CircuitState oldState, CircuitState newState);
    
private slots:
    void onTimeout();
    void onHalfOpenTimeout();
    
private:
    void setState(CircuitState newState);
    void checkHalfOpen();
    
    QString m_serviceName;
    CircuitBreakerConfig m_config;
    CircuitState m_state;
    int m_failureCount;
    int m_successCount;
    QDateTime m_lastFailureTime;
    QTimer* m_timeoutTimer;
    QTimer* m_halfOpenTimer;
    mutable QMutex m_mutex;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CIRCUITBREAKER_H
