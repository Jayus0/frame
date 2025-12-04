#include "eagle/core/RBAC.h"
#include "RBAC_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QVariantMap>
#include <QtCore/QVariantList>
#include <QtCore/QDateTime>
#include <algorithm>

namespace Eagle {
namespace Core {

// Role实现
bool Role::hasPermission(const QString& permission) const
{
    if (permissions.contains(permission)) {
        return true;
    }
    
    // 检查父角色的权限（递归）
    // 注意：这里需要RBACManager来获取父角色，简化实现只检查直接父角色
    return false;  // 简化实现
}

QSet<QString> Role::getAllPermissions() const
{
    QSet<QString> allPerms = permissions;
    // 这里应该递归获取父角色的权限，简化实现
    return allPerms;
}

// User实现
bool User::hasPermission(const QString& permission, const QMap<QString, Role>& allRoles) const
{
    if (!enabled) {
        return false;
    }
    
    // 检查直接权限
    if (directPermissions.contains(permission)) {
        return true;
    }
    
    // 检查角色权限
    for (const QString& roleName : roles) {
        if (allRoles.contains(roleName)) {
            const Role& role = allRoles[roleName];
            if (role.permissions.contains(permission)) {
                return true;
            }
            // 检查继承的权限
            for (const QString& parentRoleName : role.parentRoles) {
                if (allRoles.contains(parentRoleName)) {
                    const Role& parentRole = allRoles[parentRoleName];
                    if (parentRole.permissions.contains(permission)) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

QSet<QString> User::getAllPermissions(const QMap<QString, Role>& allRoles) const
{
    QSet<QString> allPerms = directPermissions;
    
    for (const QString& roleName : roles) {
        if (allRoles.contains(roleName)) {
            const Role& role = allRoles[roleName];
            allPerms.unite(role.permissions);
            
            // 添加继承的权限
            for (const QString& parentRoleName : role.parentRoles) {
                if (allRoles.contains(parentRoleName)) {
                    const Role& parentRole = allRoles[parentRoleName];
                    allPerms.unite(parentRole.permissions);
                }
            }
        }
    }
    
    return allPerms;
}

RBACManager::RBACManager(QObject* parent)
    : QObject(parent)
    , d(new RBACManager::Private)
{
    // 初始化默认权限
    addPermission(Permission("plugin.load", "加载插件", "plugin", "load"));
    addPermission(Permission("plugin.unload", "卸载插件", "plugin", "unload"));
    addPermission(Permission("plugin.reload", "重载插件", "plugin", "reload"));
    addPermission(Permission("config.read", "读取配置", "config", "read"));
    addPermission(Permission("config.write", "写入配置", "config", "write"));
    addPermission(Permission("service.call", "调用服务", "service", "call"));
    
    // 初始化默认角色
    Role adminRole("admin", "管理员");
    adminRole.permissions.insert("plugin.load");
    adminRole.permissions.insert("plugin.unload");
    adminRole.permissions.insert("plugin.reload");
    adminRole.permissions.insert("config.read");
    adminRole.permissions.insert("config.write");
    adminRole.permissions.insert("service.call");
    addRole(adminRole);
    
    Role userRole("user", "普通用户");
    userRole.permissions.insert("plugin.load");
    userRole.permissions.insert("config.read");
    userRole.permissions.insert("service.call");
    addRole(userRole);
    
    Logger::info("RBACManager", "RBAC管理器初始化完成");
}

RBACManager::~RBACManager()
{
    delete d;
    d = nullptr;
}

bool RBACManager::addPermission(const Permission& permission)
{
    if (!permission.isValid()) {
        Logger::error("RBACManager", "无效的权限");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (d->permissions.contains(permission.name)) {
        Logger::warning("RBACManager", QString("权限已存在: %1").arg(permission.name));
        return false;
    }
    
    d->permissions[permission.name] = permission;
    Logger::info("RBACManager", QString("添加权限: %1").arg(permission.name));
    
    // 清除相关缓存（所有包含此权限的缓存）
    locker.unlock();
    clearPermissionCache(permission.name);
    
    emit permissionAdded(permission.name);
    return true;
}

bool RBACManager::removePermission(const QString& permissionName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->permissions.contains(permissionName)) {
        return false;
    }
    
    // 从所有角色中移除该权限
    for (auto& role : d->roles) {
        role.permissions.remove(permissionName);
    }
    
    // 从所有用户中移除该权限
    for (auto& user : d->users) {
        user.directPermissions.remove(permissionName);
    }
    
    d->permissions.remove(permissionName);
    Logger::info("RBACManager", QString("移除权限: %1").arg(permissionName));
    
    // 清除相关缓存
    locker.unlock();
    clearPermissionCache(permissionName);
    
    emit permissionRemoved(permissionName);
    return true;
}

Permission RBACManager::getPermission(const QString& permissionName) const
{
    QMutexLocker locker(&d->mutex);
    return d->permissions.value(permissionName);
}

QStringList RBACManager::getAllPermissions() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->permissions.keys();
}

bool RBACManager::addRole(const Role& role)
{
    if (!role.isValid()) {
        Logger::error("RBACManager", "无效的角色");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (d->roles.contains(role.name)) {
        Logger::warning("RBACManager", QString("角色已存在: %1").arg(role.name));
        return false;
    }
    
    // 验证权限是否存在
    for (const QString& permName : role.permissions) {
        if (!d->permissions.contains(permName)) {
            Logger::warning("RBACManager", QString("角色 %1 包含不存在的权限: %2").arg(role.name, permName));
        }
    }
    
    d->roles[role.name] = role;
    Logger::info("RBACManager", QString("添加角色: %1").arg(role.name));
    
    // 清除所有缓存（角色变更可能影响所有用户）
    locker.unlock();
    clearCache();
    
    emit roleAdded(role.name);
    return true;
}

bool RBACManager::removeRole(const QString& roleName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->roles.contains(roleName)) {
        return false;
    }
    
    // 从所有用户中移除该角色
    for (auto& user : d->users) {
        user.roles.remove(roleName);
    }
    
    d->roles.remove(roleName);
    Logger::info("RBACManager", QString("移除角色: %1").arg(roleName));
    
    // 清除所有缓存（角色变更可能影响所有用户）
    locker.unlock();
    clearCache();
    
    emit roleRemoved(roleName);
    return true;
}

bool RBACManager::assignPermissionToRole(const QString& roleName, const QString& permissionName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->roles.contains(roleName)) {
        Logger::error("RBACManager", QString("角色不存在: %1").arg(roleName));
        return false;
    }
    
    if (!d->permissions.contains(permissionName)) {
        Logger::error("RBACManager", QString("权限不存在: %1").arg(permissionName));
        return false;
    }
    
    d->roles[roleName].permissions.insert(permissionName);
    Logger::info("RBACManager", QString("为角色 %1 分配权限: %2").arg(roleName, permissionName));
    
    // 清除相关缓存
    locker.unlock();
    clearPermissionCache(permissionName);
    
    return true;
}

bool RBACManager::revokePermissionFromRole(const QString& roleName, const QString& permissionName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->roles.contains(roleName)) {
        return false;
    }
    
    d->roles[roleName].permissions.remove(permissionName);
    Logger::info("RBACManager", QString("从角色 %1 撤销权限: %2").arg(roleName, permissionName));
    
    // 清除相关缓存
    locker.unlock();
    clearPermissionCache(permissionName);
    
    return true;
}

bool RBACManager::addRoleInheritance(const QString& childRole, const QString& parentRole)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->roles.contains(childRole) || !d->roles.contains(parentRole)) {
        Logger::error("RBACManager", "角色不存在");
        return false;
    }
    
    // 检查循环继承
    if (childRole == parentRole) {
        Logger::error("RBACManager", "不能自己继承自己");
        return false;
    }
    
    d->roles[childRole].parentRoles.insert(parentRole);
    Logger::info("RBACManager", QString("角色 %1 继承角色 %2").arg(childRole, parentRole));
    
    // 清除所有缓存（角色继承变更可能影响所有用户）
    locker.unlock();
    clearCache();
    
    return true;
}

Role RBACManager::getRole(const QString& roleName) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->roles.value(roleName);
}

QStringList RBACManager::getAllRoles() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->roles.keys();
}

