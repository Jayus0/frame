#ifndef EAGLE_CORE_PERMISSIONCHANGENOTIFICATION_H
#define EAGLE_CORE_PERMISSIONCHANGENOTIFICATION_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>

namespace Eagle {
namespace Core {

/**
 * @brief 权限变更类型
 */
enum class PermissionChangeType {
    PermissionAdded,              // 权限添加
    PermissionRemoved,            // 权限删除
    RoleAdded,                    // 角色添加
    RoleRemoved,                  // 角色删除
    RolePermissionGranted,        // 角色权限授予
    RolePermissionRevoked,        // 角色权限撤销
    UserAdded,                    // 用户添加
    UserRemoved,                  // 用户删除
    UserRoleAssigned,             // 用户角色分配
    UserRoleRevoked,              // 用户角色撤销
    UserPermissionGranted,        // 用户权限授予
    UserPermissionRevoked,        // 用户权限撤销
    RoleInheritanceAdded,         // 角色继承添加
    RoleInheritanceRemoved        // 角色继承删除
};

/**
 * @brief 权限变更通知
 */
struct PermissionChangeNotification {
    PermissionChangeType changeType;   // 变更类型
    QString targetId;                 // 目标ID（用户ID、角色名、权限名）
    QString targetType;                // 目标类型（user、role、permission）
    QString relatedId;                 // 相关ID（如角色名、权限名）
    QString relatedType;               // 相关类型
    QString operatorId;                // 操作者ID
    QDateTime timestamp;               // 变更时间
    QVariantMap metadata;              // 额外元数据
    
    PermissionChangeNotification()
        : changeType(PermissionChangeType::PermissionAdded)
    {
        timestamp = QDateTime::currentDateTime();
    }
    
    /**
     * @brief 转换为JSON
     */
    QVariantMap toJson() const;
    
    /**
     * @brief 从JSON创建
     */
    static PermissionChangeNotification fromJson(const QVariantMap& json);
    
    /**
     * @brief 获取变更描述
     */
    QString getDescription() const;
};

/**
 * @brief 权限变更通知管理器
 */
class PermissionChangeNotifier {
public:
    /**
     * @brief 创建权限变更通知
     */
    static PermissionChangeNotification createNotification(
        PermissionChangeType type,
        const QString& targetId,
        const QString& targetType,
        const QString& relatedId = QString(),
        const QString& relatedType = QString(),
        const QString& operatorId = QString(),
        const QVariantMap& metadata = QVariantMap());
    
    /**
     * @brief 发布权限变更事件到EventBus
     */
    static void publishToEventBus(class EventBus* eventBus, 
                                  const PermissionChangeNotification& notification);
    
    /**
     * @brief 获取事件名称
     */
    static QString getEventName(PermissionChangeType type);
    
    /**
     * @brief 获取所有权限变更事件名称
     */
    static QStringList getAllEventNames();
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_PERMISSIONCHANGENOTIFICATION_H
