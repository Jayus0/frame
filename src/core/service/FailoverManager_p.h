#ifndef FAILOVERMANAGER_P_H
#define FAILOVERMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include "eagle/core/FailoverManager.h"

namespace Eagle {
namespace Core {

class ServiceRegistry;

class FailoverManager::Private {
public:
    ServiceRegistry* serviceRegistry;
    bool enabled;
    QMap<QString, FailoverConfig> serviceConfigs;  // serviceName -> config
    QMap<QString, QMap<QString, ServiceNode>> serviceNodes;  // serviceName -> (nodeId -> node)
    QMap<QString, QString> currentPrimaries;  // serviceName -> primaryNodeId
    QMap<QString, QList<FailoverEvent>> failoverHistory;  // serviceName -> events
    QMap<QString, bool> serviceEnabled;  // serviceName -> enabled
    QTimer* healthCheckTimer;
    QTimer* stateSyncTimer;
    mutable QMutex mutex;
    
    Private(ServiceRegistry* sr)
        : serviceRegistry(sr)
        , enabled(true)
    {
        healthCheckTimer = new QTimer();
        healthCheckTimer->setSingleShot(false);
        healthCheckTimer->setInterval(5000);  // 默认5秒
        
        stateSyncTimer = new QTimer();
        stateSyncTimer->setSingleShot(false);
        stateSyncTimer->setInterval(10000);  // 默认10秒
    }
    
    ~Private() {
        if (healthCheckTimer) {
            healthCheckTimer->stop();
            healthCheckTimer->deleteLater();
        }
        if (stateSyncTimer) {
            stateSyncTimer->stop();
            stateSyncTimer->deleteLater();
        }
    }
};

} // namespace Core
} // namespace Eagle

#endif // FAILOVERMANAGER_P_H
