#ifndef PLUGINMANAGER_P_H
#define PLUGINMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QPluginLoader>
#include <QtCore/QMutex>
#include "eagle/core/IPlugin.h"

namespace Eagle {
namespace Core {

class PluginManagerPrivate {
public:
    QStringList pluginPaths;
    QMap<QString, QPluginLoader*> loaders;
    QMap<QString, IPlugin*> plugins;
    QMap<QString, PluginMetadata> metadata;
    bool signatureRequired = false;
    QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // PLUGINMANAGER_P_H
