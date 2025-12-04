#ifndef BACKUPMANAGER_P_H
#define BACKUPMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include "eagle/core/BackupManager.h"
#include "eagle/core/ConfigManager.h"

namespace Eagle {
namespace Core {

class BackupManager::Private {
public:
    ConfigManager* configManager;
    BackupPolicy policy;
    QMap<QString, BackupInfo> backups;  // backupId -> BackupInfo
    QTimer* scheduleTimer;
    bool autoBackupEnabled;
    mutable QMutex mutex;
    
    // 上次备份的配置快照（用于增量备份）
    QVariantMap lastBackupSnapshot;
    QDateTime lastBackupTime;
    
    Private(ConfigManager* cm)
        : configManager(cm)
        , autoBackupEnabled(false)
    {
        scheduleTimer = new QTimer();
        scheduleTimer->setSingleShot(false);
    }
    
    ~Private() {
        if (scheduleTimer) {
            scheduleTimer->stop();
            scheduleTimer->deleteLater();
        }
    }
};

} // namespace Core
} // namespace Eagle

#endif // BACKUPMANAGER_P_H
