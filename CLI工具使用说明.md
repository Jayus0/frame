# Eagle CLI工具使用说明

## 概述

`eagle-cli` 是Eagle Framework的命令行工具，提供项目创建、插件生成、配置管理、调试等功能，帮助开发者快速开始使用框架。

## 安装

### 从源码编译

```bash
cd tools/eagle-cli
mkdir build && cd build
cmake ..
make
```

编译后的可执行文件位于 `build/bin/eagle-cli`。

### 添加到PATH

为了方便使用，可以将 `eagle-cli` 添加到系统PATH：

```bash
# Linux/macOS
export PATH=$PATH:/path/to/eagle-cli

# 或创建符号链接
sudo ln -s /path/to/eagle-cli /usr/local/bin/eagle-cli
```

## 命令列表

### 1. 创建项目（create project）

创建一个新的Eagle Framework项目。

**语法：**
```bash
eagle-cli create project <project-name>
```

**示例：**
```bash
eagle-cli create project MyApp
```

**功能：**
- 创建项目目录结构
- 生成CMakeLists.txt
- 生成main.cpp模板
- 生成README.md
- 创建配置文件

**生成的项目结构：**
```
MyApp/
├── CMakeLists.txt
├── README.md
├── src/
│   └── main.cpp
├── include/
├── plugins/
├── config/
│   └── app.json
└── build/
```

### 2. 创建插件（create plugin）

创建一个新的Eagle Framework插件。

**语法：**
```bash
eagle-cli create plugin <plugin-name> [--type=<type>]
```

**参数：**
- `plugin-name`: 插件名称
- `--type`: 插件类型（service、ui、tool），默认为service

**示例：**
```bash
# 创建服务插件
eagle-cli create plugin MyServicePlugin --type=service

# 创建UI插件
eagle-cli create plugin MyUIPlugin --type=ui

# 创建工具插件
eagle-cli create plugin MyToolPlugin --type=tool
```

**功能：**
- 创建插件目录结构
- 生成CMakeLists.txt
- 生成插件头文件和源文件
- 实现IPlugin接口
- 生成README.md

**生成的插件结构：**
```
MyServicePlugin/
├── CMakeLists.txt
├── README.md
├── src/
│   └── MyServicePlugin.cpp
└── include/
    └── MyServicePlugin.h
```

### 3. 配置管理（config）

管理Eagle Framework的配置。

**语法：**
```bash
eagle-cli config <command> [key] [value]
```

**命令：**
- `get <key>`: 获取配置值
- `set <key> <value>`: 设置配置值
- `list`: 列出所有配置

**示例：**
```bash
# 获取配置
eagle-cli config get app_name

# 设置配置
eagle-cli config set app_name "MyApp"

# 列出所有配置
eagle-cli config list
```

**配置文件位置：**
- Linux: `~/.config/eagle/config.json`
- macOS: `~/Library/Preferences/eagle/config.json`
- Windows: `%APPDATA%/eagle/config.json`

### 4. 调试命令（debug）

提供调试和诊断功能。

**语法：**
```bash
eagle-cli debug <command>
```

**命令：**
- `list-plugins`: 列出已加载的插件（需要运行中的框架实例）
- `list-services`: 列出已注册的服务（需要运行中的框架实例）
- `health`: 检查框架健康状态（需要运行中的框架实例）

**示例：**
```bash
eagle-cli debug list-plugins
eagle-cli debug list-services
eagle-cli debug health
```

**注意：** 调试命令需要连接到运行中的Eagle Framework实例，当前版本为占位实现。

## 使用示例

### 示例1：创建新项目

```bash
# 创建项目
eagle-cli create project MyEagleApp

# 进入项目目录
cd MyEagleApp

# 构建项目
mkdir build && cd build
cmake ..
make

# 运行项目
./MyEagleApp
```

### 示例2：创建插件

```bash
# 创建服务插件
eagle-cli create plugin UserService --type=service

# 进入插件目录
cd UserService

# 构建插件
mkdir build && cd build
cmake ..
make

# 插件会生成在 build/ 目录中
```

### 示例3：配置管理

```bash
# 设置应用名称
eagle-cli config set app_name "MyApp"

# 设置版本号
eagle-cli config set version "1.0.0"

# 查看所有配置
eagle-cli config list

# 获取特定配置
eagle-cli config get app_name
```

## 项目模板

### 项目模板结构

创建的项目包含以下文件：

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp Qt5::Core Qt5::Widgets)
```

**src/main.cpp:**
```cpp
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "Hello from MyApp!";
    
    return 0;
}
```

## 插件模板

### 服务插件模板

生成的插件实现了 `IPlugin` 接口：

**头文件：**
```cpp
#ifndef MYSERVICEPLUGIN_H
#define MYSERVICEPLUGIN_H

#include <QtCore/QObject>
#include "eagle/core/IPlugin.h"

class MyServicePlugin : public QObject, public Eagle::Core::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.eagle.plugin" FILE "MyServicePlugin.json")
    Q_INTERFACES(Eagle::Core::IPlugin)

public:
    explicit MyServicePlugin(QObject* parent = nullptr);
    ~MyServicePlugin();
    
    // IPlugin interface
    QString name() const override;
    QString version() const override;
    bool initialize() override;
    void shutdown() override;
};

#endif // MYSERVICEPLUGIN_H
```

## 配置文件格式

配置文件使用JSON格式：

```json
{
    "app_name": "EagleApp",
    "version": "1.0.0"
}
```

## 故障排除

### 问题1：命令未找到

**错误：**
```bash
eagle-cli: command not found
```

**解决：**
- 确保 `eagle-cli` 已编译
- 将 `eagle-cli` 添加到PATH
- 或使用完整路径：`/path/to/eagle-cli`

### 问题2：目录已存在

**错误：**
```bash
Error: Directory already exists: /path/to/project
```

**解决：**
- 删除现有目录
- 或使用不同的项目名称

### 问题3：Qt未找到

**错误：**
```bash
CMake Error: Could not find Qt5
```

**解决：**
- 确保Qt5已安装
- 设置 `CMAKE_PREFIX_PATH` 环境变量
- 或使用 `-DCMAKE_PREFIX_PATH=/path/to/qt5` 参数

## 未来计划

CLI工具将继续完善，计划添加：

1. **插件签名生成**
   ```bash
   eagle-cli sign plugin <plugin-path>
   ```

2. **配置验证**
   ```bash
   eagle-cli config validate
   ```

3. **远程调试**
   ```bash
   eagle-cli debug connect <host:port>
   ```

4. **性能分析**
   ```bash
   eagle-cli profile <command>
   ```

5. **测试运行**
   ```bash
   eagle-cli test <test-name>
   ```

## 贡献

欢迎贡献代码和建议！请参考项目贡献指南。

## 许可证

与Eagle Framework使用相同的许可证。
