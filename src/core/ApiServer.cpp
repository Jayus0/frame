#include "eagle/core/ApiServer.h"
#include "ApiServer_p.h"
#include "eagle/core/Framework.h"
#include "eagle/core/Logger.h"
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonParseError>
#include <QtCore/QDateTime>
#include <QtNetwork/QHostAddress>

namespace Eagle {
namespace Core {

// ============================================================================
// HttpRequest 实现
// ============================================================================

HttpRequest HttpRequest::parse(const QByteArray& rawRequest, const QString& remoteAddress) {
    HttpRequest req;
    req.remoteAddress = remoteAddress;
    
    // 按行分割
    QList<QByteArray> lines = rawRequest.split('\n');
    if (lines.isEmpty()) {
        return req;
    }
    
    // 解析请求行
    QByteArray requestLine = lines[0].trimmed();
    QList<QByteArray> requestParts = requestLine.split(' ');
    if (requestParts.size() >= 3) {
        req.method = QString::fromUtf8(requestParts[0]).toUpper();
        QString fullPath = QString::fromUtf8(requestParts[1]);
        req.version = QString::fromUtf8(requestParts[2]);
        
        // 解析路径和查询参数
        int queryIndex = fullPath.indexOf('?');
        if (queryIndex >= 0) {
            req.path = fullPath.left(queryIndex);
            QString queryString = fullPath.mid(queryIndex + 1);
            QUrlQuery query(queryString);
            // QUrlQuery::queryItems() 返回 QList<QPair<QString, QString>>
            QList<QPair<QString, QString> > items = query.queryItems();
            for (const QPair<QString, QString>& item : items) {
                req.queryParams[item.first] = item.second;
            }
        } else {
            req.path = fullPath;
        }
    }
    
    // 解析请求头
    bool isBody = false;
    QByteArray bodyData;
    for (int i = 1; i < lines.size(); ++i) {
        QByteArray line = lines[i].trimmed();
        if (line.isEmpty()) {
            isBody = true;
            continue;
        }
        
        if (!isBody) {
            int colonIndex = line.indexOf(':');
            if (colonIndex > 0) {
                QString key = QString::fromUtf8(line.left(colonIndex)).trimmed();
                QString value = QString::fromUtf8(line.mid(colonIndex + 1)).trimmed();
                req.headers[key.toLower()] = value;
            }
        } else {
            bodyData.append(line);
            if (i < lines.size() - 1) {
                bodyData.append('\n');
            }
        }
    }
    
    req.body = bodyData;
    
    return req;
}

QJsonObject HttpRequest::jsonBody() const {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError) {
        return QJsonObject();
    }
    return doc.object();
}

QString HttpRequest::header(const QString& name) const {
    return headers.value(name.toLower());
}

QString HttpRequest::getAuthToken() const {
    // 优先从Authorization header获取
    QString authHeader = header("authorization");
    if (!authHeader.isEmpty()) {
        if (authHeader.startsWith("Bearer ", Qt::CaseInsensitive)) {
            return authHeader.mid(7);
        }
        if (authHeader.startsWith("ApiKey ", Qt::CaseInsensitive)) {
            return authHeader.mid(7);
        }
    }
    
    // 从查询参数获取
    if (queryParams.contains("api_key")) {
        return queryParams["api_key"];
    }
    
    return QString();
}

// ============================================================================
// HttpResponse 实现
// ============================================================================

HttpResponse::HttpResponse()
    : statusCode(200)
{
    headers["Content-Type"] = "application/json; charset=utf-8";
    headers["Server"] = "EagleFramework/1.0";
}

void HttpResponse::setJson(const QJsonObject& json) {
    QJsonDocument doc(json);
    body = doc.toJson(QJsonDocument::Compact);
    statusCode = 200;
}

void HttpResponse::setJson(const QJsonArray& json) {
    QJsonDocument doc(json);
    body = doc.toJson(QJsonDocument::Compact);
    statusCode = 200;
}

void HttpResponse::setError(int code, const QString& message, const QString& details) {
    statusCode = code;
    QJsonObject error;
    error["error"] = true;
    error["code"] = code;
    error["message"] = message;
    error["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!details.isEmpty()) {
        error["details"] = details;
    }
    QJsonDocument doc(error);
    body = doc.toJson(QJsonDocument::Compact);
}

void HttpResponse::setSuccess(const QJsonObject& data) {
    statusCode = 200;
    QJsonObject result;
    result["success"] = true;
    result["data"] = data;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonDocument doc(result);
    body = doc.toJson(QJsonDocument::Compact);
}

void HttpResponse::setHeader(const QString& name, const QString& value) {
    headers[name] = value;
}

QByteArray HttpResponse::toHttpResponse() const {
    QByteArray response;
    
    // 状态行
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 201: statusText = "Created"; break;
        case 400: statusText = "Bad Request"; break;
        case 401: statusText = "Unauthorized"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    
    // 响应头
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        response.append(QString("%1: %2\r\n").arg(it.key()).arg(it.value()).toUtf8());
    }
    
