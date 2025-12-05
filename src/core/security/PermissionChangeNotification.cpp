#include "eagle/core/PermissionChangeNotification.h"
#include "eagle/core/EventBus.h"
#include "eagle/core/Logger.h"
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

QVariantMap PermissionChangeNotification::toJson() const
{
    QVariantMap json;
    json["changeType"] = static_cast<int>(changeType);
    json["targetId"] = targetId;
    json["targetType"] = targetType;
    json["relatedId"] = relatedId;
    json["relatedType"] = relatedType;
    json["operatorId"] = operatorId;
    json["timestamp"] = timestamp.toString(Qt::ISODate);
    json["metadata"] = metadata;
    return json;
}

PermissionChangeNotification PermissionChangeNotification::fromJson(const QVariantMap& json)
{
    PermissionChangeNotification notification;
    notification.changeType = static_cast<PermissionChangeType>(json.value("changeType").toInt());
    notification.targetId = json.value("targetId").toString();
    notification.targetType = json.value("targetType").toString();
    notification.relatedId = json.value("relatedId").toString();
    notification.relatedType = json.value("relatedType").toString();
    notification.operatorId = json.value("operatorId").toString();
    notification.timestamp = QDateTime::fromString(json.value("timestamp").toString(), Qt::ISODate);
    notification.metadata = json.value("metadata").toMap();
    return notification;
}

QString PermissionChangeNotification::getDescription() const
{
    QString desc;
    switch (changeType) {
    case PermissionChangeType::PermissionAdded:
        desc = QString("权限已添加: %1").arg(targetId);
        break;
    case PermissionChangeType::PermissionRemoved:
        desc = QString("权限已删除: %1").arg(targetId);
        break;
    case PermissionChangeType::RoleAdded:
        desc = QString("角色已添加: %1").arg(targetId);
        break;
    case PermissionChangeType::RoleRemoved:
        desc = QString("角色已删除: %1").arg(targetId);
        break;
    case PermissionChangeType::RolePermissionGranted:
        desc = QString("角色 %1 已授予权限: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::RolePermissionRevoked:
        desc = QString("角色 %1 已撤销权限: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::UserAdded:
        desc = QString("用户已添加: %1").arg(targetId);
        break;
    case PermissionChangeType::UserRemoved:
        desc = QString("用户已删除: %1").arg(targetId);
        break;
    case PermissionChangeType::UserRoleAssigned:
        desc = QString("用户 %1 已分配角色: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::UserRoleRevoked:
        desc = QString("用户 %1 已撤销角色: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::UserPermissionGranted:
        desc = QString("用户 %1 已授予权限: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::UserPermissionRevoked:
        desc = QString("用户 %1 已撤销权限: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::RoleInheritanceAdded:
        desc = QString("角色 %1 已继承角色: %2").arg(targetId, relatedId);
        break;
    case PermissionChangeType::RoleInheritanceRemoved:
        desc = QString("角色 %1 已移除继承角色: %2").arg(targetId, relatedId);
        break;
    }
    
    if (!operatorId.isEmpty()) {
        desc += QString(" (操作者: %1)").arg(operatorId);
    }
    
    return desc;
}

PermissionChangeNotification PermissionChangeNotifier::createNotification(
    PermissionChangeType type,
    const QString& targetId,
    const QString& targetType,
    const QString& relatedId,
    const QString& relatedType,
    const QString& operatorId,
    const QVariantMap& metadata)
{
    PermissionChangeNotification notification;
    notification.changeType = type;
    notification.targetId = targetId;
    notification.targetType = targetType;
    notification.relatedId = relatedId;
    notification.relatedType = relatedType;
    notification.operatorId = operatorId;
    notification.timestamp = QDateTime::currentDateTime();
    notification.metadata = metadata;
    return notification;
}

void PermissionChangeNotifier::publishToEventBus(EventBus* eventBus, 
                                                 const PermissionChangeNotification& notification)
{
    if (!eventBus) {
        Logger::warning("PermissionChangeNotifier", "EventBus not available, skipping notification");
        return;
    }
    
    // 发布特定类型的事件
    QString eventName = getEventName(notification.changeType);
    eventBus->publish(eventName, notification.toJson());
    
    // 发布通用权限变更事件
    eventBus->publish("rbac.permission.changed", notification.toJson());
    
    Logger::debug("PermissionChangeNotifier", 
                  QString("Published permission change event: %1").arg(notification.getDescription()));
}

QString PermissionChangeNotifier::getEventName(PermissionChangeType type)
{
    switch (type) {
    case PermissionChangeType::PermissionAdded:
        return "rbac.permission.added";
    case PermissionChangeType::PermissionRemoved:
        return "rbac.permission.removed";
    case PermissionChangeType::RoleAdded:
        return "rbac.role.added";
    case PermissionChangeType::RoleRemoved:
        return "rbac.role.removed";
    case PermissionChangeType::RolePermissionGranted:
        return "rbac.role.permission.granted";
    case PermissionChangeType::RolePermissionRevoked:
        return "rbac.role.permission.revoked";
    case PermissionChangeType::UserAdded:
        return "rbac.user.added";
    case PermissionChangeType::UserRemoved:
        return "rbac.user.removed";
    case PermissionChangeType::UserRoleAssigned:
        return "rbac.user.role.assigned";
    case PermissionChangeType::UserRoleRevoked:
        return "rbac.user.role.revoked";
    case PermissionChangeType::UserPermissionGranted:
        return "rbac.user.permission.granted";
    case PermissionChangeType::UserPermissionRevoked:
        return "rbac.user.permission.revoked";
    case PermissionChangeType::RoleInheritanceAdded:
        return "rbac.role.inheritance.added";
    case PermissionChangeType::RoleInheritanceRemoved:
        return "rbac.role.inheritance.removed";
    }
    return "rbac.unknown";
}

QStringList PermissionChangeNotifier::getAllEventNames()
{
    return QStringList() 
        << "rbac.permission.added"
        << "rbac.permission.removed"
        << "rbac.role.added"
        << "rbac.role.removed"
        << "rbac.role.permission.granted"
        << "rbac.role.permission.revoked"
        << "rbac.user.added"
        << "rbac.user.removed"
        << "rbac.user.role.assigned"
        << "rbac.user.role.revoked"
        << "rbac.user.permission.granted"
        << "rbac.user.permission.revoked"
        << "rbac.role.inheritance.added"
        << "rbac.role.inheritance.removed"
        << "rbac.permission.changed";
}

} // namespace Core
} // namespace Eagle
