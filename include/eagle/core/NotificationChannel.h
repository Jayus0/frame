#ifndef EAGLE_CORE_NOTIFICATIONCHANNEL_H
#define EAGLE_CORE_NOTIFICATIONCHANNEL_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include "AlertSystem.h"

namespace Eagle {
namespace Core {

/**
 * @brief 通知渠道类型
 */
enum class NotificationChannelType {
    Email,      // 邮件
    Webhook,    // Webhook
    SMS,        // 短信（预留）
    Custom      // 自定义
};

/**
 * @brief 通知消息
 */
struct NotificationMessage {
    QString title;              // 标题
    QString content;             // 内容
    AlertLevel level;           // 告警级别
    QString ruleId;             // 规则ID
    QString metricName;         // 指标名称
    double value;                // 当前值
    double threshold;            // 阈值
    QDateTime timestamp;        // 时间戳
    QVariantMap metadata;        // 额外元数据
    
    NotificationMessage()
        : level(AlertLevel::Info)
        , value(0.0)
        , threshold(0.0)
    {
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief 通知渠道基类
 */
class NotificationChannel : public QObject {
    Q_OBJECT
    
public:
    explicit NotificationChannel(QObject* parent = nullptr);
    virtual ~NotificationChannel();
    
    // 渠道信息
    virtual QString name() const = 0;
    virtual NotificationChannelType type() const = 0;
    virtual bool isEnabled() const;
    virtual void setEnabled(bool enabled);
    
    // 配置
    virtual bool configure(const QVariantMap& config) = 0;
    virtual QVariantMap getConfig() const = 0;
    
    // 发送通知
    virtual bool send(const NotificationMessage& message) = 0;
    
    // 测试连接
    virtual bool testConnection() = 0;
    
signals:
    void notificationSent(const QString& channelName, bool success);
    void notificationFailed(const QString& channelName, const QString& error);
    
protected:
    bool enabled;
    QVariantMap config;
};

/**
 * @brief 邮件通知渠道配置
 */
struct EmailChannelConfig {
    QString smtpServer;          // SMTP服务器地址
    int smtpPort;                // SMTP端口（默认25）
    bool useTLS;                  // 是否使用TLS
    bool useSSL;                  // 是否使用SSL
    QString username;             // 用户名
    QString password;             // 密码
    QString fromAddress;          // 发件人地址
    QString fromName;             // 发件人名称
    QStringList toAddresses;      // 收件人地址列表
    QStringList ccAddresses;      // 抄送地址列表
    QString subjectTemplate;     // 主题模板
    QString bodyTemplate;         // 正文模板
    
    EmailChannelConfig()
        : smtpPort(25)
        , useTLS(false)
        , useSSL(false)
    {
        subjectTemplate = "[告警] {title}";
        bodyTemplate = "告警级别: {level}\n指标: {metricName}\n当前值: {value}\n阈值: {threshold}\n时间: {timestamp}\n\n{content}";
    }
    
    bool isValid() const {
        return !smtpServer.isEmpty() && 
               !fromAddress.isEmpty() && 
               !toAddresses.isEmpty();
    }
};

/**
 * @brief Webhook通知渠道配置
 */
struct WebhookChannelConfig {
    QString url;                  // Webhook URL
    QString method;                // HTTP方法（GET/POST，默认POST）
    QVariantMap headers;           // 自定义请求头
    QString bodyTemplate;          // 请求体模板（JSON格式）
    int timeoutMs;                 // 超时时间（毫秒）
    int maxRetries;                // 最大重试次数
    bool verifySSL;                // 是否验证SSL证书
    
    WebhookChannelConfig()
        : method("POST")
        , timeoutMs(5000)
        , maxRetries(3)
        , verifySSL(true)
    {
        headers["Content-Type"] = "application/json";
        bodyTemplate = R"({"title":"{title}","level":"{level}","metricName":"{metricName}","value":{value},"threshold":{threshold},"timestamp":"{timestamp}","content":"{content}"})";
    }
    
    bool isValid() const {
        return !url.isEmpty();
    }
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::NotificationChannelType)
Q_DECLARE_METATYPE(Eagle::Core::NotificationMessage)
Q_DECLARE_METATYPE(Eagle::Core::EmailChannelConfig)
Q_DECLARE_METATYPE(Eagle::Core::WebhookChannelConfig)

#endif // EAGLE_CORE_NOTIFICATIONCHANNEL_H
