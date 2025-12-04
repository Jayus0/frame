#ifndef HOTRELOADMANAGER_P_H
#define HOTRELOADMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include "eagle/core/HotReloadManager.h"

namespace Eagle {
namespace Core {

class PluginManager;

class HotReloadManager::Private {
public:
    PluginManager* pluginManager;
    bool enabled;
    bool autoSaveState;
    QString stateStoragePath;
    QMap<QString, PluginStateSnapshot> pluginStates;  // pluginId -> snapshot
    QMap<QString, HotReloadStatus> pluginStatuses;    // pluginId -> status
    mutable QMutex mutex;
    
    Private(PluginManager* pm)
        : pluginManager(pm)
        , enabled(true)
        , autoSaveState(true)
    {
        stateStoragePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/hotreload";
    }
};

} // namespace Core
} // namespace Eagle

#endif // HOTRELOADMANAGER_P_H
