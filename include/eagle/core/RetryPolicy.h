#ifndef EAGLE_CORE_RETRYPOLICY_H
#define EAGLE_CORE_RETRYPOLICY_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

// Forward declaration
class RetryPolicyPrivate;

/**
 * @brief 重试策略类型
 */
enum class RetryStrategy {
    Fixed,          // 固定延迟
    Exponential,    // 指数退避
    Linear          // 线性增长
};

/**
 * @brief 重试策略配置
 */
struct RetryPolicyConfig {
    int maxRetries;                 // 最大重试次数（0表示不重试）
    int initialDelayMs;             // 初始延迟（毫秒）
    int maxDelayMs;                 // 最大延迟（毫秒）
    double backoffMultiplier;       // 退避乘数（指数退避使用）
    RetryStrategy strategy;         // 重试策略
    QStringList retryableErrors;    // 可重试的错误类型列表（空表示所有错误都可重试）
    QStringList nonRetryableErrors; // 不可重试的错误类型列表
    
    RetryPolicyConfig()
        : maxRetries(3)
        , initialDelayMs(100)
        , maxDelayMs(5000)
        , backoffMultiplier(2.0)
        , strategy(RetryStrategy::Exponential)
    {
    }
    
    bool isValid() const {
        return maxRetries >= 0 && 
               initialDelayMs >= 0 && 
               maxDelayMs >= initialDelayMs &&
               backoffMultiplier > 0;
    }
};

/**
 * @brief 重试策略管理器
 * 
 * 提供重试策略的配置和执行功能
 */
class RetryPolicy : public QObject {
    Q_OBJECT
    
public:
    explicit RetryPolicy(QObject* parent = nullptr);
    ~RetryPolicy();
    
    // 配置重试策略
    void setPolicy(const QString& key, const RetryPolicyConfig& config);
    void removePolicy(const QString& key);
    RetryPolicyConfig getPolicy(const QString& key) const;
    void setDefaultPolicy(const RetryPolicyConfig& config);
    RetryPolicyConfig getDefaultPolicy() const;
    
    // 判断是否应该重试
    bool shouldRetry(const QString& key, int attemptCount, const QString& error) const;
    
    // 计算重试延迟
    int getRetryDelay(const QString& key, int attemptCount) const;
    
    // 判断错误是否可重试
    bool isRetryableError(const QString& key, const QString& error) const;
    
    // 启用/禁用重试
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
signals:
    void retryTriggered(const QString& key, int attemptCount, const QString& error);
    void retryExhausted(const QString& key, int maxRetries, const QString& error);
    
private:
    Q_DISABLE_COPY(RetryPolicy)
    
    RetryPolicyPrivate* d;
    
    inline RetryPolicyPrivate* d_func() { return d; }
    inline const RetryPolicyPrivate* d_func() const { return d; }
    
    // 计算延迟时间
    int calculateDelay(const RetryPolicyConfig& config, int attemptCount) const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::RetryStrategy)
Q_DECLARE_METATYPE(Eagle::Core::RetryPolicyConfig)

#endif // EAGLE_CORE_RETRYPOLICY_H
