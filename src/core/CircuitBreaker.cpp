#include "eagle/core/CircuitBreaker.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>

namespace Eagle {
namespace Core {

CircuitBreaker::CircuitBreaker(const QString& serviceName, 
                               const CircuitBreakerConfig& config,
                               QObject* parent)
    : QObject(parent)
    , m_serviceName(serviceName)
    , m_config(config)
    , m_state(CircuitState::Closed)
    , m_failureCount(0)
    , m_successCount(0)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &CircuitBreaker::onTimeout);
    
    m_halfOpenTimer = new QTimer(this);
    m_halfOpenTimer->setSingleShot(true);
    connect(m_halfOpenTimer, &QTimer::timeout, this, &CircuitBreaker::onHalfOpenTimeout);
}

bool CircuitBreaker::allowCall()
{
    QMutexLocker locker(&m_mutex);
    
    switch (m_state) {
    case CircuitState::Closed:
        return true;
        
    case CircuitState::Open:
        // 检查是否超时，可以进入半开状态
        if (m_lastFailureTime.isValid()) {
            int elapsed = m_lastFailureTime.msecsTo(QDateTime::currentDateTime());
            if (elapsed >= m_config.timeoutMs) {
                setState(CircuitState::HalfOpen);
                m_successCount = 0;
                m_halfOpenTimer->start(m_config.halfOpenTimeoutMs);
                Logger::info("CircuitBreaker", QString("熔断器进入半开状态: %1").arg(m_serviceName));
                return true;
            }
        }
        return false;
        
    case CircuitState::HalfOpen:
        return true;
    }
    
    return false;
}

void CircuitBreaker::recordSuccess()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_state == CircuitState::HalfOpen) {
        m_successCount++;
        if (m_successCount >= m_config.successThreshold) {
            setState(CircuitState::Closed);
            m_failureCount = 0;
            m_halfOpenTimer->stop();
            Logger::info("CircuitBreaker", QString("熔断器恢复正常: %1").arg(m_serviceName));
        }
    } else if (m_state == CircuitState::Closed) {
        // 正常状态下，成功调用可以重置失败计数
        m_failureCount = 0;
    }
}

void CircuitBreaker::recordFailure()
{
    QMutexLocker locker(&m_mutex);
    
    m_failureCount++;
    m_lastFailureTime = QDateTime::currentDateTime();
    
    if (m_state == CircuitState::HalfOpen) {
        // 半开状态下失败，立即熔断
        setState(CircuitState::Open);
        m_halfOpenTimer->stop();
        m_timeoutTimer->start(m_config.timeoutMs);
        Logger::warning("CircuitBreaker", QString("熔断器再次开启: %1").arg(m_serviceName));
    } else if (m_state == CircuitState::Closed) {
        if (m_failureCount >= m_config.failureThreshold) {
            setState(CircuitState::Open);
            m_timeoutTimer->start(m_config.timeoutMs);
            Logger::error("CircuitBreaker", QString("熔断器开启: %1 (失败次数: %2)")
                .arg(m_serviceName).arg(m_failureCount));
        }
    }
}

CircuitState CircuitBreaker::state() const
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

QString CircuitBreaker::serviceName() const
{
    return m_serviceName;
}

void CircuitBreaker::reset()
{
    QMutexLocker locker(&m_mutex);
    setState(CircuitState::Closed);
    m_failureCount = 0;
    m_successCount = 0;
    m_lastFailureTime = QDateTime();
    m_timeoutTimer->stop();
    m_halfOpenTimer->stop();
    Logger::info("CircuitBreaker", QString("熔断器重置: %1").arg(m_serviceName));
}

void CircuitBreaker::onTimeout()
{
    QMutexLocker locker(&m_mutex);
    if (m_state == CircuitState::Open) {
        setState(CircuitState::HalfOpen);
        m_successCount = 0;
        m_halfOpenTimer->start(m_config.halfOpenTimeoutMs);
        Logger::info("CircuitBreaker", QString("熔断器超时，进入半开状态: %1").arg(m_serviceName));
    }
}

void CircuitBreaker::onHalfOpenTimeout()
{
    QMutexLocker locker(&m_mutex);
    if (m_state == CircuitState::HalfOpen) {
        // 半开状态下没有达到成功阈值，重新熔断
        setState(CircuitState::Open);
        m_timeoutTimer->start(m_config.timeoutMs);
        Logger::warning("CircuitBreaker", QString("半开状态超时，重新熔断: %1").arg(m_serviceName));
    }
}

void CircuitBreaker::setState(CircuitState newState)
{
    if (m_state != newState) {
        CircuitState oldState = m_state;
        m_state = newState;
        emit stateChanged(m_serviceName, oldState, newState);
    }
}

} // namespace Core
} // namespace Eagle