    // Content-Length
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    
    // 空行
    response.append("\r\n");
    
    // 响应体
    response.append(body);
    
    return response;
}

// ============================================================================
// ApiServer 实现
// ============================================================================

ApiServer::ApiServer(QObject* parent)
    : QObject(parent)
    , d(new ApiServerPrivate(this))
{
}

ApiServer::~ApiServer() {
    stop();
    delete d;
}

bool ApiServer::start(quint16 port) {
    if (d->isServerRunning) {
        Logger::warning("ApiServer", QString("服务器已在运行，端口: %1").arg(d->serverPort));
        return true;
    }
    
    if (!d->tcpServer) {
        d->tcpServer = new QTcpServer(this);
        connect(d->tcpServer, &QTcpServer::newConnection, this, &ApiServer::onNewConnection);
    }
    
    if (!d->tcpServer->listen(QHostAddress::Any, port)) {
        QString error = QString("无法启动HTTP服务器，端口: %1, 错误: %2")
                       .arg(port).arg(d->tcpServer->errorString());
        Logger::error("ApiServer", error);
        emit this->error(error);
        return false;
    }
    
    d->serverPort = port;
    d->isServerRunning = true;
    
    Logger::info("ApiServer", QString("HTTP服务器已启动，端口: %1").arg(port));
    emit serverStarted(port);
    
    return true;
}

void ApiServer::stop() {
    if (!d->isServerRunning) {
        return;
    }
    
    if (d->tcpServer) {
        d->tcpServer->close();
    }
    
    // 关闭所有客户端连接
    QMutexLocker locker(&d->clientsMutex);
    for (auto it = d->clientBuffers.begin(); it != d->clientBuffers.end(); ++it) {
        it.key()->close();
    }
    d->clientBuffers.clear();
    
    d->isServerRunning = false;
    
    Logger::info("ApiServer", "HTTP服务器已停止");
    emit serverStopped();
}

bool ApiServer::isRunning() const {
    return d->isServerRunning;
}

quint16 ApiServer::port() const {
    return d->serverPort;
}

void ApiServer::get(const QString& path, RequestHandler handler) {
    QMutexLocker locker(&d->routesMutex);
    Route route;
    route.method = "GET";
    route.pattern = path;
    route.handler = handler;
    d->routes.append(route);
}

void ApiServer::post(const QString& path, RequestHandler handler) {
    QMutexLocker locker(&d->routesMutex);
    Route route;
    route.method = "POST";
    route.pattern = path;
    route.handler = handler;
    d->routes.append(route);
}

void ApiServer::put(const QString& path, RequestHandler handler) {
    QMutexLocker locker(&d->routesMutex);
    Route route;
    route.method = "PUT";
    route.pattern = path;
    route.handler = handler;
    d->routes.append(route);
}

void ApiServer::delete_(const QString& path, RequestHandler handler) {
    QMutexLocker locker(&d->routesMutex);
    Route route;
    route.method = "DELETE";
    route.pattern = path;
    route.handler = handler;
    d->routes.append(route);
}

void ApiServer::use(Middleware middleware) {
    QMutexLocker locker(&d->middlewaresMutex);
    d->middlewares.append(middleware);
}

void ApiServer::setFramework(Framework* framework) {
    d->framework = framework;
}

Framework* ApiServer::framework() const {
    return d->framework;
}

void ApiServer::onNewConnection() {
    while (d->tcpServer->hasPendingConnections()) {
        QTcpSocket* client = d->tcpServer->nextPendingConnection();
        connect(client, &QTcpSocket::readyRead, this, &ApiServer::onClientReadyRead);
        connect(client, &QTcpSocket::disconnected, this, &ApiServer::onClientDisconnected);
        
        QMutexLocker locker(&d->clientsMutex);
        d->clientBuffers[client] = QByteArray();
    }
}

