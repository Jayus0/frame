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
    
    // CPU使用率计算需要的历史数据
#ifdef _WIN32
    mutable QMap<QString, qint64> lastCpuTime;  // pluginId -> last CPU time (100-nanosecond intervals)
    mutable QMap<QString, QDateTime> lastCpuUpdate;  // pluginId -> last update time
#elif defined(__APPLE__)
    mutable QMap<QString, qint64> lastCpuTime;  // pluginId -> last CPU time (microseconds)
    mutable QMap<QString, QDateTime> lastCpuUpdate;  // pluginId -> last update time
#elif defined(__linux__)
    mutable QMap<QString, qint64> lastCpuTime;  // pluginId -> last CPU time (jiffies)
    mutable QMap<QString, QDateTime> lastCpuUpdate;  // pluginId -> last update time
#endif
    
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
