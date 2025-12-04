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
#include "eagle/core/BackupManager.h"
#include "eagle/core/HotReloadManager.h"
#include "eagle/core/FailoverManager.h"
#include "eagle/core/DiagnosticManager.h"
#include "eagle/core/ResourceMonitor.h"
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
        Q_UNUSED(request);
        Q_UNUSED(response);
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
        Q_UNUSED(request);
        Q_UNUSED(response);
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
        Q_UNUSED(response);
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
        Q_UNUSED(req);
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
        Q_UNUSED(req);
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
    
    // ============================================================================
    // 备份管理API
    // ============================================================================
    
    // GET /api/v1/backups - 获取备份列表
    server->get("/api/v1/backups", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "backup.list")) {
            resp.setError(403, "Forbidden", "缺少权限: backup.list");
            return;
        }
        
        BackupManager* backupManager = framework->backupManager();
        if (!backupManager) {
            resp.setError(500, "BackupManager not available");
            return;
        }
        
        QList<BackupInfo> backups = backupManager->listBackups();
        QJsonArray backupArray;
        for (const BackupInfo& info : backups) {
            QJsonObject backup;
            backup["id"] = info.id;
            backup["name"] = info.name;
            backup["type"] = static_cast<int>(info.type);
            backup["trigger"] = static_cast<int>(info.trigger);
            backup["createTime"] = info.createTime.toString(Qt::ISODate);
            backup["description"] = info.description;
            backup["size"] = info.size;
            backup["filePath"] = info.filePath;
            backupArray.append(backup);
        }
        
        QJsonObject data;
        data["backups"] = backupArray;
        data["count"] = backupArray.size();
        resp.setSuccess(data);
    });
    
    // POST /api/v1/backups - 创建备份
    server->post("/api/v1/backups", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "backup.create")) {
            resp.setError(403, "Forbidden", "缺少权限: backup.create");
            return;
        }
        
        BackupManager* backupManager = framework->backupManager();
        if (!backupManager) {
            resp.setError(500, "BackupManager not available");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        BackupType type = static_cast<BackupType>(body.value("type").toInt(0));
        QString name = body.value("name").toString();
        QString description = body.value("description").toString();
        
        QString backupId = backupManager->createBackup(type, name, description);
        if (backupId.isEmpty()) {
            resp.setError(500, "Failed to create backup");
            return;
        }
        
        BackupInfo info = backupManager->getBackupInfo(backupId);
        QJsonObject backup;
        backup["id"] = info.id;
        backup["name"] = info.name;
        backup["type"] = static_cast<int>(info.type);
        backup["createTime"] = info.createTime.toString(Qt::ISODate);
        backup["description"] = info.description;
        backup["size"] = info.size;
        
        resp.setSuccess(backup);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/backups", backupId, AuditLevel::Info, true);
        }
    });
    
    // GET /api/v1/backups/{id} - 获取备份详情
    server->get("/api/v1/backups/{id}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "backup.view")) {
            resp.setError(403, "Forbidden", "缺少权限: backup.view");
            return;
        }
        
        BackupManager* backupManager = framework->backupManager();
        if (!backupManager) {
            resp.setError(500, "BackupManager not available");
            return;
        }
        
        QString backupId = req.pathParams.value("id");
        if (backupId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing backup ID");
            return;
        }
        
        BackupInfo info = backupManager->getBackupInfo(backupId);
        if (!info.isValid()) {
            resp.setError(404, "Not Found", QString("Backup not found: %1").arg(backupId));
            return;
        }
        
        QJsonObject backup;
        backup["id"] = info.id;
        backup["name"] = info.name;
        backup["type"] = static_cast<int>(info.type);
        backup["trigger"] = static_cast<int>(info.trigger);
        backup["createTime"] = info.createTime.toString(Qt::ISODate);
        backup["description"] = info.description;
        backup["size"] = info.size;
        backup["filePath"] = info.filePath;
        
        resp.setSuccess(backup);
    });
    
    // POST /api/v1/backups/{id}/restore - 恢复备份
    server->post("/api/v1/backups/{id}/restore", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "backup.restore")) {
            resp.setError(403, "Forbidden", "缺少权限: backup.restore");
            return;
        }
        
        BackupManager* backupManager = framework->backupManager();
        if (!backupManager) {
            resp.setError(500, "BackupManager not available");
            return;
        }
        
        QString backupId = req.pathParams.value("id");
        if (backupId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing backup ID");
            return;
        }
        
        bool success = backupManager->restoreBackup(backupId, true);
        if (success) {
            QJsonObject data;
            data["backupId"] = backupId;
            data["status"] = "restored";
            resp.setSuccess(data);
        } else {
            resp.setError(500, "Failed to restore backup");
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/backups/{id}/restore", backupId, 
                         success ? AuditLevel::Info : AuditLevel::Warning, success);
        }
    });
    
    // DELETE /api/v1/backups/{id} - 删除备份
    server->delete_("/api/v1/backups/{id}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "backup.delete")) {
            resp.setError(403, "Forbidden", "缺少权限: backup.delete");
            return;
        }
        
        BackupManager* backupManager = framework->backupManager();
        if (!backupManager) {
            resp.setError(500, "BackupManager not available");
            return;
        }
        
        QString backupId = req.pathParams.value("id");
        if (backupId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing backup ID");
            return;
        }
        
        bool success = backupManager->deleteBackup(backupId);
        if (success) {
            QJsonObject data;
            data["backupId"] = backupId;
            data["status"] = "deleted";
            resp.setSuccess(data);
        } else {
            resp.setError(404, "Not Found", QString("Backup not found: %1").arg(backupId));
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "DELETE /api/v1/backups/{id}", backupId, AuditLevel::Info, success);
        }
    });
    
    // ============================================================================
    // 热重载管理API
    // ============================================================================
    
    // POST /api/v1/plugins/{id}/reload - 热重载插件
    server->post("/api/v1/plugins/{id}/reload", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.reload")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.reload");
            return;
        }
        
        HotReloadManager* hotReloadManager = framework->hotReloadManager();
        if (!hotReloadManager) {
            resp.setError(500, "HotReloadManager not available");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        bool force = body.value("force").toBool(false);
        
        HotReloadResult result = hotReloadManager->reloadPlugin(pluginId, force);
        
        if (result.success) {
            QJsonObject data;
            data["pluginId"] = pluginId;
            data["status"] = "reloaded";
            data["durationMs"] = result.durationMs;
            resp.setSuccess(data);
        } else {
            resp.setError(500, "Failed to reload plugin", result.errorMessage);
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/plugins/{id}/reload", pluginId,
                         result.success ? AuditLevel::Info : AuditLevel::Warning, result.success);
        }
    });
    
    // GET /api/v1/plugins/reloadable - 获取可重载插件列表
    server->get("/api/v1/plugins/reloadable", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.reload")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.reload");
            return;
        }
        
        HotReloadManager* hotReloadManager = framework->hotReloadManager();
        if (!hotReloadManager) {
            resp.setError(500, "HotReloadManager not available");
            return;
        }
        
        QStringList reloadablePlugins = hotReloadManager->getReloadablePlugins();
        QJsonArray pluginArray;
        for (const QString& pluginId : reloadablePlugins) {
            pluginArray.append(pluginId);
        }
        
        QJsonObject data;
        data["plugins"] = pluginArray;
        data["count"] = pluginArray.size();
        resp.setSuccess(data);
    });
    
    // ============================================================================
    // 故障转移管理API
    // ============================================================================
    
    // GET /api/v1/failover/services - 获取已注册的服务列表
    server->get("/api/v1/failover/services", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "failover.view")) {
            resp.setError(403, "Forbidden", "缺少权限: failover.view");
            return;
        }
        
        FailoverManager* failoverManager = framework->failoverManager();
        if (!failoverManager) {
            resp.setError(500, "FailoverManager not available");
            return;
        }
        
        QStringList services = failoverManager->getRegisteredServices();
        QJsonArray serviceArray;
        for (const QString& serviceName : services) {
            QJsonObject service;
            service["name"] = serviceName;
            service["status"] = static_cast<int>(failoverManager->getServiceStatus(serviceName));
            ServiceNode primary = failoverManager->getCurrentPrimary(serviceName);
            service["primaryNode"] = primary.id;
            service["enabled"] = failoverManager->isServiceEnabled(serviceName);
            serviceArray.append(service);
        }
        
        QJsonObject data;
        data["services"] = serviceArray;
        data["count"] = serviceArray.size();
        resp.setSuccess(data);
    });
    
    // POST /api/v1/failover/services/{name}/failover - 执行故障转移
    server->post("/api/v1/failover/services/{name}/failover", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "failover.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: failover.manage");
            return;
        }
        
        FailoverManager* failoverManager = framework->failoverManager();
        if (!failoverManager) {
            resp.setError(500, "FailoverManager not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing service name");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString targetNodeId = body.value("targetNodeId").toString();
        
        bool success = failoverManager->performFailover(serviceName, targetNodeId);
        
        if (success) {
            ServiceNode newPrimary = failoverManager->getCurrentPrimary(serviceName);
            QJsonObject data;
            data["serviceName"] = serviceName;
            data["primaryNode"] = newPrimary.id;
            data["status"] = "switched";
            resp.setSuccess(data);
        } else {
            resp.setError(500, "Failed to perform failover");
        }
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/failover/services/{name}/failover", serviceName,
                         success ? AuditLevel::Info : AuditLevel::Warning, success);
        }
    });
    
    // GET /api/v1/failover/services/{name}/nodes - 获取服务节点列表
    server->get("/api/v1/failover/services/{name}/nodes", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "failover.view")) {
            resp.setError(403, "Forbidden", "缺少权限: failover.view");
            return;
        }
        
        FailoverManager* failoverManager = framework->failoverManager();
        if (!failoverManager) {
            resp.setError(500, "FailoverManager not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing service name");
            return;
        }
        
        QList<ServiceNode> nodes = failoverManager->getNodes(serviceName);
        QJsonArray nodeArray;
        for (const ServiceNode& node : nodes) {
            QJsonObject nodeObj;
            nodeObj["id"] = node.id;
            nodeObj["name"] = node.name;
            nodeObj["role"] = static_cast<int>(node.role);
            nodeObj["status"] = static_cast<int>(node.status);
            nodeObj["endpoint"] = node.endpoint;
            nodeObj["lastHealthCheck"] = node.lastHealthCheck.toString(Qt::ISODate);
            nodeObj["consecutiveFailures"] = node.consecutiveFailures;
            nodeArray.append(nodeObj);
        }
        
        QJsonObject data;
        data["nodes"] = nodeArray;
        data["count"] = nodeArray.size();
        resp.setSuccess(data);
    });
    
    // GET /api/v1/failover/services/{name}/history - 获取故障转移历史
    server->get("/api/v1/failover/services/{name}/history", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "failover.view")) {
            resp.setError(403, "Forbidden", "缺少权限: failover.view");
            return;
        }
        
        FailoverManager* failoverManager = framework->failoverManager();
        if (!failoverManager) {
            resp.setError(500, "FailoverManager not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing service name");
            return;
        }
        
        int limit = req.queryParams.value("limit", "10").toInt();
        QList<FailoverEvent> events = failoverManager->getFailoverHistory(serviceName, limit);
        
        QJsonArray eventArray;
        for (const FailoverEvent& event : events) {
            QJsonObject eventObj;
            eventObj["serviceName"] = event.serviceName;
            eventObj["fromNodeId"] = event.fromNodeId;
            eventObj["toNodeId"] = event.toNodeId;
            eventObj["reason"] = event.reason;
            eventObj["timestamp"] = event.timestamp.toString(Qt::ISODate);
            eventObj["success"] = event.success;
            eventArray.append(eventObj);
        }
        
        QJsonObject data;
        data["events"] = eventArray;
        data["count"] = eventArray.size();
        resp.setSuccess(data);
    });
    
    // ============================================================================
    // 诊断工具API
    // ============================================================================
    
    // POST /api/v1/diagnostics/stacktrace - 捕获堆栈跟踪
    server->post("/api/v1/diagnostics/stacktrace", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.stacktrace")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.stacktrace");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString message = body.value("message").toString();
        
        StackTrace trace = diagnosticManager->captureStackTrace(message);
        
        QJsonObject traceObj;
        traceObj["id"] = trace.id;
        traceObj["threadId"] = trace.threadId;
        traceObj["threadName"] = trace.threadName;
        traceObj["timestamp"] = trace.timestamp.toString(Qt::ISODate);
        traceObj["message"] = trace.message;
        
        QJsonArray framesArray;
        for (const StackFrame& frame : trace.frames) {
            QJsonObject frameObj;
            frameObj["function"] = frame.function;
            frameObj["file"] = frame.file;
            frameObj["line"] = frame.line;
            frameObj["address"] = frame.address;
            frameObj["module"] = frame.module;
            framesArray.append(frameObj);
        }
        traceObj["frames"] = framesArray;
        
        resp.setSuccess(traceObj);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/diagnostics/stacktrace", trace.id, AuditLevel::Info, true);
        }
    });
    
    // GET /api/v1/diagnostics/stacktraces - 获取堆栈跟踪列表
    server->get("/api/v1/diagnostics/stacktraces", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.view")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.view");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QStringList traceIds = diagnosticManager->getStackTraceIds();
        QJsonArray traceArray;
        for (const QString& traceId : traceIds) {
            StackTrace trace = diagnosticManager->getStackTrace(traceId);
            QJsonObject traceObj;
            traceObj["id"] = trace.id;
            traceObj["threadId"] = trace.threadId;
            traceObj["threadName"] = trace.threadName;
            traceObj["timestamp"] = trace.timestamp.toString(Qt::ISODate);
            traceObj["message"] = trace.message;
            traceObj["frameCount"] = trace.frames.size();
            traceArray.append(traceObj);
        }
        
        QJsonObject data;
        data["traces"] = traceArray;
        data["count"] = traceArray.size();
        resp.setSuccess(data);
    });
    
    // GET /api/v1/diagnostics/stacktraces/{id} - 获取堆栈跟踪详情
    server->get("/api/v1/diagnostics/stacktraces/{id}", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.view")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.view");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QString traceId = req.pathParams.value("id");
        if (traceId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing trace ID");
            return;
        }
        
        StackTrace trace = diagnosticManager->getStackTrace(traceId);
        if (trace.id.isEmpty()) {
            resp.setError(404, "Not Found", QString("Stack trace not found: %1").arg(traceId));
            return;
        }
        
        QJsonObject traceObj;
        traceObj["id"] = trace.id;
        traceObj["threadId"] = trace.threadId;
        traceObj["threadName"] = trace.threadName;
        traceObj["timestamp"] = trace.timestamp.toString(Qt::ISODate);
        traceObj["message"] = trace.message;
        
        QJsonArray framesArray;
        for (const StackFrame& frame : trace.frames) {
            QJsonObject frameObj;
            frameObj["function"] = frame.function;
            frameObj["file"] = frame.file;
            frameObj["line"] = frame.line;
            frameObj["address"] = frame.address;
            frameObj["module"] = frame.module;
            framesArray.append(frameObj);
        }
        traceObj["frames"] = framesArray;
        
        resp.setSuccess(traceObj);
    });
    
    // POST /api/v1/diagnostics/memory/snapshot - 捕获内存快照
    server->post("/api/v1/diagnostics/memory/snapshot", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.memory")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.memory");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString name = body.value("name").toString();
        
        MemorySnapshot snapshot = diagnosticManager->captureMemorySnapshot(name);
        
        QJsonObject snapshotObj;
        snapshotObj["id"] = snapshot.id;
        snapshotObj["timestamp"] = snapshot.timestamp.toString(Qt::ISODate);
        snapshotObj["totalMemory"] = snapshot.totalMemory;
        snapshotObj["usedMemory"] = snapshot.usedMemory;
        snapshotObj["freeMemory"] = snapshot.freeMemory;
        snapshotObj["heapSize"] = snapshot.heapSize;
        snapshotObj["objectCount"] = snapshot.objectCount;
        snapshotObj["memoryUsagePercent"] = snapshot.memoryUsagePercent();
        snapshotObj["details"] = QJsonObject::fromVariantMap(snapshot.details);
        
        resp.setSuccess(snapshotObj);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/diagnostics/memory/snapshot", snapshot.id, AuditLevel::Info, true);
        }
    });
    
    // GET /api/v1/diagnostics/memory/snapshots - 获取内存快照列表
    server->get("/api/v1/diagnostics/memory/snapshots", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.view")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.view");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QStringList snapshotIds = diagnosticManager->getMemorySnapshotIds();
        QJsonArray snapshotArray;
        for (const QString& snapshotId : snapshotIds) {
            MemorySnapshot snapshot = diagnosticManager->getMemorySnapshot(snapshotId);
            QJsonObject snapshotObj;
            snapshotObj["id"] = snapshot.id;
            snapshotObj["timestamp"] = snapshot.timestamp.toString(Qt::ISODate);
            snapshotObj["usedMemory"] = snapshot.usedMemory;
            snapshotObj["heapSize"] = snapshot.heapSize;
            snapshotObj["memoryUsagePercent"] = snapshot.memoryUsagePercent();
            snapshotArray.append(snapshotObj);
        }
        
        QJsonObject data;
        data["snapshots"] = snapshotArray;
        data["count"] = snapshotArray.size();
        resp.setSuccess(data);
    });
    
    // POST /api/v1/diagnostics/memory/compare - 比较内存快照
    server->post("/api/v1/diagnostics/memory/compare", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.memory")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.memory");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString snapshotId1 = body.value("snapshot1").toString();
        QString snapshotId2 = body.value("snapshot2").toString();
        
        if (snapshotId1.isEmpty() || snapshotId2.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing snapshot IDs");
            return;
        }
        
        QVariantMap comparison = diagnosticManager->compareSnapshots(snapshotId1, snapshotId2);
        if (comparison.isEmpty()) {
            resp.setError(404, "Not Found", "One or both snapshots not found");
            return;
        }
        
        resp.setSuccess(QJsonObject::fromVariantMap(comparison));
    });
    
    // GET /api/v1/diagnostics/deadlocks - 获取死锁列表
    server->get("/api/v1/diagnostics/deadlocks", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "diagnostic.view")) {
            resp.setError(403, "Forbidden", "缺少权限: diagnostic.view");
            return;
        }
        
        DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            resp.setError(500, "DiagnosticManager not available");
            return;
        }
        
        QList<DeadlockInfo> deadlocks = diagnosticManager->getDeadlocks();
        QJsonArray deadlockArray;
        for (const DeadlockInfo& deadlock : deadlocks) {
            QJsonObject deadlockObj;
            deadlockObj["id"] = deadlock.id;
            deadlockObj["timestamp"] = deadlock.timestamp.toString(Qt::ISODate);
            deadlockObj["threadIds"] = QJsonArray::fromStringList(deadlock.threadIds);
            deadlockObj["lockIds"] = QJsonArray::fromStringList(deadlock.lockIds);
            deadlockObj["description"] = deadlock.description;
            deadlockArray.append(deadlockObj);
        }
        
        QJsonObject data;
        data["deadlocks"] = deadlockArray;
        data["count"] = deadlockArray.size();
        resp.setSuccess(data);
    });
    
    // ============================================================================
    // 资源监控API
    // ============================================================================
    
    // GET /api/v1/plugins/{id}/resources - 获取插件资源使用情况
    server->get("/api/v1/plugins/{id}/resources", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.resource.view")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.resource.view");
            return;
        }
        
        ResourceMonitor* resourceMonitor = framework->resourceMonitor();
        if (!resourceMonitor) {
            resp.setError(500, "ResourceMonitor not available");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        ResourceUsage usage = resourceMonitor->getResourceUsage(pluginId);
        ResourceLimits limits = resourceMonitor->getResourceLimits(pluginId);
        
        QJsonObject data;
        data["pluginId"] = pluginId;
        
        QJsonObject usageObj;
        usageObj["memoryBytes"] = usage.memoryBytes;
        usageObj["memoryMB"] = usage.memoryBytes / 1024 / 1024;
        usageObj["cpuPercent"] = usage.cpuPercent;
        usageObj["threadCount"] = usage.threadCount;
        usageObj["lastUpdate"] = usage.lastUpdate.toString(Qt::ISODate);
        data["usage"] = usageObj;
        
        QJsonObject limitsObj;
        limitsObj["maxMemoryMB"] = limits.maxMemoryMB;
        limitsObj["maxCpuPercent"] = limits.maxCpuPercent;
        limitsObj["maxThreads"] = limits.maxThreads;
        limitsObj["enforceLimits"] = limits.enforceLimits;
        data["limits"] = limitsObj;
        
        data["limitExceeded"] = resourceMonitor->isResourceLimitExceeded(pluginId);
        
        resp.setSuccess(data);
    });
    
    // POST /api/v1/plugins/{id}/resources/limits - 设置插件资源限制
    server->post("/api/v1/plugins/{id}/resources/limits", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.resource.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.resource.manage");
            return;
        }
        
        ResourceMonitor* resourceMonitor = framework->resourceMonitor();
        if (!resourceMonitor) {
            resp.setError(500, "ResourceMonitor not available");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        ResourceLimits limits;
        limits.maxMemoryMB = body.value("maxMemoryMB").toInt(-1);
        limits.maxCpuPercent = body.value("maxCpuPercent").toInt(-1);
        limits.maxThreads = body.value("maxThreads").toInt(-1);
        limits.enforceLimits = body.value("enforceLimits").toBool(false);
        
        resourceMonitor->setResourceLimits(pluginId, limits);
        
        QJsonObject data;
        data["pluginId"] = pluginId;
        data["limits"] = QJsonObject::fromVariantMap(QVariantMap({
            {"maxMemoryMB", limits.maxMemoryMB},
            {"maxCpuPercent", limits.maxCpuPercent},
            {"maxThreads", limits.maxThreads},
            {"enforceLimits", limits.enforceLimits}
        }));
        
        resp.setSuccess(data);
        
        // 审计日志
        AuditLogManager* auditLog = framework->auditLogManager();
        if (auditLog) {
            auditLog->log(userId, "POST /api/v1/plugins/{id}/resources/limits", pluginId, AuditLevel::Info, true);
        }
    });
    
    // GET /api/v1/plugins/{id}/resources/events - 获取资源超限事件
    server->get("/api/v1/plugins/{id}/resources/events", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.resource.view")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.resource.view");
            return;
        }
        
        ResourceMonitor* resourceMonitor = framework->resourceMonitor();
        if (!resourceMonitor) {
            resp.setError(500, "ResourceMonitor not available");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Bad Request", "Missing plugin ID");
            return;
        }
        
        int limit = req.queryParams.value("limit", "10").toInt();
        QList<ResourceLimitExceeded> events = resourceMonitor->getLimitExceededEvents(pluginId, limit);
        
        QJsonArray eventArray;
        for (const ResourceLimitExceeded& event : events) {
            QJsonObject eventObj;
            eventObj["pluginId"] = event.pluginId;
            eventObj["resourceType"] = event.resourceType;
            eventObj["currentValue"] = event.currentValue;
            eventObj["limitValue"] = event.limitValue;
            eventObj["timestamp"] = event.timestamp.toString(Qt::ISODate);
            eventArray.append(eventObj);
        }
        
        QJsonObject data;
        data["events"] = eventArray;
        data["count"] = eventArray.size();
        resp.setSuccess(data);
    });
}

} // namespace Core
} // namespace Eagle
