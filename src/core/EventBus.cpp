#include "eagle/core/EventBus.h"
#include "EventBus_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>
#include <functional>

namespace Eagle {
namespace Core {

EventBus::EventBus(QObject* parent)
    : QObject(parent)
    , d_ptr(new EventBusPrivate)
{
}

EventBus::~EventBus()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->subscriptions.clear();
    delete d_ptr;
}

void EventBus::subscribe(const QString& event, QObject* receiver, const char* method)
{
    if (!receiver || !method) {
        Logger::warning("EventBus", "Invalid subscription parameters");
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    EventSubscription sub;
    sub.receiver = receiver;
    sub.method = QMetaObject::normalizedSignature(method);
    sub.isCallback = false;
    
    d->subscriptions[event].append(sub);
    
    Logger::debug("EventBus", QString("订阅事件: %1 -> %2::%3")
        .arg(event, receiver->metaObject()->className(), method));
}

void EventBus::subscribe(const QString& event, std::function<void(const QVariant&)> callback)
{
    if (!callback) {
        Logger::warning("EventBus", "Invalid callback");
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    EventSubscription sub;
    sub.callback = callback;
    sub.isCallback = true;
    
    d->subscriptions[event].append(sub);
    
    Logger::debug("EventBus", QString("订阅事件(回调): %1").arg(event));
}

void EventBus::unsubscribe(QObject* receiver)
{
    if (!receiver) {
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    for (auto it = d->subscriptions.begin(); it != d->subscriptions.end();) {
        QList<EventSubscription>& subs = it.value();
        for (int i = subs.size() - 1; i >= 0; --i) {
            if (subs[i].receiver == receiver && !subs[i].isCallback) {
                subs.removeAt(i);
            }
        }
        if (subs.isEmpty()) {
            it = d->subscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

void EventBus::unsubscribe(const QString& event, QObject* receiver)
{
    if (!receiver) {
        return;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->subscriptions.contains(event)) {
        return;
    }
    
    QList<EventSubscription>& subs = d->subscriptions[event];
    for (int i = subs.size() - 1; i >= 0; --i) {
        if (subs[i].receiver == receiver && !subs[i].isCallback) {
            subs.removeAt(i);
        }
    }
    
    if (subs.isEmpty()) {
        d->subscriptions.remove(event);
    }
}

void EventBus::publish(const QString& event, const QVariant& data)
{
    auto* d = d_func();
    
    QList<EventSubscription> subs;
    {
        QMutexLocker locker(&d->mutex);
        if (d->subscriptions.contains(event)) {
            subs = d->subscriptions[event];
        }
    }
    
    for (const EventSubscription& sub : subs) {
        if (sub.isCallback) {
            try {
                sub.callback(data);
            } catch (const std::exception& e) {
                Logger::error("EventBus", QString("回调执行异常: %1").arg(e.what()));
            }
        } else if (sub.receiver) {
            const QMetaObject* metaObj = sub.receiver->metaObject();
            int methodIndex = metaObj->indexOfMethod(sub.method.constData());
            if (methodIndex != -1) {
                QMetaMethod method = metaObj->method(methodIndex);
                if (method.parameterCount() == 1) {
                    QGenericArgument arg = Q_ARG(QVariant, data);
                    method.invoke(sub.receiver, Qt::QueuedConnection, arg);
                } else {
                    method.invoke(sub.receiver, Qt::QueuedConnection);
                }
            }
        }
    }
    
    emit eventPublished(event, data);
}

void EventBus::publishSync(const QString& event, const QVariant& data)
{
    auto* d = d_func();
    
    QList<EventSubscription> subs;
    {
        QMutexLocker locker(&d->mutex);
        if (d->subscriptions.contains(event)) {
            subs = d->subscriptions[event];
        }
    }
    
    for (const EventSubscription& sub : subs) {
        if (sub.isCallback) {
            try {
                sub.callback(data);
            } catch (const std::exception& e) {
                Logger::error("EventBus", QString("回调执行异常: %1").arg(e.what()));
            }
        } else if (sub.receiver) {
            const QMetaObject* metaObj = sub.receiver->metaObject();
            int methodIndex = metaObj->indexOfMethod(sub.method.constData());
            if (methodIndex != -1) {
                QMetaMethod method = metaObj->method(methodIndex);
                if (method.parameterCount() == 1) {
                    QGenericArgument arg = Q_ARG(QVariant, data);
                    method.invoke(sub.receiver, Qt::DirectConnection, arg);
                } else {
                    method.invoke(sub.receiver, Qt::DirectConnection);
                }
            }
        }
    }
    
    emit eventPublished(event, data);
}

} // namespace Core
} // namespace Eagle
