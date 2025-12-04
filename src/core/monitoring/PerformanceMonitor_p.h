#ifndef PERFORMANCEMONITOR_P_H
#define PERFORMANCEMONITOR_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include "eagle/core/PerformanceMonitor.h"

namespace Eagle {
namespace Core {

class PerformanceMonitor::Private {
public:
    QMap<QString, PerformanceMetric> metrics;
    QMap<QString, qint64> pluginLoadTimes;  // pluginId -> loadTimeMs
    QMap<QString, QMap<QString, qint64>> serviceCallTimes;  // serviceName -> method -> callTimeMs
    QTimer* updateTimer;
    int updateIntervalMs = 1000;  // 1秒更新一次
    bool enabled = true;
    mutable QMutex mutex;
    
    // 系统指标缓存
    double cpuUsage = 0.0;
    qint64 memoryUsage = 0;
    QDateTime lastCpuUpdate;
    qint64 lastCpuTotal = 0;
    qint64 lastCpuIdle = 0;
};

} // namespace Core
} // namespace Eagle

#endif // PERFORMANCEMONITOR_P_H
