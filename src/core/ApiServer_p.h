#ifndef EAGLE_CORE_APISERVER_P_H
#define EAGLE_CORE_APISERVER_P_H

#include "eagle/core/ApiServer.h"
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QMutex>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <functional>

namespace Eagle {
namespace Core {

struct Route {
    QString method;              // GET, POST, DELETE等
    QString pattern;             // 路由模式（如 /api/v1/plugins/{id}）
    RequestHandler handler;      // 处理器函数
};

class ApiServerPrivate {
public:
    ApiServerPrivate(ApiServer* qq)
        : q(qq)
        , tcpServer(nullptr)
        , serverPort(8080)
        , isServerRunning(false)
        , framework(nullptr)
    {
    }
    
    ~ApiServerPrivate() {
        if (tcpServer) {
            tcpServer->close();
            delete tcpServer;
        }
    }
    
    ApiServer* q;
    QTcpServer* tcpServer;
    quint16 serverPort;
    bool isServerRunning;
    Framework* framework;
    
    // 路由表
    QList<Route> routes;
    QMutex routesMutex;
    
    // 中间件列表
    QList<Middleware> middlewares;
    QMutex middlewaresMutex;
    
    // 客户端连接管理
    QMap<QTcpSocket*, QByteArray> clientBuffers;
    QMutex clientsMutex;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_APISERVER_P_H
