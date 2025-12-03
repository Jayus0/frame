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
    mutable QMutex mutex;  // mutable 允许在 const 函数中锁定
    QMap<QString, QList<QPair<QObject*, QByteArray>>> watchers;
    bool encryptionEnabled = false;  // 是否启用加密
    QStringList sensitiveKeys;  // 需要加密的键列表
    QString encryptionKey;  // 加密密钥
};

} // namespace Core
} // namespace Eagle

#endif // CONFIGMANAGER_P_H
