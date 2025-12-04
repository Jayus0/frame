#include "eagle/core/ApiServer.h"
#include "eagle/core/Framework.h"
#include "eagle/core/PluginManager.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/ConfigManager.h"
#include "eagle/core/PerformanceMonitor.h"
#include "eagle/core/AuditLog.h"
#include "eagle/core/ApiKeyManager.h"
#include "eagle/core/SessionManager.h"
#include "eagle/core/RBAC.h"
#include "eagle/core/RateLimiter.h"
#include "eagle/core/Logger.h"
#include "eagle/core/IPlugin.h"
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

/**
 * @brief 创建认证中间件
 */
Middleware createAuthMiddleware(Framework* framework) {
    return [framework](const HttpRequest& request, HttpResponse& response) -> bool {
        if (!framework) {
            response.setError(500, "Framework not available");
            return false;
        }
        
        // 跳过健康检查端点
        if (request.path == "/api/v1/health") {
            return true;
        }
        
        // 获取认证token
        QString token = request.getAuthToken();
        if (token.isEmpty()) {
            response.setError(401, "Unauthorized", "Missing authentication token");
            return false;
        }
        
        // 尝试API密钥验证
        ApiKeyManager* apiKeyManager = framework->apiKeyManager();
        if (apiKeyManager && apiKeyManager->validateKey(token)) {
            // API密钥验证成功
            return true;
        }
        
        // 尝试会话验证
        SessionManager* sessionManager = framework->sessionManager();
        if (sessionManager && sessionManager->validateSession(token)) {
            // 会话验证成功
            return true;
        }
        
        // 认证失败
        response.setError(401, "Unauthorized", "Invalid authentication token");
        return false;
    };
}

/**
 * @brief 创建权限检查中间件
 */
Middleware createPermissionMiddleware(Framework* framework) {
    return [framework](const HttpRequest& request, HttpResponse& response) -> bool {
        if (!framework) {
            return true; // 如果没有framework，跳过权限检查
        }
        
        // 从认证token中获取用户ID（需要在认证中间件中设置）
        // 这里简化处理，在路由处理器中进行权限检查
        return true;
    };
}

/**
 * @brief 创建限流中间件
 */
Middleware createRateLimitMiddleware(Framework* framework) {
    return [framework](const HttpRequest& request, HttpResponse& response) -> bool {
        if (!framework) {
            return true;
        }
        
        RateLimiter* rateLimiter = framework->rateLimiter();
        if (!rateLimiter || !rateLimiter->isEnabled()) {
            return true;
        }
        
        QString key = request.remoteAddress;
        
        // 检查限流（默认：每分钟100次请求）
        if (!rateLimiter->allowRequest(key, 100, 60000)) {
            response.setError(429, "Too Many Requests", "Rate limit exceeded");
            return false;
        }
        
        return true;
    };
}

/**
 * @brief 获取用户ID（从认证token）
 */
QString getUserIdFromRequest(Framework* framework, const HttpRequest& request) {
    QString token = request.getAuthToken();
    if (token.isEmpty()) {
        return "anonymous";
    }
    
    // 尝试API密钥
    ApiKeyManager* apiKeyManager = framework->apiKeyManager();
    if (apiKeyManager && apiKeyManager->validateKey(token)) {
        ApiKey key = apiKeyManager->getKeyByValue(token);
        return key.userId;
    }
    
    // 尝试会话
    SessionManager* sessionManager = framework->sessionManager();
    if (sessionManager && sessionManager->validateSession(token)) {
        Session session = sessionManager->getSession(token);
        return session.userId;
    }
    
    return "anonymous";
}

/**
 * @brief 注册所有API路由
 */
