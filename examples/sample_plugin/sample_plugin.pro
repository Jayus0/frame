QT += core widgets

CONFIG += c++17 warn_on
CONFIG += plugin

TARGET = SamplePlugin
TEMPLATE = lib

# 源文件
SOURCES += \
    SamplePlugin.cpp

# 头文件
HEADERS += \
    SamplePlugin.h

# 包含目录
INCLUDEPATH += $$PWD/../../include

# 链接库
LIBS += -L$$PWD/../../lib -lEagleCore

# 输出目录
DESTDIR = $$PWD/../../plugins
OBJECTS_DIR = $$PWD/../../build/sample_plugin/obj
MOC_DIR = $$PWD/../../build/sample_plugin/moc

# 版本信息
VERSION = 1.0.0
