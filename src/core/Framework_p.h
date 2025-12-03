#ifndef FRAMEWORK_P_H
#define FRAMEWORK_P_H

#include "eagle/core/PluginManager.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/EventBus.h"
#include "eagle/core/ConfigManager.h"
#include "eagle/core/RBAC.h"
#include "eagle/core/AuditLog.h"
#include "eagle/core/PerformanceMonitor.h"
#include "eagle/core/AlertSystem.h"

namespace Eagle {
namespace Core {

class Framework::Private {
public:
    PluginManager* pluginManager;
    ServiceRegistry* serviceRegistry;
    EventBus* eventBus;
    ConfigManager* configManager;
    RBACManager* rbacManager;
    AuditLogManager* auditLogManager;
    PerformanceMonitor* performanceMonitor;
    AlertSystem* alertSystem;
    bool initialized;
    
    Private() 
        : pluginManager(nullptr)
        , serviceRegistry(nullptr)
        , eventBus(nullptr)
        , configManager(nullptr)
        , rbacManager(nullptr)
        , auditLogManager(nullptr)
        , performanceMonitor(nullptr)
        , alertSystem(nullptr)
        , initialized(false)
    {
    }
};

} // namespace Core
} // namespace Eagle

#endif // FRAMEWORK_P_H
