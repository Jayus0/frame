#ifndef RBAC_P_H
#define RBAC_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>
#include "eagle/core/RBAC.h"

namespace Eagle {
namespace Core {

class RBACManager::Private {
public:
    QMap<QString, Permission> permissions;
    QMap<QString, Role> roles;
    QMap<QString, User> users;
    mutable QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // RBAC_P_H