void registerApiRoutes(ApiServer* server) {
    Framework* framework = server->framework();
    if (!framework) {
        Logger::error("ApiRoutes", "Framework未初始化");
        return;
    }
    
    // 注册中间件
    server->use(createAuthMiddleware(framework));
    server->use(createPermissionMiddleware(framework));
    server->use(createRateLimitMiddleware(framework));
    
    // ============================================================================
    // 插件管理API
    // ============================================================================
    
    // GET /api/v1/plugins - 获取插件列表
    server->get("/api/v1/plugins", [framework](const HttpRequest& req, HttpResponse& resp) {
        // 权限检查
        QString userId = getUserIdFromRequest(framework, req);
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.read")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.read");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        QStringList pluginIds = pluginManager->availablePlugins();
        QJsonArray plugins;
        
        for (const QString& pluginId : pluginIds) {
            QJsonObject plugin;
            plugin["id"] = pluginId;
            
            PluginMetadata metadata = pluginManager->getPluginMetadata(pluginId);
            plugin["name"] = metadata.name;
            plugin["version"] = metadata.version;
            plugin["author"] = metadata.author;
            plugin["description"] = metadata.description;
            plugin["loaded"] = pluginManager->isPluginLoaded(pluginId);
            
            plugins.append(plugin);
        }
        
        QJsonObject data;
        data["plugins"] = plugins;
        data["total"] = plugins.size();
        resp.setSuccess(data);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "GET /api/v1/plugins", "plugins", AuditLevel::Info, true);
        }
    });
    
    // GET /api/v1/plugins/{id} - 获取插件详情
    server->get("/api/v1/plugins/{id}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        if (!pluginManager->availablePlugins().contains(pluginId)) {
            resp.setError(404, "Not Found", QString("Plugin not found: %1").arg(pluginId));
            return;
        }
        
        PluginMetadata metadata = pluginManager->getPluginMetadata(pluginId);
        QJsonObject plugin;
        plugin["id"] = pluginId;
        plugin["name"] = metadata.name;
        plugin["version"] = metadata.version;
        plugin["author"] = metadata.author;
        plugin["description"] = metadata.description;
        plugin["loaded"] = pluginManager->isPluginLoaded(pluginId);
        
        // 依赖信息
        QStringList dependencies = pluginManager->resolveDependencies(pluginId);
        QJsonArray deps;
        for (const QString& dep : dependencies) {
            deps.append(dep);
        }
        plugin["dependencies"] = deps;
        
        resp.setSuccess(plugin);
    });
    
    // POST /api/v1/plugins/{id}/load - 加载插件
    server->post("/api/v1/plugins/{id}/load", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.load")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.load");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        if (pluginManager->isPluginLoaded(pluginId)) {
            resp.setError(400, "Bad Request", QString("Plugin already loaded: %1").arg(pluginId));
            return;
        }
        
        bool success = pluginManager->loadPlugin(pluginId);
        if (success) {
            QJsonObject data;
            data["pluginId"] = pluginId;
            data["status"] = "loaded";
            resp.setSuccess(data);
        } else {
            resp.setError(500, "Failed to load plugin", QString("Failed to load plugin: %1").arg(pluginId));
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/plugins/{id}/load", pluginId, 
                         success ? AuditLevel::Info : AuditLevel::Warning, success);
        }
    });
    
    // DELETE /api/v1/plugins/{id} - 卸载插件
    server->delete_("/api/v1/plugins/{id}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.unload")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.unload");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        if (!pluginManager->isPluginLoaded(pluginId)) {
            resp.setError(400, "Bad Request", QString("Plugin not loaded: %1").arg(pluginId));
            return;
        }
        
        bool success = pluginManager->unloadPlugin(pluginId);
        if (success) {
            QJsonObject data;
            data["pluginId"] = pluginId;
            data["status"] = "unloaded";
            resp.setSuccess(data);
        } else {
            resp.setError(500, "Failed to unload plugin", QString("Failed to unload plugin: %1").arg(pluginId));
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "DELETE /api/v1/plugins/{id}", pluginId,
                         success ? AuditLevel::Info : AuditLevel::Warning, success);
        }
    });
    
    // ============================================================================
    // 系统管理API
    // ============================================================================
    
    // GET /api/v1/health - 健康检查
    server->get("/api/v1/health", [framework](const HttpRequest& req, HttpResponse& resp) {
        QJsonObject health;
        health["status"] = "healthy";
        health["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        // 系统资源信息
        PerformanceMonitor* monitor = framework->performanceMonitor();
        if (monitor) {
            QJsonObject system;
            system["cpuUsage"] = monitor->getCpuUsage();
            system["memoryUsageMB"] = monitor->getMemoryUsageMB();
            health["system"] = system;
        }
        
        // 插件状态
        PluginManager* pluginManager = framework->pluginManager();
        if (pluginManager) {
            QJsonObject plugins;
            plugins["total"] = pluginManager->availablePlugins().size();
            plugins["loaded"] = 0;
            for (const QString& id : pluginManager->availablePlugins()) {
                if (pluginManager->isPluginLoaded(id)) {
                    plugins["loaded"] = plugins["loaded"].toInt() + 1;
                }
            }
            health["plugins"] = plugins;
        }
        
        resp.setSuccess(health);
    });
    
    // GET /api/v1/metrics - 监控指标
    server->get("/api/v1/metrics", [framework](const HttpRequest& req, HttpResponse& resp) {
        PerformanceMonitor* monitor = framework->performanceMonitor();
        if (!monitor) {
            resp.setError(500, "PerformanceMonitor not available");
            return;
        }
        
        QJsonObject metrics;
        
        // 系统指标
        QJsonObject system;
        system["cpuUsage"] = monitor->getCpuUsage();
        system["memoryUsageMB"] = monitor->getMemoryUsageMB();
        metrics["system"] = system;
        
        // 插件指标
        PluginManager* pluginManager = framework->pluginManager();
        if (pluginManager) {
            QJsonArray plugins;
            for (const QString& pluginId : pluginManager->availablePlugins()) {
                QJsonObject plugin;
                plugin["id"] = pluginId;
                plugin["loadTime"] = monitor->getPluginLoadTime(pluginId);
                plugins.append(plugin);
            }
            metrics["plugins"] = plugins;
        }
        
        resp.setSuccess(metrics);
    });
    
    // GET /api/v1/logs - 日志查询
    server->get("/api/v1/logs", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "log.read")) {
            resp.setError(403, "Forbidden", "缺少权限: log.read");
            return;
        }
        
        AuditLogManager* auditLog = framework->auditLogManager();
        if (!auditLog) {
            resp.setError(500, "AuditLogManager not available");
            return;
        }
        
        QString queryUserId = req.queryParams.value("userId");
        QString operation = req.queryParams.value("operation");
        int limit = req.queryParams.value("limit", "100").toInt();
        
        // 使用query方法查询日志
        QList<AuditLogEntry> entries = auditLog->query(queryUserId, operation);
        
        // 如果结果太多，只取最近的limit条
        if (entries.size() > limit) {
            // 取最后limit条（最新的）
            entries = entries.mid(entries.size() - limit);
        }
        
        QJsonArray logs;
        for (const AuditLogEntry& entry : entries) {
            QJsonObject log;
            log["userId"] = entry.userId;
            log["operation"] = entry.action;  // 使用action而不是operation
            log["resource"] = entry.resource;
            log["level"] = static_cast<int>(entry.level);
            log["success"] = entry.success;
            log["timestamp"] = entry.timestamp.toString(Qt::ISODate);
            logs.append(log);
        }
        
        QJsonObject data;
        data["logs"] = logs;
        data["total"] = logs.size();
        resp.setSuccess(data);
    });
    
    // POST /api/v1/config - 配置更新
    server->post("/api/v1/config", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.write")) {
            resp.setError(403, "Forbidden", "缺少权限: config.write");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        QJsonObject jsonBody = req.jsonBody();
        if (jsonBody.isEmpty()) {
            resp.setError(400, "Bad Request", "Invalid JSON body");
            return;
        }
        
        // 转换为QVariantMap
        QVariantMap config;
        for (auto it = jsonBody.begin(); it != jsonBody.end(); ++it) {
            config[it.key()] = it.value().toVariant();
        }
        
        // 更新配置
        for (auto it = config.begin(); it != config.end(); ++it) {
            configManager->set(it.key(), it.value(), ConfigManager::Global);
        }
        
        QJsonObject data;
        data["message"] = "Configuration updated successfully";
        resp.setSuccess(data);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/config", "config", AuditLevel::Info, true);
        }
    });
}

} // namespace Core
} // namespace Eagle
