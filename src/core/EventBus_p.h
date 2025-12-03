#ifndef EVENTBUS_P_H
#define EVENTBUS_P_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QByteArray>
#include <functional>

namespace Eagle {
namespace Core {

struct EventSubscription {
    QObject* receiver;
    QByteArray method;
    std::function<void(const QVariant&)> callback;
    bool isCallback;
    
    EventSubscription() : receiver(nullptr), isCallback(false) {}
};

class EventBusPrivate {
public:
    QMap<QString, QList<EventSubscription>> subscriptions;
    QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // EVENTBUS_P_H