bool RBACManager::addUser(const User& user)
{
    if (!user.isValid()) {
        Logger::error("RBACManager", "无效的用户");
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (d->users.contains(user.userId)) {
        Logger::warning("RBACManager", QString("用户已存在: %1").arg(user.userId));
        return false;
    }
    
    // 验证角色是否存在
    for (const QString& roleName : user.roles) {
        if (!d->roles.contains(roleName)) {
            Logger::warning("RBACManager", QString("用户 %1 包含不存在的角色: %2").arg(user.userId, roleName));
        }
    }
    
    d->users[user.userId] = user;
    Logger::info("RBACManager", QString("添加用户: %1").arg(user.userId));
    
    // 清除该用户的缓存（如果有）
    locker.unlock();
    clearUserCache(user.userId);
    
    emit userAdded(user.userId);
    return true;
}

bool RBACManager::removeUser(const QString& userId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->users.contains(userId)) {
        return false;
    }
    
    d->users.remove(userId);
    Logger::info("RBACManager", QString("移除用户: %1").arg(userId));
    
    // 清除该用户的缓存
    locker.unlock();
    clearUserCache(userId);
    
    emit userRemoved(userId);
    return true;
}

bool RBACManager::assignRoleToUser(const QString& userId, const QString& roleName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->users.contains(userId)) {
        Logger::error("RBACManager", QString("用户不存在: %1").arg(userId));
        return false;
    }
    
    if (!d->roles.contains(roleName)) {
        Logger::error("RBACManager", QString("角色不存在: %1").arg(roleName));
        return false;
    }
    
    d->users[userId].roles.insert(roleName);
    Logger::info("RBACManager", QString("为用户 %1 分配角色: %2").arg(userId, roleName));
    
    // 清除该用户的缓存
    locker.unlock();
    clearUserCache(userId);
    
    return true;
}

