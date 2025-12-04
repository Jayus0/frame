QT += core widgets network

CONFIG += c++17 warn_on
CONFIG += plugin

TARGET = EagleCore
TEMPLATE = lib

# 插件管理模块
PLUGIN_SOURCES += \
    ../src/core/plugin/PluginManager.cpp \
    ../src/core/plugin/PluginSignature.cpp \
    ../src/core/plugin/PluginIsolation.cpp

# 服务模块
SERVICE_SOURCES += \
    ../src/core/service/ServiceRegistry.cpp \
    ../src/core/service/CircuitBreaker.cpp \
    ../src/core/service/RetryPolicy.cpp \
    ../src/core/service/DegradationPolicy.cpp \
    ../src/core/service/FailoverManager.cpp

# 配置模块
CONFIG_SOURCES += \
    ../src/core/config/ConfigManager.cpp \
    ../src/core/config/ConfigEncryption.cpp \
    ../src/core/config/BackupManager.cpp

# 安全模块
SECURITY_SOURCES += \
    ../src/core/security/RBAC.cpp \
    ../src/core/security/AuditLog.cpp \
    ../src/core/security/ApiKeyManager.cpp \
    ../src/core/security/SessionManager.cpp \
    ../src/core/security/RateLimiter.cpp

# 监控模块
MONITORING_SOURCES += \
    ../src/core/monitoring/PerformanceMonitor.cpp \
    ../src/core/monitoring/AlertSystem.cpp \
    ../src/core/monitoring/NotificationChannel.cpp \
    ../src/core/monitoring/EmailChannel.cpp \
    ../src/core/monitoring/WebhookChannel.cpp

# API模块
API_SOURCES += \
    ../src/core/api/ApiServer.cpp \
    ../src/core/api/ApiRoutes.cpp

# 事件模块
EVENT_SOURCES += \
    ../src/core/event/EventBus.cpp

# 测试模块
TEST_SOURCES += \
    ../src/core/test/TestCaseBase.cpp \
    ../src/core/test/TestRunner.cpp

# 框架模块
FRAMEWORK_SOURCES += \
    ../src/core/framework/Framework.cpp \
    ../src/core/framework/Logger.cpp

# 热重载模块
HOTRELOAD_SOURCES += \
    ../src/core/hotreload/HotReloadManager.cpp

# 合并所有源文件
SOURCES += \
    $$PLUGIN_SOURCES \
    $$SERVICE_SOURCES \
    $$CONFIG_SOURCES \
    $$SECURITY_SOURCES \
    $$MONITORING_SOURCES \
    $$API_SOURCES \
    $$EVENT_SOURCES \
    $$TEST_SOURCES \
    $$FRAMEWORK_SOURCES \
    $$HOTRELOAD_SOURCES

# 头文件
HEADERS += \
    ../include/eagle/core/IPlugin.h \
    ../include/eagle/core/PluginManager.h \
    ../src/core/plugin/PluginManager_p.h \
    ../include/eagle/core/ServiceRegistry.h \
    ../src/core/service/ServiceRegistry_p.h \
    ../include/eagle/core/ServiceDescriptor.h \
    ../include/eagle/core/EventBus.h \
    ../src/core/event/EventBus_p.h \
    ../include/eagle/core/ConfigManager.h \
    ../src/core/config/ConfigManager_p.h \
    ../include/eagle/core/Logger.h \
    ../include/eagle/core/Framework.h \
    ../src/core/framework/Framework_p.h \
    ../include/eagle/core/PluginSignature.h \
    ../include/eagle/core/CircuitBreaker.h \
    ../include/eagle/core/ConfigEncryption.h \
    ../include/eagle/core/PluginIsolation.h \
    ../include/eagle/core/RBAC.h \
    ../src/core/security/RBAC_p.h \
    ../include/eagle/core/AuditLog.h \
    ../src/core/security/AuditLog_p.h \
    ../include/eagle/core/PerformanceMonitor.h \
    ../src/core/monitoring/PerformanceMonitor_p.h \
    ../include/eagle/core/AlertSystem.h \
    ../src/core/monitoring/AlertSystem_p.h \
    ../include/eagle/core/RateLimiter.h \
    ../src/core/security/RateLimiter_p.h \
    ../include/eagle/core/ApiKeyManager.h \
    ../src/core/security/ApiKeyManager_p.h \
    ../include/eagle/core/SessionManager.h \
    ../src/core/security/SessionManager_p.h \
    ../include/eagle/core/ApiServer.h \
    ../src/core/api/ApiServer_p.h \
    ../include/eagle/core/ApiRoutes.h \
    ../include/eagle/core/RetryPolicy.h \
    ../src/core/service/RetryPolicy_p.h \
    ../include/eagle/core/DegradationPolicy.h \
    ../src/core/service/DegradationPolicy_p.h \
    ../include/eagle/core/FailoverManager.h \
    ../src/core/service/FailoverManager_p.h \
    ../include/eagle/core/NotificationChannel.h \
    ../include/eagle/core/EmailChannel.h \
    ../include/eagle/core/WebhookChannel.h \
    ../include/eagle/core/BackupManager.h \
    ../src/core/config/BackupManager_p.h \
    ../include/eagle/core/TestCaseBase.h \
    ../include/eagle/core/TestRunner.h \
    ../src/core/test/TestRunner_p.h \
    ../include/eagle/core/HotReloadManager.h \
    ../src/core/hotreload/HotReloadManager_p.h

# 包含目录
INCLUDEPATH += $$PWD/../include

# 输出目录
DESTDIR = $$PWD/../lib
OBJECTS_DIR = $$PWD/../build/core/obj
MOC_DIR = $$PWD/../build/core/moc
RCC_DIR = $$PWD/../build/core/rcc

# 版本信息
VERSION = 1.0.0
QMAKE_TARGET_PRODUCT = "Eagle Framework Core"
QMAKE_TARGET_DESCRIPTION = "Eagle Framework Core Library"
QMAKE_TARGET_COPYRIGHT = "Copyright (c) 2024"
