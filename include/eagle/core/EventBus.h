#ifndef EAGLE_CORE_EVENTBUS_H
#define EAGLE_CORE_EVENTBUS_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QMap>
#include <QtCore/QMutex>

namespace Eagle {
namespace Core {

class EventBusPrivate;

/**
 * @brief 事件总线
 * 
 * 提供发布/订阅模式的事件通信机制
 */
class EventBus : public QObject {
    Q_OBJECT
    
public:
    explicit EventBus(QObject* parent = nullptr);
    ~EventBus();
    
    // 订阅事件
    void subscribe(const QString& event, QObject* receiver, const char* method);
    void subscribe(const QString& event, std::function<void(const QVariant&)> callback);
    
    // 取消订阅
    void unsubscribe(QObject* receiver);
    void unsubscribe(const QString& event, QObject* receiver);
    
    // 发布事件
    void publish(const QString& event, const QVariant& data = QVariant());
    
    // 同步发布（等待所有订阅者处理完成）
    void publishSync(const QString& event, const QVariant& data = QVariant());
    
signals:
    void eventPublished(const QString& event, const QVariant& data);
    
private:
    Q_DISABLE_COPY(EventBus)
    EventBusPrivate* d_ptr;
    
    inline EventBusPrivate* d_func() { return d_ptr; }
    inline const EventBusPrivate* d_func() const { return d_ptr; }
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_EVENTBUS_H
