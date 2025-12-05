#ifndef EAGLE_CORE_APISERVER_H
#define EAGLE_CORE_APISERVER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QAbstractSocket>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <functional>
#include "SslConfig.h"

namespace Eagle {
namespace Core {

// 前向声明（必须在ApiServer类定义之前）
class ApiServerPrivate;

class Framework;
class HttpRequest;
class HttpResponse;

/**
 * @brief HTTP请求处理器函数类型
 */
typedef std::function<void(const HttpRequest&, HttpResponse&)> RequestHandler;

/**
 * @brief HTTP中间件函数类型
 */
typedef std::function<bool(const HttpRequest&, HttpResponse&)> Middleware;

/**
 * @brief HTTP请求类
 */
class HttpRequest {
public:
    QString method;              // GET, POST, DELETE等
    QString path;                 // 请求路径
    QString version;              // HTTP版本
    QMap<QString, QString> headers;  // 请求头
    QByteArray body;              // 请求体
    QMap<QString, QString> queryParams;  // 查询参数
    QMap<QString, QString> pathParams;    // 路径参数（如{id}）
    QString remoteAddress;        // 客户端IP地址
    
    // 解析请求
    static HttpRequest parse(const QByteArray& rawRequest, const QString& remoteAddress = QString());
    
    // 获取JSON body
    QJsonObject jsonBody() const;
    
    // 获取header值
    QString header(const QString& name) const;
    
    // 获取认证token
    QString getAuthToken() const;
};

/**
 * @brief HTTP响应类
 */
class HttpResponse {
public:
    int statusCode;               // 状态码
    QMap<QString, QString> headers;  // 响应头
    QByteArray body;              // 响应体
    
    HttpResponse();
    
    // 设置JSON响应
    void setJson(const QJsonObject& json);
    void setJson(const QJsonArray& json);
    
    // 设置错误响应
    void setError(int code, const QString& message, const QString& details = QString());
    
    // 设置成功响应
    void setSuccess(const QJsonObject& data = QJsonObject());
    
    // 转换为HTTP响应字符串
    QByteArray toHttpResponse() const;
    
    // 设置header
    void setHeader(const QString& name, const QString& value);
};

/**
 * @brief REST API服务器
 * 
 * 提供HTTP服务器功能，支持路由、中间件、认证等
 */
class ApiServer : public QObject {
    Q_OBJECT
    
public:
    explicit ApiServer(QObject* parent = nullptr);
    ~ApiServer();
    
    // 服务器控制
    bool start(quint16 port = 8080);
    bool startHttps(quint16 port = 8443, const SslConfig& sslConfig = SslConfig());
    void stop();
    bool isRunning() const;
    quint16 port() const;
    
    // SSL/TLS配置
    void setSslConfig(const SslConfig& config);
    SslConfig sslConfig() const;
    SslManager* sslManager() const;
    bool isHttpsEnabled() const;
    
    // 路由注册
    void get(const QString& path, RequestHandler handler);
    void post(const QString& path, RequestHandler handler);
    void put(const QString& path, RequestHandler handler);
    void delete_(const QString& path, RequestHandler handler);
    
    // 中间件
    void use(Middleware middleware);
    
    // 设置Framework实例（用于访问其他组件）
    void setFramework(Framework* framework);
    Framework* framework() const;
    
signals:
    void requestReceived(const QString& method, const QString& path);
    void requestCompleted(const QString& method, const QString& path, int statusCode);
    void serverStarted(quint16 port);
    void serverStopped();
    void error(const QString& error);
    
private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    
private:
    Q_DISABLE_COPY(ApiServer)
    
    // ApiServerPrivate 已在命名空间级别前向声明
    ApiServerPrivate* d;
    
    inline ApiServerPrivate* d_func() { return d; }
    inline const ApiServerPrivate* d_func() const { return d; }
    
    // 路由匹配
    bool matchRoute(const QString& routePattern, const QString& path, QMap<QString, QString>& params) const;
    
    // 处理请求
    void handleRequest(QAbstractSocket* socket, const HttpRequest& request);
    
    // 发送响应
    void sendResponse(QAbstractSocket* socket, const HttpResponse& response);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_APISERVER_H
