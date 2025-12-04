#include "eagle/core/Framework.h"
#include "Framework_p.h"
#include "eagle/core/Logger.h"
#include "eagle/core/ApiRoutes.h"
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>

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
    
    // 创建企业级组件
    d->rbacManager = new RBACManager(this);
    d->auditLogManager = new AuditLogManager(this);
    d->performanceMonitor = new PerformanceMonitor(this);
    d->alertSystem = new AlertSystem(d->performanceMonitor, this);
    d->rateLimiter = new RateLimiter(this);
    d->apiKeyManager = new ApiKeyManager(this);
    d->sessionManager = new SessionManager(this);
    d->apiServer = new ApiServer(this);
    
    // 配置ServiceRegistry使用RBAC和限流器
    if (d->serviceRegistry) {
        d->serviceRegistry->setPermissionCheckEnabled(true);
        d->serviceRegistry->setRateLimitEnabled(true);
    }
    
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
        // 获取可执行文件所在目录
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList defaultPaths;
        defaultPaths << appDir + "/plugins";  // 可执行文件同目录下的plugins
        defaultPaths << appDir + "/../plugins";  // 上一级目录的plugins
        defaultPaths << QDir::currentPath() + "/plugins";  // 当前工作目录的plugins
        
        QVariantMap defaultConfig;
        defaultConfig["framework"] = QVariantMap({
            {"plugins", QVariantMap({
                {"enabled", true},
                {"scan_paths", defaultPaths}
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
    
    // 初始化API服务器
    if (d->apiServer) {
        d->apiServer->setFramework(this);
        registerApiRoutes(d->apiServer);
        
        // 从配置中读取API服务器端口
        QVariantMap apiConfig = frameworkConfig["api"].toMap();
        quint16 apiPort = apiConfig.value("port", 8080).toUInt();
        bool apiEnabled = apiConfig.value("enabled", false).toBool();
        
        if (apiEnabled) {
            if (d->apiServer->start(apiPort)) {
                Logger::info("Framework", QString("API服务器已启动，端口: %1").arg(apiPort));
            } else {
                Logger::warning("Framework", "API服务器启动失败");
            }
        }
    }
    
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
    
    // 停止API服务器
    if (d->apiServer) {
        d->apiServer->stop();
    }
    
    // 清理组件
    delete d->apiServer;
    d->apiServer = nullptr;
    
    delete d->alertSystem;
    d->alertSystem = nullptr;
    
    delete d->performanceMonitor;
    d->performanceMonitor = nullptr;
    
    delete d->auditLogManager;
    d->auditLogManager = nullptr;
    
    delete d->rateLimiter;
    d->rateLimiter = nullptr;
    
    delete d->sessionManager;
    d->sessionManager = nullptr;
    
    delete d->apiKeyManager;
    d->apiKeyManager = nullptr;
    
    delete d->rbacManager;
    d->rbacManager = nullptr;
    
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

RBACManager* Framework::rbacManager() const
{
    return d->rbacManager;
}

AuditLogManager* Framework::auditLogManager() const
{
    return d->auditLogManager;
}

PerformanceMonitor* Framework::performanceMonitor() const
{
    return d->performanceMonitor;
}

AlertSystem* Framework::alertSystem() const
{
    return d->alertSystem;
}

RateLimiter* Framework::rateLimiter() const
{
    return d->rateLimiter;
}

ApiKeyManager* Framework::apiKeyManager() const
{
    return d->apiKeyManager;
}

SessionManager* Framework::sessionManager() const
{
    return d->sessionManager;
}

ApiServer* Framework::apiServer() const
{
    return d->apiServer;
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
