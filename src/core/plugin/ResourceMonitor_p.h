#ifndef RESOURCEMONITOR_P_H
#define RESOURCEMONITOR_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include "eagle/core/ResourceMonitor.h"

namespace Eagle {
namespace Core {

class ResourceMonitor::Private {
public:
    bool monitoringEnabled;
    bool enforcementEnabled;
    int monitoringIntervalMs;
    QMap<QString, ResourceLimits> pluginLimits;  // pluginId -> limits
    QMap<QString, ResourceUsage> pluginUsage;   // pluginId -> usage
    mutable QMap<QString, QList<ResourceLimitExceeded>> limitExceededEvents;  // pluginId -> events (mutable for const methods)
    QTimer* monitoringTimer;
    mutable QMutex mutex;
    
    Private()
        : monitoringEnabled(true)
        , enforcementEnabled(false)
        , monitoringIntervalMs(5000)  // 默认5秒
    {
        monitoringTimer = new QTimer();
        monitoringTimer->setSingleShot(false);
        monitoringTimer->setInterval(monitoringIntervalMs);
    }
    
    ~Private() {
        if (monitoringTimer) {
            monitoringTimer->stop();
            monitoringTimer->deleteLater();
        }
    }
};

} // namespace Core
} // namespace Eagle

#endif // RESOURCEMONITOR_P_H
