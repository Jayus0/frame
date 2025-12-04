#ifndef RBAC_P_H
#define RBAC_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include "eagle/core/RBAC.h"

namespace Eagle {
namespace Core {

/**
 * @brief 缓存项
 */
struct PermissionCacheEntry {
    bool result;                    // 权限检查结果
    QDateTime expireTime;           // 过期时间
    QDateTime accessTime;            // 最后访问时间（用于LRU）
    
    PermissionCacheEntry()
        : result(false)
    {
    }
    
    PermissionCacheEntry(bool res, const QDateTime& expire)
        : result(res), expireTime(expire), accessTime(QDateTime::currentDateTime())
    {
    }
    
    bool isExpired() const {
        return QDateTime::currentDateTime() > expireTime;
    }
    
    void updateAccessTime() {
        accessTime = QDateTime::currentDateTime();
    }
};

class RBACManager::Private {
public:
    QMap<QString, Permission> permissions;
    QMap<QString, Role> roles;
    QMap<QString, User> users;
    mutable QMutex mutex;
    
    // 权限缓存
    QMap<QString, PermissionCacheEntry> permissionCache;  // key = "userId:permissionName"
    bool cacheEnabled = true;                              // 是否启用缓存
    int cacheMaxSize = 1000;                               // 最大缓存条目数
    int cacheTTLSeconds = 300;                            // 缓存TTL（秒），默认5分钟
    mutable QMutex cacheMutex;                             // 缓存专用互斥锁
};

} // namespace Core
} // namespace Eagle

#endif // RBAC_P_H
