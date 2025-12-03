QT += core widgets network

CONFIG += c++17 warn_on
CONFIG += plugin

TARGET = EagleCore
TEMPLATE = lib

# 源文件
SOURCES += \
    ../src/core/PluginManager.cpp \
    ../src/core/ServiceRegistry.cpp \
    ../src/core/EventBus.cpp \
    ../src/core/ConfigManager.cpp \
    ../src/core/Logger.cpp \
    ../src/core/Framework.cpp \
    ../src/core/PluginSignature.cpp \
    ../src/core/CircuitBreaker.cpp \
    ../src/core/ConfigEncryption.cpp \
    ../src/core/PluginIsolation.cpp

# 头文件
HEADERS += \
    ../include/eagle/core/IPlugin.h \
    ../include/eagle/core/PluginManager.h \
    ../src/core/PluginManager_p.h \
    ../include/eagle/core/ServiceRegistry.h \
    ../src/core/ServiceRegistry_p.h \
    ../include/eagle/core/ServiceDescriptor.h \
    ../include/eagle/core/EventBus.h \
    ../src/core/EventBus_p.h \
    ../include/eagle/core/ConfigManager.h \
    ../src/core/ConfigManager_p.h \
    ../include/eagle/core/Logger.h \
    ../include/eagle/core/Framework.h \
    ../src/core/Framework_p.h \
    ../include/eagle/core/PluginSignature.h \
    ../include/eagle/core/CircuitBreaker.h \
    ../include/eagle/core/ConfigEncryption.h \
    ../include/eagle/core/PluginIsolation.h

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
