#ifndef EAGLE_CORE_ALERTSYSTEM_H
#define EAGLE_CORE_ALERTSYSTEM_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariant>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QtMath>
#include <functional>

namespace Eagle {
namespace Core {

class PerformanceMonitor;
class NotificationChannel;

/**
 * @brief 告警级别
 */
enum class AlertLevel {
    Info,      // 信息
    Warning,   // 警告
    Error,     // 错误
    Critical   // 严重
};

/**
 * @brief 告警规则
 */
struct AlertRule {
    QString id;                    // 规则ID
    QString name;                  // 规则名称
    QString metricName;            // 指标名称
    AlertLevel level;              // 告警级别
    QString condition;             // 条件（如：">", "<", ">=", "<=", "=="）
    double threshold;              // 阈值
    int durationMs;                // 持续时间（毫秒），超过此时间才触发
    bool enabled;                  // 是否启用
    QString description;           // 描述
    
    AlertRule()
        : level(AlertLevel::Warning)
        , threshold(0.0)
        , durationMs(0)
        , enabled(true)
    {}
    
    bool isValid() const {
        return !id.isEmpty() && !metricName.isEmpty() && !condition.isEmpty();
    }
    
    // 检查是否触发告警
    bool check(double value) const {
        if (condition == ">") {
            return value > threshold;
        } else if (condition == "<") {
            return value < threshold;
        } else if (condition == ">=") {
            return value >= threshold;
        } else if (condition == "<=") {
            return value <= threshold;
        } else if (condition == "==") {
            return qAbs(value - threshold) < 0.0001;  // 浮点数比较
        } else if (condition == "!=") {
            return qAbs(value - threshold) >= 0.0001;
        }
        return false;
    }
};

/**
 * @brief 告警记录
 */
struct AlertRecord {
    QString id;                    // 告警ID
    QString ruleId;                // 规则ID
    QString metricName;            // 指标名称
    AlertLevel level;              // 级别
    double value;                  // 触发时的值
    double threshold;              // 阈值
    QDateTime triggerTime;         // 触发时间
    QDateTime resolveTime;         // 解决时间
    bool resolved;                 // 是否已解决
    QString message;               // 告警消息
    
    AlertRecord()
        : level(AlertLevel::Warning)
        , value(0.0)
        , threshold(0.0)
        , resolved(false)
    {
        triggerTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 告警系统
 */
class AlertSystem : public QObject {
    Q_OBJECT
    
public:
    explicit AlertSystem(PerformanceMonitor* monitor, QObject* parent = nullptr);
    ~AlertSystem();
    
    // 规则管理
    bool addRule(const AlertRule& rule);
    bool removeRule(const QString& ruleId);
    bool updateRule(const AlertRule& rule);
    AlertRule getRule(const QString& ruleId) const;
    QStringList getAllRuleIds() const;
    
    // 告警查询
    QList<AlertRecord> getActiveAlerts() const;
    QList<AlertRecord> getAlerts(const QString& ruleId = QString(),
                                AlertLevel minLevel = AlertLevel::Info,
                                const QDateTime& startTime = QDateTime(),
                                const QDateTime& endTime = QDateTime()) const;
    
    // 告警处理
    bool resolveAlert(const QString& alertId);
    bool resolveAlertsByRule(const QString& ruleId);
    
    // 配置
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // 注册告警处理器
    void registerAlertHandler(std::function<void(const AlertRecord&)> handler);
    
    // 通知渠道管理
    bool addNotificationChannel(NotificationChannel* channel);
    bool removeNotificationChannel(const QString& channelName);
    NotificationChannel* getNotificationChannel(const QString& channelName) const;
    QStringList getNotificationChannelNames() const;
    
    // 通知策略配置
    void setNotificationEnabled(bool enabled);
    bool isNotificationEnabled() const;
    void setNotificationLevels(const QList<AlertLevel>& levels);
    QList<AlertLevel> getNotificationLevels() const;
    
signals:
    void alertTriggered(const AlertRecord& alert);
    void alertResolved(const QString& alertId);
    
private slots:
    void onMetricUpdated(const QString& name, double value);
    
private:
    Q_DISABLE_COPY(AlertSystem)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    void checkRules(const QString& metricName, double value);
    QString generateAlertId() const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::AlertRule)
Q_DECLARE_METATYPE(Eagle::Core::AlertRecord)
Q_DECLARE_METATYPE(Eagle::Core::AlertLevel)

#endif // EAGLE_CORE_ALERTSYSTEM_H
