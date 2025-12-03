QT += core widgets network

CONFIG += c++17 plugin
CONFIG += warn_on

TARGET = EagleFramework
TEMPLATE = subdirs

SUBDIRS += \
    core \
    examples

# 输出目录
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui

# 包含目录
INCLUDEPATH += $$PWD/include

# 定义
DEFINES += EAGLE_FRAMEWORK_VERSION=\\\"1.0.0\\\"
