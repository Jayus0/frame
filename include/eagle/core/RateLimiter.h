#ifndef EAGLE_CORE_RATELIMITER_H
#define EAGLE_CORE_RATELIMITER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

/**
 * @brief 限流算法类型
 */
enum class RateLimitAlgorithm {
    TokenBucket,    // 令牌桶算法
    SlidingWindow   // 滑动窗口算法
};

/**
 * @brief 限流器
 * 
 * 支持令牌桶和滑动窗口两种算法
 */
class RateLimiter : public QObject {
    Q_OBJECT
    
public:
    explicit RateLimiter(QObject* parent = nullptr);
    ~RateLimiter();
    
    // 配置限流规则
    void setLimit(const QString& key, int maxRequests, int windowMs, 
                  RateLimitAlgorithm algorithm = RateLimitAlgorithm::TokenBucket);
    void removeLimit(const QString& key);
    void clearLimits();
    
    // 检查是否允许请求
    bool allowRequest(const QString& key);
    bool allowRequest(const QString& key, int maxRequests, int windowMs);
    
    // 获取限流信息
    int getRemainingRequests(const QString& key) const;
    QDateTime getResetTime(const QString& key) const;
    
    // 配置
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
signals:
    void rateLimitExceeded(const QString& key, int maxRequests, int windowMs);
    
private slots:
    void onCleanupTimer();
    
private:
    Q_DISABLE_COPY(RateLimiter)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    // 令牌桶算法
    bool checkTokenBucket(const QString& key, int maxRequests, int windowMs);
    
    // 滑动窗口算法
    bool checkSlidingWindow(const QString& key, int maxRequests, int windowMs);
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::RateLimitAlgorithm)

#endif // EAGLE_CORE_RATELIMITER_H