bool RBACManager::revokeRoleFromUser(const QString& userId, const QString& roleName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->users.contains(userId)) {
        return false;
    }
    
    d->users[userId].roles.remove(roleName);
    Logger::info("RBACManager", QString("从用户 %1 撤销角色: %2").arg(userId, roleName));
    
    // 清除该用户的缓存
    locker.unlock();
    clearUserCache(userId);
    
    return true;
}

bool RBACManager::assignPermissionToUser(const QString& userId, const QString& permissionName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->users.contains(userId)) {
        Logger::error("RBACManager", QString("用户不存在: %1").arg(userId));
        return false;
    }
    
    if (!d->permissions.contains(permissionName)) {
        Logger::error("RBACManager", QString("权限不存在: %1").arg(permissionName));
        return false;
    }
    
    d->users[userId].directPermissions.insert(permissionName);
    Logger::info("RBACManager", QString("为用户 %1 分配权限: %2").arg(userId, permissionName));
    
    // 清除该用户的缓存
    locker.unlock();
    clearUserCache(userId);
    
    emit permissionGranted(userId, permissionName);
    return true;
}

bool RBACManager::revokePermissionFromUser(const QString& userId, const QString& permissionName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    if (!d->users.contains(userId)) {
        return false;
    }
    
    d->users[userId].directPermissions.remove(permissionName);
    Logger::info("RBACManager", QString("从用户 %1 撤销权限: %2").arg(userId, permissionName));
    
    // 清除该用户的缓存
    locker.unlock();
    clearUserCache(userId);
    
    emit permissionRevoked(userId, permissionName);
    return true;
}

User RBACManager::getUser(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->users.value(userId);
}

QStringList RBACManager::getAllUsers() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->users.keys();
}

bool RBACManager::checkPermission(const QString& userId, const QString& permissionName) const
{
    const auto* d = d_func();
    
    // 检查缓存
    if (d->cacheEnabled) {
        QString cacheKey = QString("%1:%2").arg(userId, permissionName);
        QMutexLocker cacheLocker(&d->cacheMutex);
        
        if (d->permissionCache.contains(cacheKey)) {
            PermissionCacheEntry entry = d->permissionCache[cacheKey];
            
            // 检查是否过期
            if (!entry.isExpired()) {
                entry.updateAccessTime();
                // 更新缓存中的访问时间
                d->permissionCache[cacheKey] = entry;
                return entry.result;
            } else {
                // 过期，移除
                d->permissionCache.remove(cacheKey);
            }
        }
        cacheLocker.unlock();
    }
    
    // 执行实际权限检查
    QMutexLocker locker(&d->mutex);
    
    if (!d->users.contains(userId)) {
        Logger::warning("RBACManager", QString("用户不存在: %1").arg(userId));
        bool result = false;
        
        // 缓存结果（即使是false也缓存）
        if (d->cacheEnabled) {
            cachePermissionResult(userId, permissionName, result);
        }
        return result;
    }
    
    const User& user = d->users[userId];
    bool result = user.hasPermission(permissionName, d->roles);
    
    // 缓存结果
    if (d->cacheEnabled) {
        locker.unlock();  // 释放主锁，避免死锁
        cachePermissionResult(userId, permissionName, result);
    }
    
    return result;
}

