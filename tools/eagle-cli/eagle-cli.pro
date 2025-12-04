QT += core
CONFIG += console
CONFIG -= app_bundle

TARGET = eagle-cli
TEMPLATE = app

SOURCES += main.cpp

# 输出目录
DESTDIR = $$PWD/../../bin
OBJECTS_DIR = $$PWD/../../build/tools/eagle-cli/obj
MOC_DIR = $$PWD/../../build/tools/eagle-cli/moc

# 版本信息
VERSION = 1.0.0
QMAKE_TARGET_PRODUCT = "Eagle CLI Tool"
QMAKE_TARGET_DESCRIPTION = "Eagle Framework Command Line Tool"
QMAKE_TARGET_COPYRIGHT = "Copyright (c) 2024"
