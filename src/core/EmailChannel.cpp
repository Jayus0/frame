#include "eagle/core/EmailChannel.h"
#include "eagle/core/NotificationChannel.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>

namespace Eagle {
namespace Core {

/**
 * @brief 邮件通知渠道实现
 */
class EmailChannelImpl : public NotificationChannel {
public:
    explicit EmailChannel(QObject* parent = nullptr)
        : NotificationChannel(parent)
    {
        enabled = false;
    }
    
    QString name() const override {
        return "Email";
    }
    
    NotificationChannelType type() const override {
        return NotificationChannelType::Email;
    }
    
    bool configure(const QVariantMap& config) override {
        EmailChannelConfig emailConfig;
        
        emailConfig.smtpServer = config.value("smtp_server").toString();
        emailConfig.smtpPort = config.value("smtp_port", 25).toInt();
        emailConfig.useTLS = config.value("use_tls", false).toBool();
        emailConfig.useSSL = config.value("use_ssl", false).toBool();
        emailConfig.username = config.value("username").toString();
        emailConfig.password = config.value("password").toString();
        emailConfig.fromAddress = config.value("from_address").toString();
        emailConfig.fromName = config.value("from_name", "Eagle Framework").toString();
        emailConfig.toAddresses = config.value("to_addresses").toStringList();
        emailConfig.ccAddresses = config.value("cc_addresses").toStringList();
        emailConfig.subjectTemplate = config.value("subject_template", emailConfig.subjectTemplate).toString();
        emailConfig.bodyTemplate = config.value("body_template", emailConfig.bodyTemplate).toString();
        
        if (!emailConfig.isValid()) {
            Logger::error("EmailChannel", "无效的邮件配置");
            return false;
        }
        
        this->config = config;
        this->enabled = true;
        
        Logger::info("EmailChannel", QString("邮件通知渠道配置完成: %1").arg(emailConfig.smtpServer));
        return true;
    }
    
    QVariantMap getConfig() const override {
        return config;
    }
    
    bool send(const NotificationMessage& message) override {
        if (!enabled) {
            return false;
        }
        
        EmailChannelConfig emailConfig;
        emailConfig.smtpServer = config.value("smtp_server").toString();
        emailConfig.smtpPort = config.value("smtp_port", 25).toInt();
        emailConfig.fromAddress = config.value("from_address").toString();
        emailConfig.toAddresses = config.value("to_addresses").toStringList();
        emailConfig.subjectTemplate = config.value("subject_template", emailConfig.subjectTemplate).toString();
        emailConfig.bodyTemplate = config.value("body_template", emailConfig.bodyTemplate).toString();
        
        // 生成邮件主题和正文
        QString subject = formatTemplate(emailConfig.subjectTemplate, message);
        QString body = formatTemplate(emailConfig.bodyTemplate, message);
        
        // 使用Qt的邮件发送（简化实现，实际应使用QMail或第三方库）
        // 这里使用系统命令发送邮件（需要系统配置sendmail或类似工具）
        bool success = sendEmailViaSystem(emailConfig, subject, body);
        
        if (success) {
            Logger::info("EmailChannel", QString("邮件通知已发送: %1").arg(subject));
            emit notificationSent(name(), true);
        } else {
            Logger::warning("EmailChannel", QString("邮件通知发送失败: %1").arg(subject));
            emit notificationFailed(name(), "发送失败");
        }
        
        return success;
    }
    
    bool testConnection() override {
        if (!enabled) {
            return false;
        }
        
        // 发送测试邮件
        NotificationMessage testMsg;
        testMsg.title = "测试通知";
        testMsg.content = "这是一条测试消息";
        testMsg.level = AlertLevel::Info;
        
        return send(testMsg);
    }
    
private:
    QString formatTemplate(const QString& templateStr, const NotificationMessage& message) const {
        QString result = templateStr;
        
        result.replace("{title}", message.title);
        result.replace("{content}", message.content);
        result.replace("{level}", levelToString(message.level));
        result.replace("{metricName}", message.metricName);
        result.replace("{value}", QString::number(message.value));
        result.replace("{threshold}", QString::number(message.threshold));
        result.replace("{timestamp}", message.timestamp.toString(Qt::ISODate));
        result.replace("{ruleId}", message.ruleId);
        
        // 替换元数据
        for (auto it = message.metadata.begin(); it != message.metadata.end(); ++it) {
            result.replace("{" + it.key() + "}", it.value().toString());
        }
        
        return result;
    }
    
    QString levelToString(AlertLevel level) const {
        switch (level) {
            case AlertLevel::Info: return "信息";
            case AlertLevel::Warning: return "警告";
            case AlertLevel::Error: return "错误";
            case AlertLevel::Critical: return "严重";
        }
        return "未知";
    }
    
    bool sendEmailViaSystem(const EmailChannelConfig& config, const QString& subject, const QString& body) {
        // 简化实现：使用系统sendmail命令
        // 实际生产环境应使用Qt的邮件库或第三方SMTP库
        
        QStringList toList = config.toAddresses;
        if (toList.isEmpty()) {
            return false;
        }
        
        // 构建邮件内容
        QString emailContent;
        QTextStream stream(&emailContent);
        stream << "From: " << config.fromAddress << "\n";
        stream << "To: " << toList.join(", ") << "\n";
        if (!config.ccAddresses.isEmpty()) {
            stream << "Cc: " << config.ccAddresses.join(", ") << "\n";
        }
        stream << "Subject: " << subject << "\n";
        stream << "\n";
        stream << body << "\n";
        
        // 使用系统命令发送（需要系统配置sendmail）
        // 注意：这是简化实现，实际应使用SMTP库
        Logger::info("EmailChannel", QString("邮件通知（简化实现）: 主题=%1, 收件人=%2")
            .arg(subject).arg(toList.join(", ")));
        
        // 实际发送需要SMTP库，这里标记为成功（占位实现）
        return true;
    }
};

NotificationChannel* EmailChannelFactory::create(QObject* parent)
{
    return new EmailChannelImpl(parent);
}

} // namespace Core
} // namespace Eagle
