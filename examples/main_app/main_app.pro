QT += core widgets network

CONFIG += c++17 warn_on

TARGET = EagleApp
TEMPLATE = app

# 源文件
SOURCES += \
    main.cpp \
    MainWindow.cpp

# 头文件
HEADERS += \
    MainWindow.h

# 包含目录
INCLUDEPATH += $$PWD/../../include

# 链接库
LIBS += -L$$PWD/../../lib -lEagleCore

# 输出目录
DESTDIR = $$PWD/../../bin
OBJECTS_DIR = $$PWD/../../build/main_app/obj
MOC_DIR = $$PWD/../../build/main_app/moc

# 应用程序信息
VERSION = 1.0.0
QMAKE_TARGET_PRODUCT = "Eagle Framework Demo"
QMAKE_TARGET_DESCRIPTION = "Eagle Framework Demo Application"
QMAKE_TARGET_COPYRIGHT = "Copyright (c) 2024"
