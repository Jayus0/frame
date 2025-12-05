#ifndef EAGLE_CORE_FRAMEWORK_H
#define EAGLE_CORE_FRAMEWORK_H

#include <QtCore/QObject>
#include "PluginManager.h"
#include "ServiceRegistry.h"
#include "EventBus.h"
#include "ConfigManager.h"
#include "RBAC.h"
#include "AuditLog.h"
#include "PerformanceMonitor.h"
#include "AlertSystem.h"
#include "RateLimiter.h"
#include "ApiKeyManager.h"
#include "SessionManager.h"
#include "ApiServer.h"
#include "BackupManager.h"
#include "HotReloadManager.h"
#include "FailoverManager.h"
#include "DiagnosticManager.h"
#include "ResourceMonitor.h"

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
    RBACManager* rbacManager() const;
    AuditLogManager* auditLogManager() const;
    PerformanceMonitor* performanceMonitor() const;
    AlertSystem* alertSystem() const;
    RateLimiter* rateLimiter() const;
    ApiKeyManager* apiKeyManager() const;
    SessionManager* sessionManager() const;
    ApiServer* apiServer() const;
    BackupManager* backupManager() const;
    HotReloadManager* hotReloadManager() const;
    FailoverManager* failoverManager() const;
    DiagnosticManager* diagnosticManager() const;
    ResourceMonitor* resourceMonitor() const;
    
    // 框架信息
    QString version() const;
    bool isInitialized() const;
    
    // 系统健康检查
    QJsonObject systemHealth() const;
    
signals:
    void initialized();
    void shutdownCompleted();  // 重命名信号，避免与函数名冲突
    
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
