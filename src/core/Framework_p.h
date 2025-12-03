#ifndef FRAMEWORK_P_H
#define FRAMEWORK_P_H

#include "eagle/core/PluginManager.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/EventBus.h"
#include "eagle/core/ConfigManager.h"

namespace Eagle {
namespace Core {

class Framework::Private {
public:
    PluginManager* pluginManager;
    ServiceRegistry* serviceRegistry;
    EventBus* eventBus;
    ConfigManager* configManager;
    bool initialized;
    
    Private() 
        : pluginManager(nullptr)
        , serviceRegistry(nullptr)
        , eventBus(nullptr)
        , configManager(nullptr)
        , initialized(false)
    {
    }
};

} // namespace Core
} // namespace Eagle

#endif // FRAMEWORK_P_H
