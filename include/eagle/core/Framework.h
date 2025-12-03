#ifndef EAGLE_CORE_FRAMEWORK_H
#define EAGLE_CORE_FRAMEWORK_H

#include <QtCore/QObject>
#include "PluginManager.h"
#include "ServiceRegistry.h"
#include "EventBus.h"
#include "ConfigManager.h"

namespace Eagle {
namespace Core {

/**
 * @brief 框架主类
 * 
 * 统一管理所有核心组件
 */
class Framework : public QObject {
    Q_OBJECT
    
public:
    static Framework* instance();
    static void destroy();
    
    // 初始化
    bool initialize(const QString& configPath = QString());
    void shutdown();
    
    // 核心组件访问
    PluginManager* pluginManager() const;
    ServiceRegistry* serviceRegistry() const;
    EventBus* eventBus() const;
    ConfigManager* configManager() const;
    
    // 框架信息
    QString version() const;
    bool isInitialized() const;
    
signals:
    void initialized();
    void shutdown();
    
private:
    explicit Framework(QObject* parent = nullptr);
    ~Framework();
    Q_DISABLE_COPY(Framework)
    
    static Framework* s_instance;
    
    class Private;
    Private* d;
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_FRAMEWORK_H