bool RBACManager::checkAnyPermission(const QString& userId, const QStringList& permissionNames) const
{
    for (const QString& perm : permissionNames) {
        if (checkPermission(userId, perm)) {
            return true;
        }
    }
    return false;
}

bool RBACManager::checkAllPermissions(const QString& userId, const QStringList& permissionNames) const
{
    for (const QString& perm : permissionNames) {
        if (!checkPermission(userId, perm)) {
            return false;
        }
    }
    return true;
}

bool RBACManager::loadFromConfig(const QVariantMap& config)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 加载权限
    QVariantMap perms = config["permissions"].toMap();
    for (auto it = perms.begin(); it != perms.end(); ++it) {
        QVariantMap permData = it.value().toMap();
        Permission perm(it.key(), 
                       permData["description"].toString(),
                       permData["resource"].toString(),
                       permData["action"].toString());
        d->permissions[perm.name] = perm;
    }
    
    // 加载角色
    QVariantMap rolesData = config["roles"].toMap();
    for (auto it = rolesData.begin(); it != rolesData.end(); ++it) {
        QVariantMap roleData = it.value().toMap();
        Role role(it.key(), roleData["description"].toString());
        QStringList permsList = roleData["permissions"].toStringList();
        role.permissions = QSet<QString>(permsList.begin(), permsList.end());
        QStringList parentList = roleData["parent_roles"].toStringList();
        role.parentRoles = QSet<QString>(parentList.begin(), parentList.end());
        d->roles[role.name] = role;
    }
    
    // 加载用户
    QVariantMap usersData = config["users"].toMap();
    for (auto it = usersData.begin(); it != usersData.end(); ++it) {
        QVariantMap userData = it.value().toMap();
        User user(it.key(), userData["username"].toString());
        QStringList rolesList = userData["roles"].toStringList();
        user.roles = QSet<QString>(rolesList.begin(), rolesList.end());
        QStringList permsList = userData["permissions"].toStringList();
        user.directPermissions = QSet<QString>(permsList.begin(), permsList.end());
        user.enabled = userData.contains("enabled") ? userData["enabled"].toBool() : true;
        d->users[user.userId] = user;
    }
    
    Logger::info("RBACManager", "从配置加载RBAC数据完成");
    return true;
}

QVariantMap RBACManager::saveToConfig() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QVariantMap config;
    
    // 保存权限
    QVariantMap perms;
    for (auto it = d->permissions.begin(); it != d->permissions.end(); ++it) {
        QVariantMap permData;
        permData["description"] = it.value().description;
        permData["resource"] = it.value().resource;
        permData["action"] = it.value().action;
        perms[it.key()] = permData;
    }
    config["permissions"] = perms;
    
    // 保存角色
    QVariantMap rolesData;
    for (auto it = d->roles.begin(); it != d->roles.end(); ++it) {
        QVariantMap roleData;
        roleData["description"] = it.value().description;
        roleData["permissions"] = QStringList(it.value().permissions.values());
        roleData["parent_roles"] = QStringList(it.value().parentRoles.values());
        rolesData[it.key()] = roleData;
    }
    config["roles"] = rolesData;
    
    // 保存用户
    QVariantMap usersData;
    for (auto it = d->users.begin(); it != d->users.end(); ++it) {
        QVariantMap userData;
        userData["username"] = it.value().username;
        userData["roles"] = QStringList(it.value().roles.values());
        userData["permissions"] = QStringList(it.value().directPermissions.values());
        userData["enabled"] = it.value().enabled;
        usersData[it.key()] = userData;
    }
    config["users"] = usersData;
    
    return config;
}

// 权限缓存管理
void RBACManager::setCacheEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    d->cacheEnabled = enabled;
    if (!enabled) {
        d->permissionCache.clear();
    }
    Logger::info("RBACManager", QString("权限缓存%1").arg(enabled ? "启用" : "禁用"));
}

bool RBACManager::isCacheEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    return d->cacheEnabled;
}

void RBACManager::setCacheMaxSize(int maxSize)
{
    auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    d->cacheMaxSize = qMax(1, maxSize);
    
    // 如果当前缓存超过最大大小，执行LRU淘汰
    if (d->permissionCache.size() > d->cacheMaxSize) {
        evictLRUEntries();
    }
    
    Logger::info("RBACManager", QString("设置缓存最大大小: %1").arg(d->cacheMaxSize));
}

