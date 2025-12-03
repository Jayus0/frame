#ifndef EAGLE_CORE_SERVICEDESCRIPTOR_H
#define EAGLE_CORE_SERVICEDESCRIPTOR_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

namespace Eagle {
namespace Core {

struct ServiceDescriptor {
    QString serviceName;
    QString version;
    QStringList methods;
    QStringList endpoints;
    QString healthCheck;
    QObject* provider;
    
    ServiceDescriptor() : provider(nullptr) {}
    
    bool isValid() const {
        return !serviceName.isEmpty() && !version.isEmpty() && provider != nullptr;
    }
    
    bool operator==(const ServiceDescriptor& other) const {
        return serviceName == other.serviceName && version == other.version;
    }
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::ServiceDescriptor)

#endif // EAGLE_CORE_SERVICEDESCRIPTOR_H
