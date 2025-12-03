#ifndef CONFIGMANAGER_P_H
#define CONFIGMANAGER_P_H

#include <QtCore/QVariantMap>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QByteArray>

namespace Eagle {
namespace Core {

class ConfigManagerPrivate {
public:
    QVariantMap globalConfig;
    QVariantMap userConfig;
    QMap<QString, QVariantMap> pluginConfigs;
    QMutex mutex;
    QMap<QString, QList<QPair<QObject*, QByteArray>>> watchers;
};

} // namespace Core
} // namespace Eagle

#endif // CONFIGMANAGER_P_H