int RBACManager::getCacheMaxSize() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    return d->cacheMaxSize;
}

void RBACManager::setCacheTTL(int seconds)
{
    auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    d->cacheTTLSeconds = qMax(1, seconds);
    Logger::info("RBACManager", QString("设置缓存TTL: %1秒").arg(d->cacheTTLSeconds));
}

int RBACManager::getCacheTTL() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    return d->cacheTTLSeconds;
}

void RBACManager::clearCache()
{
    auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    int size = d->permissionCache.size();
    d->permissionCache.clear();
    Logger::info("RBACManager", QString("清除权限缓存，共%1条记录").arg(size));
}

void RBACManager::clearUserCache(const QString& userId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->cacheMutex);
    
    QString prefix = userId + ":";
    QList<QString> keysToRemove;
    
    for (auto it = d->permissionCache.begin(); it != d->permissionCache.end(); ++it) {
        if (it.key().startsWith(prefix)) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString& key : keysToRemove) {
        d->permissionCache.remove(key);
    }
    
    if (!keysToRemove.isEmpty()) {
        Logger::info("RBACManager", QString("清除用户 %1 的缓存，共%2条记录").arg(userId).arg(keysToRemove.size()));
    }
}

int RBACManager::getCacheSize() const
{
    auto* d = const_cast<Private*>(d_func());
    QMutexLocker locker(&d->cacheMutex);
    
    // 清理过期条目
    QList<QString> expiredKeys;
    for (auto it = d->permissionCache.begin(); it != d->permissionCache.end(); ++it) {
        if (it.value().isExpired()) {
            expiredKeys.append(it.key());
        }
    }
    for (const QString& key : expiredKeys) {
        d->permissionCache.remove(key);
    }
    
    return d->permissionCache.size();
}

// 私有辅助函数：缓存权限检查结果
void RBACManager::cachePermissionResult(const QString& userId, const QString& permissionName, bool result) const
{
    auto* d = const_cast<Private*>(d_func());
    QMutexLocker locker(&d->cacheMutex);
    
    if (!d->cacheEnabled) {
        return;
    }
    
    QString cacheKey = QString("%1:%2").arg(userId, permissionName);
    QDateTime expireTime = QDateTime::currentDateTime().addSecs(d->cacheTTLSeconds);
    
    // 如果缓存已满，执行LRU淘汰
    if (d->permissionCache.size() >= d->cacheMaxSize && !d->permissionCache.contains(cacheKey)) {
        evictLRUEntries();
    }
    
    // 添加或更新缓存
    d->permissionCache[cacheKey] = PermissionCacheEntry(result, expireTime);
}

// 私有辅助函数：LRU淘汰
void RBACManager::evictLRUEntries() const
{
    auto* d = const_cast<Private*>(d_func());
    
    if (d->permissionCache.size() <= d->cacheMaxSize) {
        return;
    }
    
    // 需要淘汰的数量
    int toEvict = d->permissionCache.size() - d->cacheMaxSize + 1;
    
    // 按访问时间排序，淘汰最久未访问的
    QList<QPair<QDateTime, QString>> entries;
    for (auto it = d->permissionCache.begin(); it != d->permissionCache.end(); ++it) {
        entries.append(qMakePair(it.value().accessTime, it.key()));
    }
    
    // 按访问时间升序排序（最早的在前）
    std::sort(entries.begin(), entries.end(), 
              [](const QPair<QDateTime, QString>& a, const QPair<QDateTime, QString>& b) {
                  return a.first < b.first;
              });
    
    // 淘汰最久未访问的条目
    for (int i = 0; i < toEvict && i < entries.size(); ++i) {
        d->permissionCache.remove(entries[i].second);
    }
}

// 私有辅助函数：清除特定权限的缓存
void RBACManager::clearPermissionCache(const QString& permissionName) const
{
    auto* d = const_cast<Private*>(d_func());
    QMutexLocker locker(&d->cacheMutex);
    
    QString suffix = ":" + permissionName;
    QList<QString> keysToRemove;
    
    for (auto it = d->permissionCache.begin(); it != d->permissionCache.end(); ++it) {
        if (it.key().endsWith(suffix)) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString& key : keysToRemove) {
        d->permissionCache.remove(key);
    }
    
    if (!keysToRemove.isEmpty()) {
        Logger::debug("RBACManager", QString("清除权限 %1 的缓存，共%2条记录").arg(permissionName).arg(keysToRemove.size()));
    }
}

} // namespace Core
} // namespace Eagle
