set(SOURCES
    PluginManager.cpp
    ServiceRegistry.cpp
    EventBus.cpp
    ConfigManager.cpp
    Logger.cpp
    Framework.cpp
    PluginSignature.cpp
    CircuitBreaker.cpp
    ConfigEncryption.cpp
    PluginIsolation.cpp
    RBAC.cpp
    AuditLog.cpp
    PerformanceMonitor.cpp
    AlertSystem.cpp
    RateLimiter.cpp
    ApiKeyManager.cpp
    SessionManager.cpp
    ApiServer.cpp
    ApiRoutes.cpp
    RetryPolicy.cpp
)

# 使用相对路径（相对于src/core目录），这样Qt Creator会显示相对路径而不是绝对路径
# src/core -> ../../include 表示从src/core目录向上两级到项目根目录，然后进入include目录
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
)

add_library(EagleCore SHARED ${SOURCES} ${HEADERS})

target_link_libraries(EagleCore
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)

target_include_directories(EagleCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(EagleCore PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${HEADERS}"
)
