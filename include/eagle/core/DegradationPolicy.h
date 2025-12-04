#ifndef EAGLE_CORE_DEGRADATIONPOLICY_H
#define EAGLE_CORE_DEGRADATIONPOLICY_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QStringList>

namespace Eagle {
namespace Core {

// Forward declaration
class DegradationPolicyPrivate;

/**
 * @brief 降级触发条件
 */
enum class DegradationTrigger {
    CircuitBreakerOpen,    // 熔断器开启
    Timeout,               // 超时
    ErrorRate,             // 错误率过高
    Manual,                // 手动触发
    Always                 // 总是降级（用于测试）
};

/**
 * @brief 降级方案类型
 */
enum class DegradationStrategy {
    FallbackService,       // 使用备用服务
    DefaultValue,          // 返回默认值
    SimplifiedService,    // 使用简化服务
    Disabled              // 禁用服务（返回空值）
};

/**
 * @brief 降级策略配置
 */
struct DegradationPolicyConfig {
    QString serviceName;                    // 服务名称
    DegradationTrigger trigger;             // 触发条件
    DegradationStrategy strategy;           // 降级策略
    QString fallbackServiceName;            // 备用服务名称（FallbackService时使用）
    QString fallbackMethod;                 // 备用方法名称
    QVariant defaultValue;                  // 默认值（DefaultValue时使用）
    QString simplifiedServiceName;          // 简化服务名称（SimplifiedService时使用）
    double errorRateThreshold;              // 错误率阈值（0.0-1.0，ErrorRate时使用）
    int errorRateWindowMs;                  // 错误率统计窗口（毫秒）
    bool enabled;                            // 是否启用降级
    
    DegradationPolicyConfig()
        : trigger(DegradationTrigger::CircuitBreakerOpen)
        , strategy(DegradationStrategy::DefaultValue)
        , errorRateThreshold(0.5)
        , errorRateWindowMs(60000)
        , enabled(true)
    {
    }
    
    bool isValid() const {
        if (!enabled) return true;
        
        switch (strategy) {
            case DegradationStrategy::FallbackService:
                return !fallbackServiceName.isEmpty();
            case DegradationStrategy::SimplifiedService:
                return !simplifiedServiceName.isEmpty();
            case DegradationStrategy::DefaultValue:
            case DegradationStrategy::Disabled:
                return true;
        }
        return false;
    }
};

/**
 * @brief 降级策略管理器
 * 
 * 提供降级策略的配置和管理功能
 */
class DegradationPolicy : public QObject {
    Q_OBJECT
    
public:
    explicit DegradationPolicy(QObject* parent = nullptr);
    ~DegradationPolicy();
    
    // 配置降级策略
    void setPolicy(const QString& serviceName, const DegradationPolicyConfig& config);
    void removePolicy(const QString& serviceName);
    DegradationPolicyConfig getPolicy(const QString& serviceName) const;
    void setDefaultPolicy(const DegradationPolicyConfig& config);
    DegradationPolicyConfig getDefaultPolicy() const;
    
    // 判断是否应该降级
    bool shouldDegrade(const QString& serviceName, 
                      DegradationTrigger trigger,
                      double errorRate = 0.0) const;
    
    // 获取降级策略
    DegradationPolicyConfig getDegradationStrategy(const QString& serviceName,
                                                   DegradationTrigger trigger,
                                                   double errorRate = 0.0) const;
    
    // 启用/禁用降级
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // 启用/禁用特定服务的降级
    void setServiceEnabled(const QString& serviceName, bool enabled);
    bool isServiceEnabled(const QString& serviceName) const;
    
signals:
    void degradationTriggered(const QString& serviceName, 
                              DegradationTrigger trigger,
                              DegradationStrategy strategy);
    void degradationRecovered(const QString& serviceName);
    
private:
    Q_DISABLE_COPY(DegradationPolicy)
    
    DegradationPolicyPrivate* d;
    
    inline DegradationPolicyPrivate* d_func() { return d; }
    inline const DegradationPolicyPrivate* d_func() const { return d; }
    
    // 检查触发条件是否匹配
    bool checkTriggerCondition(const DegradationPolicyConfig& config,
                               DegradationTrigger trigger,
                               double errorRate) const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::DegradationTrigger)
Q_DECLARE_METATYPE(Eagle::Core::DegradationStrategy)
Q_DECLARE_METATYPE(Eagle::Core::DegradationPolicyConfig)

#endif // EAGLE_CORE_DEGRADATIONPOLICY_H
