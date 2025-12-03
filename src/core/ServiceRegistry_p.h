#ifndef SERVICEREGISTRY_P_H
#define SERVICEREGISTRY_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include "eagle/core/ServiceDescriptor.h"

namespace Eagle {
namespace Core {

class ServiceRegistryPrivate {
public:
    QMap<QString, QList<ServiceDescriptor>> services; // serviceName -> versions
    QMap<QString, QObject*> providers; // serviceName+version -> provider
    QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // SERVICEREGISTRY_P_H
