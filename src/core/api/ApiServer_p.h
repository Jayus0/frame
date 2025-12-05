#ifndef EAGLE_CORE_APISERVER_P_H
#define EAGLE_CORE_APISERVER_P_H

#include "eagle/core/ApiServer.h"
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QSslSocket>
#include <QtCore/QMutex>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <functional>

namespace Eagle {
namespace Core {

// 前向声明
class ApiServer;

struct Route {
    QString method;              // GET, POST, DELETE等
    QString pattern;             // 路由模式（如 /api/v1/plugins/{id}）
    RequestHandler handler;      // 处理器函数
};

// 定义ApiServerPrivate类（非嵌套类，在ApiServer.h中前向声明）
class ApiServerPrivate {
public:
    ApiServerPrivate(ApiServer* qq)
        : q(qq)
        , tcpServer(nullptr)
        , serverPort(8080)
        , isServerRunning(false)
        , isHttpsEnabled(false)
        , framework(nullptr)
        , sslManager(nullptr)
    {
    }
    
    ~ApiServerPrivate() {
        if (tcpServer) {
            tcpServer->close();
            delete tcpServer;
        }
        if (sslManager) {
            delete sslManager;
        }
    }
    
    ApiServer* q;
    QTcpServer* tcpServer;  // 同时用于HTTP和HTTPS（HTTPS模式下使用QSslSocket）
    quint16 serverPort;
    bool isServerRunning;
    bool isHttpsEnabled;
    Framework* framework;
    SslManager* sslManager;
    
    // 路由表
    QList<Route> routes;
    QMutex routesMutex;
    
    // 中间件列表
    QList<Middleware> middlewares;
    QMutex middlewaresMutex;
    
    // 客户端连接管理
    QMap<QAbstractSocket*, QByteArray> clientBuffers;
    QMutex clientsMutex;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_APISERVER_P_H
