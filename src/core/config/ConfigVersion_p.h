#ifndef CONFIGVERSION_P_H
#define CONFIGVERSION_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QVariantMap>
#include "eagle/core/ConfigVersion.h"

namespace Eagle {
namespace Core {

class ConfigVersionManager::Private {
public:
    QMap<int, ConfigVersion> versions;  // version -> ConfigVersion
    int currentVersion;                  // 当前版本号
    QString storagePath;                 // 版本存储路径
    bool enabled;                        // 是否启用版本管理
    mutable QMutex mutex;                 // 线程安全锁
    
    Private()
        : currentVersion(0)
        , enabled(true)
    {}
};

} // namespace Core
} // namespace Eagle

#endif // CONFIGVERSION_P_H
