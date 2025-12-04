#include "eagle/core/WebhookChannel.h"
#include "eagle/core/NotificationChannel.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QThread>

namespace Eagle {
namespace Core {

/**
 * @brief Webhook通知渠道实现
 */
class WebhookChannelImpl : public NotificationChannel {
public:
    explicit WebhookChannel(QObject* parent = nullptr)
        : NotificationChannel(parent)
        , networkManager(new QNetworkAccessManager(this))
    {
        enabled = false;
    }
    
    QString name() const override {
        return "Webhook";
    }
    
    NotificationChannelType type() const override {
        return NotificationChannelType::Webhook;
    }
    
    bool configure(const QVariantMap& config) override {
        WebhookChannelConfig webhookConfig;
        
        webhookConfig.url = config.value("url").toString();
        webhookConfig.method = config.value("method", "POST").toString().toUpper();
        webhookConfig.timeoutMs = config.value("timeout_ms", 5000).toInt();
        webhookConfig.maxRetries = config.value("max_retries", 3).toInt();
        webhookConfig.verifySSL = config.value("verify_ssl", true).toBool();
        webhookConfig.bodyTemplate = config.value("body_template", webhookConfig.bodyTemplate).toString();
        
        // 解析headers
        QVariantMap headersConfig = config.value("headers").toMap();
        if (!headersConfig.isEmpty()) {
            webhookConfig.headers = headersConfig;
        } else {
            webhookConfig.headers["Content-Type"] = "application/json";
        }
        
        if (!webhookConfig.isValid()) {
            Logger::error("WebhookChannel", "无效的Webhook配置");
            return false;
        }
        
        this->config = config;
        this->enabled = true;
        
        Logger::info("WebhookChannel", QString("Webhook通知渠道配置完成: %1").arg(webhookConfig.url));
        return true;
    }
    
    QVariantMap getConfig() const override {
        return config;
    }
    
    bool send(const NotificationMessage& message) override {
        if (!enabled) {
            return false;
        }
        
        QString url = config.value("url").toString();
        QString method = config.value("method", "POST").toString().toUpper();
        int timeoutMs = config.value("timeout_ms", 5000).toInt();
        int maxRetries = config.value("max_retries", 3).toInt();
        QString bodyTemplate = config.value("body_template").toString();
        QVariantMap headers = config.value("headers").toMap();
        
        // 格式化请求体
        QString body = formatTemplate(bodyTemplate, message);
        
        // 发送HTTP请求
        for (int attempt = 0; attempt < maxRetries; ++attempt) {
            bool success = sendHttpRequest(url, method, headers, body, timeoutMs);
            if (success) {
                Logger::info("WebhookChannel", QString("Webhook通知已发送: %1").arg(url));
                emit notificationSent(name(), true);
                return true;
            }
            
            if (attempt < maxRetries - 1) {
                Logger::warning("WebhookChannel", QString("Webhook通知发送失败，重试 %1/%2").arg(attempt + 1).arg(maxRetries));
                QThread::msleep(1000 * (attempt + 1));  // 指数退避
            }
        }
        
        Logger::error("WebhookChannel", QString("Webhook通知发送失败（已重试%1次）: %2").arg(maxRetries).arg(url));
        emit notificationFailed(name(), "发送失败");
        return false;
    }
    
    bool testConnection() override {
        if (!enabled) {
            return false;
        }
        
        QString url = config.value("url").toString();
        QString method = config.value("method", "GET").toString().toUpper();
        QVariantMap headers = config.value("headers").toMap();
        
        // 发送测试请求
        return sendHttpRequest(url, method, headers, "", config.value("timeout_ms", 5000).toInt());
    }
    
private:
    QNetworkAccessManager* networkManager;
    
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
            case AlertLevel::Info: return "info";
            case AlertLevel::Warning: return "warning";
            case AlertLevel::Error: return "error";
            case AlertLevel::Critical: return "critical";
        }
        return "unknown";
    }
    
    bool sendHttpRequest(const QString& urlStr, const QString& method, 
                        const QVariantMap& headers, const QString& body, int timeoutMs) {
        QUrl url(urlStr);
        if (!url.isValid()) {
            Logger::error("WebhookChannel", QString("无效的URL: %1").arg(urlStr));
            return false;
        }
        
        QNetworkRequest request(url);
        
        // 设置请求头
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
        }
        
        // 发送请求
        QNetworkReply* reply = nullptr;
        if (method == "GET") {
            reply = networkManager->get(request);
        } else if (method == "POST") {
            reply = networkManager->post(request, body.toUtf8());
        } else if (method == "PUT") {
            reply = networkManager->put(request, body.toUtf8());
        } else {
            Logger::error("WebhookChannel", QString("不支持的HTTP方法: %1").arg(method));
            return false;
        }
        
        if (!reply) {
            return false;
        }
        
        // 等待响应（带超时）
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        timer.setInterval(timeoutMs);
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        
        timer.start();
        loop.exec();
        
        bool success = false;
        if (reply->error() == QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            success = (statusCode >= 200 && statusCode < 300);
        } else {
            Logger::warning("WebhookChannel", QString("HTTP请求失败: %1").arg(reply->errorString()));
        }
        
        reply->deleteLater();
        return success;
    }
};

NotificationChannel* WebhookChannelFactory::create(QObject* parent)
{
    return new WebhookChannelImpl(parent);
}

} // namespace Core
} // namespace Eagle
