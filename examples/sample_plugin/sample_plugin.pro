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
RCC_DIR = $$PWD/../../build/sample_plugin/rcc

# 确保构建目录存在
build_dir = $$OBJECTS_DIR
!exists($$build_dir): system(mkdir -p $$build_dir)
moc_dir = $$MOC_DIR
!exists($$moc_dir): system(mkdir -p $$moc_dir)
rcc_dir = $$RCC_DIR
!exists($$rcc_dir): system(mkdir -p $$rcc_dir)

# 版本信息
VERSION = 1.0.0
