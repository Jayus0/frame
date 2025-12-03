QT -= gui
QT += core

CONFIG += c++17 warn_on ordered

TEMPLATE = subdirs

# 子项目列表（按编译顺序）
SUBDIRS += \
    core \
    examples

# 确保 core 在 examples 之前编译
core.depends = 
examples.depends = core
