# Eagle Framework - 企业级Qt插件化框架

## 项目简介

Eagle Framework 是一个基于 Qt 5.15 的企业级插件化框架，旨在提供统一的开发规范、能力复用和敏捷开发能力。

## 核心特性

- ✅ **插件管理系统** - 完整的插件生命周期管理，支持动态加载/卸载、依赖解析、热更新
- ✅ **服务总线系统** - 统一的插件间通信机制，支持服务注册、发现和调用
- ✅ **配置管理系统** - 多级配置支持，配置热更新，配置验证
- ✅ **事件总线系统** - 发布/订阅模式的事件通信
- ✅ **日志系统** - 统一的日志管理，支持多级别日志输出
- ✅ **框架核心** - 统一的框架入口，管理所有核心组件

## 技术栈

- **编程语言**: C++17
- **GUI框架**: Qt 5.15
- **构建系统**: CMake 3.20+
- **包管理**: Conan 2.0 (可选)

## 快速开始

### 环境要求

- Qt 5.15 或更高版本
- Qt Creator 4.12 或更高版本（推荐）
- CMake 3.20 或更高版本（可选，用于CMake构建）
- C++17 兼容的编译器 (GCC 7+, Clang 5+, MSVC 2017+)

### 使用 Qt Creator 打开项目（推荐）

1. **打开 Qt Creator**
2. **选择 "文件" -> "打开文件或项目"**
3. **选择项目根目录下的 `EagleFramework.pro` 文件**
4. **配置构建套件（Kit）**
   - 选择 Qt 5.15 版本
   - 选择 C++17 编译器
5. **点击 "配置项目"**
6. **点击左下角的 "运行" 按钮即可编译和运行**

### 使用 CMake 编译（可选）

```bash
# 创建构建目录
mkdir build && cd build

# 配置CMake
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt5

# 编译
cmake --build .

# 运行示例程序
./bin/EagleApp
```

### 开发插件

1. **创建插件类**

```cpp
#include "eagle/core/IPlugin.h"

class MyPlugin : public Eagle::Core::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.eagle.framework.IPlugin/1.0")
    Q_INTERFACES(Eagle::Core::IPlugin)
    
public:
    PluginMetadata metadata() const override {
        PluginMetadata meta;
        meta.pluginId = "com.example.myplugin";
        meta.name = "我的插件";
        meta.version = "1.0.0";
        return meta;
    }
    
    bool initialize(const PluginContext& context) override {
        // 初始化逻辑
        return true;
    }
    
    void shutdown() override {
        // 清理逻辑
    }
};
```

2. **编译插件**

将插件编译为动态库（.so/.dll/.dylib），并放置在插件目录中。

3. **使用插件**

```cpp
// 获取框架实例
Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
framework->initialize();

// 加载插件
framework->pluginManager()->loadPlugin("com.example.myplugin");

// 获取插件实例
Eagle::Core::IPlugin* plugin = framework->pluginManager()->getPlugin("com.example.myplugin");
```

## 项目结构

```
.
├── include/              # 头文件
│   └── eagle/
│       └── core/        # 核心框架头文件
├── src/                 # 源代码
│   └── core/            # 核心框架实现
├── examples/             # 示例代码
│   ├── sample_plugin/   # 示例插件
│   └── main_app/        # 主程序示例
├── CMakeLists.txt       # 主CMake配置
└── README.md           # 本文档
```

## 核心模块

### PluginManager (插件管理器)

负责插件的发现、加载、卸载和生命周期管理。

```cpp
// 扫描插件
pluginManager->scanPlugins();

// 加载插件
pluginManager->loadPlugin("com.example.plugin");

// 卸载插件
pluginManager->unloadPlugin("com.example.plugin");

// 热更新
pluginManager->hotUpdatePlugin("com.example.plugin");
```

### ServiceRegistry (服务注册中心)

提供服务的注册、发现和调用功能。

```cpp
// 注册服务
ServiceDescriptor desc;
desc.serviceName = "UserService";
desc.version = "1.0.0";
serviceRegistry->registerService(desc, serviceProvider);

// 查找服务
QObject* service = serviceRegistry->findService("UserService");

// 调用服务
QVariant result = serviceRegistry->callService("UserService", "getUser", args);
```

### EventBus (事件总线)

提供发布/订阅模式的事件通信。

```cpp
// 订阅事件
eventBus->subscribe("user.login", this, SLOT(onUserLogin(QVariant)));

// 发布事件
eventBus->publish("user.login", userData);
```

### ConfigManager (配置管理器)

统一的配置管理，支持多级配置和热更新。

```cpp
// 加载配置
configManager->loadFromFile("config.json");

// 获取配置
QVariant value = configManager->get("database.host");

// 设置配置
configManager->set("database.port", 3306);

// 监听配置变更
configManager->watchConfig("database.host", this, SLOT(onConfigChanged()));
```

### Logger (日志系统)

统一的日志管理。

```cpp
// 初始化日志
Logger::initialize("./logs", LogLevel::Info);

// 记录日志
Logger::info("MyModule", "这是一条信息日志");
Logger::error("MyModule", "这是一条错误日志");
```

## 开发指南

### 插件开发规范

1. **插件必须实现 `IPlugin` 接口**
2. **插件必须提供有效的元数据**
3. **插件应该正确处理初始化和清理**
4. **插件应该通过服务总线或事件总线与其他插件通信**

### 配置管理

框架支持多级配置：
- **Global**: 全局配置，所有插件共享
- **User**: 用户配置，覆盖全局配置
- **Plugin**: 插件配置，仅对特定插件有效

### 错误处理

- 插件加载失败不会影响其他插件
- 所有错误都会记录到日志系统
- 框架提供错误信号通知

## 许可证

本项目采用 MIT 许可证。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 联系方式

如有问题，请通过以下方式联系：
- 提交 Issue
- 发送邮件

## 更新日志

### v1.0.0 (2024-01-25)
- 初始版本发布
- 实现核心插件管理系统
- 实现服务总线系统
- 实现配置管理系统
- 实现事件总线系统
- 实现日志系统
- 提供示例插件和主程序
