#ifndef DIAGNOSTICMANAGER_P_H
#define DIAGNOSTICMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include "eagle/core/DiagnosticManager.h"

namespace Eagle {
namespace Core {

class DiagnosticManager::Private {
public:
    bool enabled;
    bool deadlockDetectionEnabled;
    int maxStackTraces;
    int maxMemorySnapshots;
    QMap<QString, StackTrace> stackTraces;  // traceId -> StackTrace
    QMap<QString, MemorySnapshot> memorySnapshots;  // snapshotId -> MemorySnapshot
    QList<DeadlockInfo> deadlocks;  // 死锁列表
    QTimer* deadlockDetectionTimer;
    mutable QMutex mutex;
    
    Private()
        : enabled(true)
        , deadlockDetectionEnabled(false)
        , maxStackTraces(100)
        , maxMemorySnapshots(50)
    {
        deadlockDetectionTimer = new QTimer();
        deadlockDetectionTimer->setSingleShot(false);
        deadlockDetectionTimer->setInterval(5000);  // 5秒检测一次
    }
    
    ~Private() {
        if (deadlockDetectionTimer) {
            deadlockDetectionTimer->stop();
            deadlockDetectionTimer->deleteLater();
        }
    }
};

} // namespace Core
} // namespace Eagle

#endif // DIAGNOSTICMANAGER_P_H
