# 插件管理模块
set(PLUGIN_SOURCES
    plugin/PluginManager.cpp
    plugin/PluginSignature.cpp
    plugin/PluginIsolation.cpp
    plugin/ResourceMonitor.cpp
)

# 服务模块
set(SERVICE_SOURCES
    service/ServiceRegistry.cpp
    service/CircuitBreaker.cpp
    service/RetryPolicy.cpp
    service/DegradationPolicy.cpp
    service/FailoverManager.cpp
    service/LoadBalancer.cpp
    service/AsyncServiceCall.cpp
)

# 配置模块
set(CONFIG_SOURCES
    config/ConfigManager.cpp
    config/ConfigEncryption.cpp
    config/ConfigSchema.cpp
    config/ConfigVersion.cpp
    config/ConfigFormat.cpp
    config/BackupManager.cpp
)

# 安全模块
set(SECURITY_SOURCES
    security/RBAC.cpp
    security/AuditLog.cpp
    security/ApiKeyManager.cpp
    security/SessionManager.cpp
    security/RateLimiter.cpp
)

# 监控模块
set(MONITORING_SOURCES
    monitoring/PerformanceMonitor.cpp
    monitoring/AlertSystem.cpp
    monitoring/NotificationChannel.cpp
    monitoring/EmailChannel.cpp
    monitoring/WebhookChannel.cpp
    monitoring/DiagnosticManager.cpp
)

# API模块
set(API_SOURCES
    api/ApiServer.cpp
    api/ApiRoutes.cpp
)

# 事件模块
set(EVENT_SOURCES
    event/EventBus.cpp
)

# 测试模块
set(TEST_SOURCES
    test/TestCaseBase.cpp
    test/TestRunner.cpp
)

# 框架模块
set(FRAMEWORK_SOURCES
    framework/Framework.cpp
    framework/Logger.cpp
)

# 热重载模块
set(HOTRELOAD_SOURCES
    hotreload/HotReloadManager.cpp
)

# 合并所有源文件
set(SOURCES
    ${PLUGIN_SOURCES}
    ${SERVICE_SOURCES}
    ${CONFIG_SOURCES}
    ${SECURITY_SOURCES}
    ${MONITORING_SOURCES}
    ${API_SOURCES}
    ${EVENT_SOURCES}
    ${TEST_SOURCES}
    ${FRAMEWORK_SOURCES}
    ${HOTRELOAD_SOURCES}
)

# 头文件
set(HEADERS
    ../../include/eagle/core/IPlugin.h
    ../../include/eagle/core/PluginManager.h
    ../../include/eagle/core/ServiceRegistry.h
    ../../include/eagle/core/ServiceDescriptor.h
    ../../include/eagle/core/EventBus.h
    ../../include/eagle/core/ConfigManager.h
    ../../include/eagle/core/Logger.h
    ../../include/eagle/core/Framework.h
    ../../include/eagle/core/PluginSignature.h
    ../../include/eagle/core/ResourceMonitor.h
    ../../include/eagle/core/CircuitBreaker.h
    ../../include/eagle/core/ConfigEncryption.h
    ../../include/eagle/core/PluginIsolation.h
    ../../include/eagle/core/RBAC.h
    ../../include/eagle/core/AuditLog.h
    ../../include/eagle/core/PerformanceMonitor.h
    ../../include/eagle/core/AlertSystem.h
    ../../include/eagle/core/RateLimiter.h
    ../../include/eagle/core/ApiKeyManager.h
    ../../include/eagle/core/SessionManager.h
    ../../include/eagle/core/ApiServer.h
    ../../include/eagle/core/ApiRoutes.h
    ../../include/eagle/core/RetryPolicy.h
    ../../include/eagle/core/DegradationPolicy.h
    ../../include/eagle/core/FailoverManager.h
    ../../include/eagle/core/NotificationChannel.h
    ../../include/eagle/core/EmailChannel.h
    ../../include/eagle/core/WebhookChannel.h
    ../../include/eagle/core/DiagnosticManager.h
    ../../include/eagle/core/BackupManager.h
    ../../include/eagle/core/TestCaseBase.h
    ../../include/eagle/core/TestRunner.h
    ../../include/eagle/core/HotReloadManager.h
)

add_library(EagleCore SHARED ${SOURCES} ${HEADERS})

target_link_libraries(EagleCore
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Concurrent
)

target_include_directories(EagleCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(EagleCore PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${HEADERS}"
)
