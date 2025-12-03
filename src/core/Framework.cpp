#include "eagle/core/Framework.h"
#include "Framework_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace Eagle {
namespace Core {

Framework* Framework::s_instance = nullptr;

Framework::Framework(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Framework::~Framework()
{
    shutdown();
    delete d;
}

Framework* Framework::instance()
{
    if (!s_instance) {
        s_instance = new Framework;
    }
    return s_instance;
}

void Framework::destroy()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool Framework::initialize(const QString& configPath)
{
    if (d->initialized) {
        Logger::warning("Framework", "框架已经初始化");
        return true;
    }
    
    Logger::info("Framework", "开始初始化框架...");
    
    // 初始化日志系统
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    Logger::initialize(logDir, LogLevel::Info);
    
    // 创建核心组件
    d->configManager = new ConfigManager(this);
    d->eventBus = new EventBus(this);
    d->serviceRegistry = new ServiceRegistry(this);
    d->pluginManager = new PluginManager(this);
    
    // PluginContext 会在加载插件时通过 PluginManager 传递
    
    // 加载配置
    QString actualConfigPath = configPath;
    if (actualConfigPath.isEmpty()) {
        actualConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/eagle.json";
    }
    
    if (QFile::exists(actualConfigPath)) {
        d->configManager->loadFromFile(actualConfigPath, ConfigManager::Global);
    } else {
        // 创建默认配置
        QVariantMap defaultConfig;
        defaultConfig["framework"] = QVariantMap({
            {"plugins", QVariantMap({
                {"enabled", true},
                {"scan_paths", QStringList({"./plugins", "../plugins"})}
            })},
            {"logging", QVariantMap({
                {"level", "info"},
                {"output", QStringList({"file", "console"})}
            })},
            {"security", QVariantMap({
                {"plugin_signature_required", false}
            })}
        });
        
        d->configManager->updateConfig(defaultConfig, ConfigManager::Global);
        
        // 保存默认配置
        QDir configDir = QFileInfo(actualConfigPath).dir();
        if (!configDir.exists()) {
            configDir.mkpath(".");
        }
        d->configManager->saveToFile(actualConfigPath, ConfigManager::Global);
    }
    
    // 从配置中读取插件路径
    QVariantMap frameworkConfig = d->configManager->get("framework").toMap();
    QVariantMap pluginsConfig = frameworkConfig["plugins"].toMap();
    if (pluginsConfig["enabled"].toBool()) {
        QStringList scanPaths = pluginsConfig["scan_paths"].toStringList();
        d->pluginManager->setPluginPaths(scanPaths);
        
        // 扫描插件
        d->pluginManager->scanPlugins();
    }
    
    // 设置安全选项
    QVariantMap securityConfig = frameworkConfig["security"].toMap();
    bool signatureRequired = securityConfig["plugin_signature_required"].toBool();
    d->pluginManager->setPluginSignatureRequired(signatureRequired);
    
    d->initialized = true;
    
    Logger::info("Framework", "框架初始化完成");
    emit initialized();
    
    return true;
}

void Framework::shutdown()
{
    if (!d->initialized) {
        return;
    }
    
    Logger::info("Framework", "开始关闭框架...");
    
    // 卸载所有插件
    if (d->pluginManager) {
        QStringList plugins = d->pluginManager->availablePlugins();
        for (const QString& pluginId : plugins) {
            if (d->pluginManager->isPluginLoaded(pluginId)) {
                d->pluginManager->unloadPlugin(pluginId);
            }
        }
    }
    
    // 清理组件
    delete d->pluginManager;
    d->pluginManager = nullptr;
    
    delete d->serviceRegistry;
    d->serviceRegistry = nullptr;
    
    delete d->eventBus;
    d->eventBus = nullptr;
    
    delete d->configManager;
    d->configManager = nullptr;
    
    Logger::shutdown();
    
    d->initialized = false;
    
    Logger::info("Framework", "框架关闭完成");
    emit shutdownCompleted();
}

PluginManager* Framework::pluginManager() const
{
    return d->pluginManager;
}

ServiceRegistry* Framework::serviceRegistry() const
{
    return d->serviceRegistry;
}

EventBus* Framework::eventBus() const
{
    return d->eventBus;
}

ConfigManager* Framework::configManager() const
{
    return d->configManager;
}

QString Framework::version() const
{
    return "1.0.0";
}

bool Framework::isInitialized() const
{
    return d->initialized;
}

} // namespace Core
} // namespace Eagle