void ApiServer::onClientReadyRead() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QMutexLocker locker(&d->clientsMutex);
    QByteArray& buffer = d->clientBuffers[client];
    buffer.append(client->readAll());
    
    // 检查是否收到完整的HTTP请求（以\r\n\r\n结尾）
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return; // 数据不完整，等待更多数据
    }
    
    // 解析Content-Length
    int contentLength = 0;
    QByteArray headerPart = buffer.left(headerEnd);
    int contentLengthIndex = headerPart.indexOf("Content-Length:");
    if (contentLengthIndex >= 0) {
        QByteArray lengthLine = headerPart.mid(contentLengthIndex);
        int colonIndex = lengthLine.indexOf(':');
        int crIndex = lengthLine.indexOf("\r\n");
        if (colonIndex >= 0 && crIndex > colonIndex) {
            QByteArray lengthStr = lengthLine.mid(colonIndex + 1, crIndex - colonIndex - 1).trimmed();
            contentLength = lengthStr.toInt();
        }
    }
    
    // 检查是否有请求体
    int bodyStart = headerEnd + 4;
    int totalExpected = bodyStart + contentLength;
    
    if (buffer.size() < totalExpected) {
        return; // 数据不完整，等待更多数据
    }
    
    // 提取完整的请求
    QByteArray fullRequest = buffer.left(totalExpected);
    buffer.remove(0, totalExpected);
    
    locker.unlock();
    
    // 解析请求
    QString remoteAddress = client->peerAddress().toString();
    HttpRequest request = HttpRequest::parse(fullRequest, remoteAddress);
    
    // 处理请求
    handleRequest(client, request);
}

void ApiServer::onClientDisconnected() {
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }
    
    QMutexLocker locker(&d->clientsMutex);
    d->clientBuffers.remove(client);
    client->deleteLater();
}

bool ApiServer::matchRoute(const QString& routePattern, const QString& path, QMap<QString, QString>& params) const {
    // 简单的路由匹配，支持 {param} 格式
    QStringList patternParts = routePattern.split('/');
    QStringList pathParts = path.split('/');
    
    if (patternParts.size() != pathParts.size()) {
        return false;
    }
    
    for (int i = 0; i < patternParts.size(); ++i) {
        QString patternPart = patternParts[i];
        QString pathPart = pathParts[i];
        
        if (patternPart.startsWith('{') && patternPart.endsWith('}')) {
            // 参数匹配
            QString paramName = patternPart.mid(1, patternPart.length() - 2);
            params[paramName] = pathPart;
        } else if (patternPart != pathPart) {
            // 精确匹配失败
            return false;
        }
    }
    
    return true;
}

void ApiServer::handleRequest(QTcpSocket* socket, const HttpRequest& request) {
    emit requestReceived(request.method, request.path);
    
    HttpResponse response;
    
    // 执行中间件
    QMutexLocker middlewareLocker(&d->middlewaresMutex);
    for (const Middleware& middleware : d->middlewares) {
        if (!middleware(request, response)) {
            // 中间件返回false，停止处理
            sendResponse(socket, response);
            emit requestCompleted(request.method, request.path, response.statusCode);
            return;
        }
    }
    middlewareLocker.unlock();
    
    // 查找匹配的路由
    QMutexLocker routeLocker(&d->routesMutex);
    bool found = false;
    for (const Route& route : d->routes) {
        if (route.method != request.method) {
            continue;
        }
        
        QMap<QString, QString> pathParams;
        if (matchRoute(route.pattern, request.path, pathParams)) {
            routeLocker.unlock();
            
            // 复制请求并设置路径参数
            HttpRequest mutableRequest = request;
            mutableRequest.pathParams = pathParams;
            
            // 执行处理器
            try {
                route.handler(mutableRequest, response);
                found = true;
            } catch (const std::exception& e) {
                Logger::error("ApiServer", QString("处理请求时发生异常: %1").arg(e.what()));
                response.setError(500, "Internal Server Error", e.what());
                found = true;
            } catch (...) {
                Logger::error("ApiServer", "处理请求时发生未知异常");
                response.setError(500, "Internal Server Error", "Unknown exception");
                found = true;
            }
            break;
        }
    }
    routeLocker.unlock();
    
    if (!found) {
        response.setError(404, "Not Found", QString("路径未找到: %1").arg(request.path));
    }
    
    sendResponse(socket, response);
    emit requestCompleted(request.method, request.path, response.statusCode);
}

void ApiServer::sendResponse(QTcpSocket* socket, const HttpResponse& response) {
    QByteArray httpResponse = response.toHttpResponse();
    socket->write(httpResponse);
    socket->flush();
}

} // namespace Core
} // namespace Eagle
