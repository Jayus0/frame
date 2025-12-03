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
)

set(HEADERS
    ${CMAKE_SOURCE_DIR}/include/eagle/core/IPlugin.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/PluginManager.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/ServiceRegistry.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/ServiceDescriptor.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/EventBus.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/Logger.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/Framework.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/PluginSignature.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/CircuitBreaker.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/ConfigEncryption.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/PluginIsolation.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/RBAC.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/AuditLog.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/PerformanceMonitor.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/AlertSystem.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/RateLimiter.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/ApiKeyManager.h
    ${CMAKE_SOURCE_DIR}/include/eagle/core/SessionManager.h
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
