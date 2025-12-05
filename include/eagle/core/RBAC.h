#ifndef EAGLE_CORE_RBAC_H
#define EAGLE_CORE_RBAC_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include "PermissionChangeNotification.h"

// 前向声明
namespace Eagle {
namespace Core {
class EventBus;
}
}

namespace Eagle {
namespace Core {

/**
 * @brief 权限
 */
struct Permission {
    QString name;           // 权限名称，如 "plugin.load"
    QString description;    // 权限描述
    QString resource;       // 资源，如 "plugin"
    QString action;        // 操作，如 "load"
    
    Permission() = default;
    Permission(const QString& n, const QString& desc = QString(), 
              const QString& res = QString(), const QString& act = QString())
        : name(n), description(desc), resource(res), action(act) {}
    
    bool isValid() const {
        return !name.isEmpty();
    }
    
    bool operator==(const Permission& other) const {
        return name == other.name;
    }
};

/**
 * @brief 角色
 */
struct Role {
    QString name;                    // 角色名称
    QString description;              // 角色描述
    QSet<QString> permissions;       // 权限集合
    QSet<QString> parentRoles;       // 父角色（继承权限）
    
    Role() = default;
    Role(const QString& n, const QString& desc = QString())
        : name(n), description(desc) {}
    
    bool isValid() const {
        return !name.isEmpty();
    }
    
    // 检查是否有某个权限（包括继承的权限）
    bool hasPermission(const QString& permission) const;
    
    // 获取所有权限（包括继承的）
    QSet<QString> getAllPermissions() const;
};

/**
 * @brief 用户
 */
struct User {
    QString userId;                  // 用户ID
    QString username;                // 用户名
    QSet<QString> roles;             // 角色集合
    QSet<QString> directPermissions; // 直接分配的权限（不通过角色）
    bool enabled = true;              // 是否启用
    
    User() = default;
    User(const QString& id, const QString& name)
        : userId(id), username(name) {}
    
    bool isValid() const {
        return !userId.isEmpty() && !username.isEmpty();
    }
    
    // 检查用户是否有某个权限
    bool hasPermission(const QString& permission, const QMap<QString, Role>& allRoles) const;
    
    // 获取用户的所有权限
    QSet<QString> getAllPermissions(const QMap<QString, Role>& allRoles) const;
};

/**
 * @brief RBAC权限管理器
 */
class RBACManager : public QObject {
    Q_OBJECT
    
public:
    explicit RBACManager(QObject* parent = nullptr);
    ~RBACManager();
    
    // 权限管理
    bool addPermission(const Permission& permission);
    bool removePermission(const QString& permissionName);
    Permission getPermission(const QString& permissionName) const;
    QStringList getAllPermissions() const;
    
    // 角色管理
    bool addRole(const Role& role);
    bool removeRole(const QString& roleName);
    bool assignPermissionToRole(const QString& roleName, const QString& permissionName);
    bool revokePermissionFromRole(const QString& roleName, const QString& permissionName);
    bool addRoleInheritance(const QString& childRole, const QString& parentRole);
    Role getRole(const QString& roleName) const;
    QStringList getAllRoles() const;
    
    // 用户管理
    bool addUser(const User& user);
    bool removeUser(const QString& userId);
    bool assignRoleToUser(const QString& userId, const QString& roleName);
    bool revokeRoleFromUser(const QString& userId, const QString& roleName);
    bool assignPermissionToUser(const QString& userId, const QString& permissionName);
    bool revokePermissionFromUser(const QString& userId, const QString& permissionName);
    User getUser(const QString& userId) const;
    QStringList getAllUsers() const;
    
    // 权限检查
    bool checkPermission(const QString& userId, const QString& permissionName) const;
    bool checkAnyPermission(const QString& userId, const QStringList& permissionNames) const;
    bool checkAllPermissions(const QString& userId, const QStringList& permissionNames) const;
    
    // 权限缓存管理
    void setCacheEnabled(bool enabled);
    bool isCacheEnabled() const;
    void setCacheMaxSize(int maxSize);
    int getCacheMaxSize() const;
    void setCacheTTL(int seconds);
    int getCacheTTL() const;
    void clearCache();
    void clearUserCache(const QString& userId);
    int getCacheSize() const;
    
    // 批量操作
    bool loadFromConfig(const QVariantMap& config);
    QVariantMap saveToConfig() const;
    
    // 权限变更通知配置
    void setNotificationEnabled(bool enabled);
    bool isNotificationEnabled() const;
    void setEventBus(EventBus* eventBus);
    EventBus* eventBus() const;
    void setOperatorId(const QString& operatorId);  // 设置当前操作者ID（用于通知）
    QString operatorId() const;
    
signals:
    // 基础变更信号（保持向后兼容）
    void permissionAdded(const QString& permissionName);
    void permissionRemoved(const QString& permissionName);
    void roleAdded(const QString& roleName);
    void roleRemoved(const QString& roleName);
    void userAdded(const QString& userId);
    void userRemoved(const QString& userId);
    void permissionGranted(const QString& userId, const QString& permissionName);
    void permissionRevoked(const QString& userId, const QString& permissionName);
    
    // 权限变更通知信号（新增）
    void permissionChanged(const PermissionChangeNotification& notification);
    
private:
    Q_DISABLE_COPY(RBACManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    // 私有辅助函数
    void cachePermissionResult(const QString& userId, const QString& permissionName, bool result) const;
    void evictLRUEntries() const;
    void clearPermissionCache(const QString& permissionName) const;
    void notifyPermissionChange(const PermissionChangeNotification& notification) const;
    
    friend class Private;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::Permission)
Q_DECLARE_METATYPE(Eagle::Core::Role)
Q_DECLARE_METATYPE(Eagle::Core::User)

#endif // EAGLE_CORE_RBAC_H
