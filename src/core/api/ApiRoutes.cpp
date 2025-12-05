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
#include "eagle/core/ConfigEncryption.h"
#include "eagle/core/ConfigSchema.h"
#include "eagle/core/ConfigVersion.h"
#include "eagle/core/ConfigFormat.h"
#include "eagle/core/PluginSignature.h"
#include "eagle/core/LoadBalancer.h"
#include "eagle/core/AsyncServiceCall.h"
#include "eagle/core/SslConfig.h"
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
    
    // POST /api/v1/config/load - 从文件加载配置（支持多种格式）
    server->post("/api/v1/config/load", [framework](const HttpRequest& req, HttpResponse& resp) {
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
        
        QJsonObject body = req.jsonBody();
        QString filePath = body.value("filePath").toString();
        QString formatStr = body.value("format").toString("auto").toLower();
        
        if (filePath.isEmpty()) {
            resp.setError(400, "Bad Request", "filePath is required");
            return;
        }
        
        ConfigFormat format = ConfigFormat::JSON;
        if (formatStr == "yaml" || formatStr == "yml") {
            format = ConfigFormat::YAML;
        } else if (formatStr == "ini" || formatStr == "conf" || formatStr == "cfg") {
            format = ConfigFormat::INI;
        } else if (formatStr != "auto" && formatStr != "json") {
            resp.setError(400, "Bad Request", QString("Unsupported format: %1").arg(formatStr));
            return;
        }
        
        bool success = configManager->loadFromFile(filePath, ConfigManager::Global, format);
        
        if (success) {
            QJsonObject result;
            result["message"] = QString("配置加载成功: %1").arg(filePath);
            QString detectedFormat = formatStr == "auto" ? 
                (ConfigFormatParser::formatFromExtension(filePath) == ConfigFormat::JSON ? "json" :
                 ConfigFormatParser::formatFromExtension(filePath) == ConfigFormat::YAML ? "yaml" : "ini") : formatStr;
            result["format"] = detectedFormat;
            resp.setSuccess(result);
        } else {
            resp.setError(500, "Internal Server Error", QString("配置加载失败: %1").arg(filePath));
        }
    });
    
    // POST /api/v1/config/save - 保存配置到文件（支持多种格式）
    server->post("/api/v1/config/save", [framework](const HttpRequest& req, HttpResponse& resp) {
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
        
        QJsonObject body = req.jsonBody();
        QString filePath = body.value("filePath").toString();
        QString formatStr = body.value("format").toString("auto").toLower();
        
        if (filePath.isEmpty()) {
            resp.setError(400, "Bad Request", "filePath is required");
            return;
        }
        
        ConfigFormat format = ConfigFormat::JSON;
        if (formatStr == "yaml" || formatStr == "yml") {
            format = ConfigFormat::YAML;
        } else if (formatStr == "ini" || formatStr == "conf" || formatStr == "cfg") {
            format = ConfigFormat::INI;
        } else if (formatStr != "auto" && formatStr != "json") {
            resp.setError(400, "Bad Request", QString("Unsupported format: %1").arg(formatStr));
            return;
        }
        
        bool success = configManager->saveToFile(filePath, ConfigManager::Global, format);
        
        if (success) {
            QJsonObject result;
            result["message"] = QString("配置保存成功: %1").arg(filePath);
            QString detectedFormat = formatStr == "auto" ? 
                (ConfigFormatParser::formatFromExtension(filePath) == ConfigFormat::JSON ? "json" :
                 ConfigFormatParser::formatFromExtension(filePath) == ConfigFormat::YAML ? "yaml" : "ini") : formatStr;
            result["format"] = detectedFormat;
            resp.setSuccess(result);
        } else {
            resp.setError(500, "Internal Server Error", QString("配置保存失败: %1").arg(filePath));
        }
    });
    
    // POST /api/v1/config/convert - 格式转换
    server->post("/api/v1/config/convert", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.read")) {
            resp.setError(403, "Forbidden", "缺少权限: config.read");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString content = body.value("content").toString();
        QString sourceFormatStr = body.value("sourceFormat").toString("json").toLower();
        QString targetFormatStr = body.value("targetFormat").toString("json").toLower();
        
        if (content.isEmpty()) {
            resp.setError(400, "Bad Request", "content is required");
            return;
        }
        
        ConfigFormat sourceFormat = ConfigFormat::JSON;
        if (sourceFormatStr == "yaml" || sourceFormatStr == "yml") {
            sourceFormat = ConfigFormat::YAML;
        } else if (sourceFormatStr == "ini" || sourceFormatStr == "conf" || sourceFormatStr == "cfg") {
            sourceFormat = ConfigFormat::INI;
        }
        
        ConfigFormat targetFormat = ConfigFormat::JSON;
        if (targetFormatStr == "yaml" || targetFormatStr == "yml") {
            targetFormat = ConfigFormat::YAML;
        } else if (targetFormatStr == "ini" || targetFormatStr == "conf" || targetFormatStr == "cfg") {
            targetFormat = ConfigFormat::INI;
        }
        
        QByteArray converted = ConfigFormatParser::convertFormat(content.toUtf8(), sourceFormat, targetFormat);
        
        QJsonObject result;
        result["content"] = QString::fromUtf8(converted);
        result["sourceFormat"] = sourceFormatStr;
        result["targetFormat"] = targetFormatStr;
        resp.setSuccess(result);
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
    
    // GET /api/v1/plugins/{id}/dependencies - 获取插件依赖关系
    server->get("/api/v1/plugins/{id}/dependencies", [framework](const HttpRequest& req, HttpResponse& resp) {
        if (!framework) {
            resp.setError(500, "Framework not available");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        QString pluginId = req.pathParams.value("id");
        if (pluginId.isEmpty()) {
            resp.setError(400, "Plugin ID is required");
            return;
        }
        
        PluginMetadata meta = pluginManager->getPluginMetadata(pluginId);
        if (!meta.isValid()) {
            resp.setError(404, "Plugin not found");
            return;
        }
        
        QStringList allDeps = pluginManager->resolveDependencies(pluginId);
        QStringList directDeps = meta.dependencies;
        
        QJsonObject result;
        result["pluginId"] = pluginId;
        result["directDependencies"] = QJsonArray::fromStringList(directDeps);
        result["allDependencies"] = QJsonArray::fromStringList(allDeps);
        
        // 检查循环依赖
        QStringList cyclePath;
        bool hasCycle = pluginManager->detectCircularDependencies(pluginId, cyclePath);
        result["hasCircularDependency"] = hasCycle;
        if (hasCycle) {
            result["circularPath"] = QJsonArray::fromStringList(cyclePath);
        }
        
        resp.setJson(result);
    });
    
    // GET /api/v1/plugins/dependencies/circular - 获取所有循环依赖
    server->get("/api/v1/plugins/dependencies/circular", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        if (!framework) {
            resp.setError(500, "Framework not available");
            return;
        }
        
        PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            resp.setError(500, "PluginManager not available");
            return;
        }
        
        QList<QStringList> allCycles = pluginManager->detectAllCircularDependencies();
        
        QJsonArray cyclesArray;
        for (const QStringList& cycle : allCycles) {
            cyclesArray.append(QJsonArray::fromStringList(cycle));
        }
        
        QJsonObject result;
        result["hasCircularDependencies"] = !allCycles.isEmpty();
        result["cycles"] = cyclesArray;
        result["count"] = allCycles.size();
        
        resp.setJson(result);
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
    
    // GET /api/v1/config/encryption/key - 获取加密密钥信息
    server->get("/api/v1/config/encryption/key", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.encryption.view")) {
            resp.setError(403, "Forbidden", "缺少权限: config.encryption.view");
            return;
        }
        
        Core::KeyVersion version = ConfigEncryption::getCurrentKeyVersion();
        
        QJsonObject result;
        result["version"] = version.version;
        result["algorithm"] = (version.algorithm == Core::EncryptionAlgorithm::AES256 ? "AES256" : "XOR");
        result["keyId"] = version.keyId;
        result["pbkdf2Iterations"] = version.pbkdf2Iterations;
        result["hasSalt"] = !version.salt.isEmpty();
        
        resp.setSuccess(result);
    });
    
    // POST /api/v1/config/encryption/rotate - 轮换加密密钥
    server->post("/api/v1/config/encryption/rotate", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.encryption.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: config.encryption.manage");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString newKey = body.value("newKey").toString();
        QString oldKey = body.value("oldKey").toString();
        
        if (newKey.isEmpty()) {
            resp.setError(400, "Bad Request", "newKey is required");
            return;
        }
        
            // 如果提供了oldKey，执行密钥轮换
            if (!oldKey.isEmpty()) {
                // 这里需要遍历所有配置并重新加密（简化实现）
                ConfigManager* configManager = framework->configManager();
                if (configManager) {
                    // 获取所有配置
                    QVariantMap allConfig = configManager->getAll();
                    QStringList keys = allConfig.keys();
                    int rotated = 0;
                    
                    for (const QString& key : keys) {
                        QVariant value = allConfig.value(key);
                        if (value.type() == QVariant::String) {
                            QString strValue = value.toString();
                            if (strValue.startsWith("ENC:")) {
                                QString encrypted = strValue.mid(4);
                                QString rotatedValue = ConfigEncryption::rotateKey(encrypted, oldKey, newKey);
                                if (!rotatedValue.isEmpty()) {
                                    configManager->set(key, QString("ENC:%1").arg(rotatedValue));
                                    rotated++;
                                }
                            }
                        }
                    }
                    
                    QJsonObject result;
                    result["rotated"] = rotated;
                    result["message"] = QString("密钥轮换完成，已更新 %1 个配置项").arg(rotated);
                    resp.setSuccess(result);
                } else {
                    resp.setError(500, "ConfigManager not available");
                }
            } else {
            // 仅设置新密钥（不执行轮换）
            ConfigEncryption::setDefaultKey(newKey);
            
            QJsonObject result;
            result["message"] = "新密钥已设置";
            resp.setSuccess(result);
        }
    });
    
    // POST /api/v1/config/encryption/generate - 生成新密钥
    server->post("/api/v1/config/encryption/generate", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.encryption.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: config.encryption.manage");
            return;
        }
        
        int length = req.jsonBody().value("length").toInt(32);
        if (length < 16 || length > 64) {
            resp.setError(400, "Bad Request", "密钥长度必须在16-64字节之间");
            return;
        }
        
        QString newKey = ConfigEncryption::generateKey(length);
        
        QJsonObject result;
        result["key"] = newKey;
        result["length"] = length;
        result["message"] = "新密钥已生成";
        resp.setSuccess(result);
    });
    
    // POST /api/v1/config/validate - 验证配置
    server->post("/api/v1/config/validate", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.validate")) {
            resp.setError(403, "Forbidden", "缺少权限: config.validate");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QVariantMap config = body.value("config").toObject().toVariantMap();
        QString schemaPath = body.value("schemaPath").toString();
        
        if (config.isEmpty()) {
            resp.setError(400, "Bad Request", "config is required");
            return;
        }
        
        if (schemaPath.isEmpty()) {
            ConfigManager* configManager = framework->configManager();
            if (configManager) {
                schemaPath = configManager->schemaPath();
            }
            if (schemaPath.isEmpty()) {
                resp.setError(400, "Bad Request", "schemaPath is required");
                return;
            }
        }
        
        ConfigSchema schema;
        if (!schema.loadFromFile(schemaPath)) {
            resp.setError(400, "Bad Request", QString("无法加载Schema文件: %1").arg(schemaPath));
            return;
        }
        
        SchemaValidationResult result = schema.validate(config);
        
        QJsonObject response;
        response["valid"] = result.valid;
        
        QJsonArray errorsArray;
        for (const SchemaValidationError& error : result.errors) {
            QJsonObject errorObj;
            errorObj["path"] = error.path;
            errorObj["message"] = error.message;
            errorObj["code"] = error.code;
            errorsArray.append(errorObj);
        }
        response["errors"] = errorsArray;
        response["errorCount"] = result.errors.size();
        
        if (result.valid) {
            resp.setSuccess(response);
        } else {
            // 设置错误响应，包含验证详情
            response["error"] = true;
            response["code"] = 400;
            response["message"] = "Validation Failed";
            response["details"] = "配置验证失败";
            resp.statusCode = 400;
            resp.setJson(response);
        }
    });
    
    // POST /api/v1/config/schema - 加载Schema
    server->post("/api/v1/config/schema", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.schema.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: config.schema.manage");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString schemaPath = body.value("schemaPath").toString();
        
        if (schemaPath.isEmpty()) {
            resp.setError(400, "Bad Request", "schemaPath is required");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        configManager->setSchemaPath(schemaPath);
        
        QJsonObject result;
        result["schemaPath"] = schemaPath;
        result["message"] = "Schema路径已设置";
        resp.setSuccess(result);
    });
    
    // GET /api/v1/config/schema - 获取当前Schema信息
    server->get("/api/v1/config/schema", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.schema.view")) {
            resp.setError(403, "Forbidden", "缺少权限: config.schema.view");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        QString schemaPath = configManager->schemaPath();
        
        QJsonObject result;
        result["schemaPath"] = schemaPath;
        
        if (!schemaPath.isEmpty()) {
            ConfigSchema schema;
            if (schema.loadFromFile(schemaPath)) {
                result["valid"] = true;
                result["title"] = schema.title();
                result["description"] = schema.description();
            } else {
                result["valid"] = false;
                result["error"] = "无法加载Schema文件";
            }
        } else {
            result["valid"] = false;
            result["message"] = "未设置Schema路径";
        }
        
        resp.setSuccess(result);
    });
    
    // POST /api/v1/plugins/sign - 为插件生成签名
    server->post("/api/v1/plugins/sign", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.sign")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.sign");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString pluginPath = body.value("pluginPath").toString();
        QString privateKeyPath = body.value("privateKeyPath").toString();
        QString certificatePath = body.value("certificatePath").toString();
        QString outputPath = body.value("outputPath").toString();
        QString algorithmStr = body.value("algorithm").toString("RSA-SHA256");
        
        if (pluginPath.isEmpty() || privateKeyPath.isEmpty() || outputPath.isEmpty()) {
            resp.setError(400, "Bad Request", "pluginPath, privateKeyPath, and outputPath are required");
            return;
        }
        
        Core::SignatureAlgorithm algorithm = Core::SignatureAlgorithm::RSA_SHA256;
        if (algorithmStr == "RSA-SHA512") {
            algorithm = Core::SignatureAlgorithm::RSA_SHA512;
        } else if (algorithmStr == "SHA256") {
            algorithm = Core::SignatureAlgorithm::SHA256;
        }
        
        bool success = Core::PluginSignatureVerifier::sign(pluginPath, privateKeyPath, certificatePath, outputPath, algorithm);
        
        if (success) {
            QJsonObject result;
            result["message"] = "插件签名成功";
            result["outputPath"] = outputPath;
            result["algorithm"] = algorithmStr;
            resp.setSuccess(result);
        } else {
            resp.setError(500, "签名失败", "无法生成插件签名");
        }
    });
    
    // POST /api/v1/plugins/verify - 验证插件签名
    server->post("/api/v1/plugins/verify", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.verify")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.verify");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString pluginPath = body.value("pluginPath").toString();
        QString signaturePath = body.value("signaturePath").toString();
        QString crlPath = body.value("crlPath").toString();  // 撤销列表路径
        
        if (pluginPath.isEmpty()) {
            resp.setError(400, "Bad Request", "pluginPath is required");
            return;
        }
        
        if (signaturePath.isEmpty()) {
            signaturePath = Core::PluginSignatureVerifier::findSignatureFile(pluginPath);
            if (signaturePath.isEmpty()) {
                resp.setError(400, "Bad Request", "签名文件不存在");
                return;
            }
        }
        
        Core::PluginSignature signature = Core::PluginSignatureVerifier::loadFromFile(signaturePath);
        if (!signature.isValid()) {
            resp.setError(400, "Bad Request", "无法加载签名文件");
            return;
        }
        
        // 检查撤销列表
        if (!crlPath.isEmpty() && Core::PluginSignatureVerifier::isRevoked(signature, crlPath)) {
            resp.setError(400, "Signature Revoked", "签名已被撤销");
            return;
        }
        
        bool valid = Core::PluginSignatureVerifier::verify(pluginPath, signature);
        
        QJsonObject result;
        result["valid"] = valid;
        result["signer"] = signature.signer;
        result["algorithm"] = (signature.algorithm == Core::SignatureAlgorithm::RSA_SHA256 ? "RSA-SHA256" :
                              signature.algorithm == Core::SignatureAlgorithm::RSA_SHA512 ? "RSA-SHA512" : "SHA256");
        result["signTime"] = signature.signTime.toString(Qt::ISODate);
        
        if (signature.certificate.isValid()) {
            QJsonObject certObj;
            certObj["subject"] = signature.certificate.subject;
            certObj["issuer"] = signature.certificate.issuer;
            certObj["validFrom"] = signature.certificate.validFrom.toString(Qt::ISODate);
            certObj["validTo"] = signature.certificate.validTo.toString(Qt::ISODate);
            certObj["serialNumber"] = signature.certificate.serialNumber;
            result["certificate"] = certObj;
        }
        
        if (valid) {
            resp.setSuccess(result);
        } else {
            // 设置错误响应，包含验证详情
            result["error"] = true;
            result["code"] = 400;
            result["message"] = "Verification Failed";
            result["details"] = "签名验证失败";
            resp.statusCode = 400;
            resp.setJson(result);
        }
    });
    
    // GET /api/v1/plugins/certificates/trusted - 获取受信任的根证书列表
    server->get("/api/v1/plugins/certificates/trusted", [framework](const HttpRequest& req, HttpResponse& resp) {
        Q_UNUSED(req);
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.certificate.view")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.certificate.view");
            return;
        }
        
        QStringList trustedRoots = Core::PluginSignatureVerifier::getTrustedRootCertificates();
        
        QJsonArray rootsArray;
        for (const QString& rootPath : trustedRoots) {
            Core::CertificateInfo cert = Core::PluginSignatureVerifier::loadCertificate(rootPath);
            QJsonObject certObj;
            certObj["path"] = rootPath;
            certObj["subject"] = cert.subject;
            certObj["issuer"] = cert.issuer;
            certObj["valid"] = cert.isValid();
            rootsArray.append(certObj);
        }
        
        QJsonObject result;
        result["trustedRoots"] = rootsArray;
        result["count"] = rootsArray.size();
        resp.setSuccess(result);
    });
    
    // POST /api/v1/plugins/certificates/trusted - 设置受信任的根证书
    server->post("/api/v1/plugins/certificates/trusted", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "plugin.certificate.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: plugin.certificate.manage");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QJsonArray rootsArray = body.value("trustedRoots").toArray();
        
        QStringList rootPaths;
        for (const QJsonValue& val : rootsArray) {
            rootPaths.append(val.toString());
        }
        
        Core::PluginSignatureVerifier::setTrustedRootCertificates(rootPaths);
        
        QJsonObject result;
        result["message"] = "受信任的根证书已设置";
        result["count"] = rootPaths.size();
        resp.setSuccess(result);
    });
    
    // GET /api/v1/services/{name}/loadbalance - 获取服务负载均衡配置
    server->get("/api/v1/services/{name}/loadbalance", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.loadbalance.view")) {
            resp.setError(403, "Forbidden", "缺少权限: service.loadbalance.view");
            return;
        }
        
        ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            resp.setError(500, "ServiceRegistry not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Service name is required");
            return;
        }
        
        LoadBalancer* loadBalancer = serviceRegistry->loadBalancer();
        if (!loadBalancer) {
            resp.setError(500, "LoadBalancer not available");
            return;
        }
        
        QString algorithm = serviceRegistry->getLoadBalanceAlgorithm(serviceName);
        QList<ServiceInstance> instances = loadBalancer->getInstances(serviceName);
        
        QJsonArray instancesArray;
        for (const ServiceInstance& instance : instances) {
            QJsonObject instanceObj;
            QString instanceId = loadBalancer->getInstanceIdByProvider(serviceName, instance.provider);
            instanceObj["instanceId"] = instanceId;
            instanceObj["version"] = instance.descriptor.version;
            instanceObj["weight"] = instance.weight;
            instanceObj["activeConnections"] = instance.activeConnections;
            instanceObj["totalRequests"] = instance.totalRequests;
            instanceObj["healthy"] = instance.healthy;
            instancesArray.append(instanceObj);
        }
        
        QJsonObject result;
        result["serviceName"] = serviceName;
        result["algorithm"] = algorithm;
        result["enabled"] = serviceRegistry->isLoadBalanceEnabled();
        result["instances"] = instancesArray;
        result["instanceCount"] = instances.size();
        
        resp.setSuccess(result);
    });
    
    // POST /api/v1/services/{name}/loadbalance - 配置服务负载均衡
    server->post("/api/v1/services/{name}/loadbalance", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.loadbalance.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: service.loadbalance.manage");
            return;
        }
        
        ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            resp.setError(500, "ServiceRegistry not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Service name is required");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString algorithm = body.value("algorithm").toString();
        bool enabled = body.value("enabled").toBool(true);
        
        if (!algorithm.isEmpty()) {
            serviceRegistry->setLoadBalanceAlgorithm(serviceName, algorithm);
        }
        
        serviceRegistry->setLoadBalanceEnabled(enabled);
        
        QJsonObject result;
        result["serviceName"] = serviceName;
        result["algorithm"] = serviceRegistry->getLoadBalanceAlgorithm(serviceName);
        result["enabled"] = serviceRegistry->isLoadBalanceEnabled();
        result["message"] = "负载均衡配置已更新";
        resp.setSuccess(result);
    });
    
    // POST /api/v1/services/{name}/instances/{instanceId}/weight - 设置实例权重
    server->post("/api/v1/services/{name}/instances/{instanceId}/weight", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.loadbalance.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: service.loadbalance.manage");
            return;
        }
        
        ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            resp.setError(500, "ServiceRegistry not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        QString instanceId = req.pathParams.value("instanceId");
        
        QJsonObject body = req.jsonBody();
        int weight = body.value("weight").toInt(1);
        
        if (weight < 1) {
            resp.setError(400, "Bad Request", "Weight must be >= 1");
            return;
        }
        
        LoadBalancer* loadBalancer = serviceRegistry->loadBalancer();
        if (!loadBalancer) {
            resp.setError(500, "LoadBalancer not available");
            return;
        }
        
        loadBalancer->setInstanceWeight(serviceName, instanceId, weight);
        
        QJsonObject result;
        result["serviceName"] = serviceName;
        result["instanceId"] = instanceId;
        result["weight"] = weight;
        result["message"] = "实例权重已更新";
        resp.setSuccess(result);
    });
    
    // POST /api/v1/services/{name}/instances/{instanceId}/health - 设置实例健康状态
    server->post("/api/v1/services/{name}/instances/{instanceId}/health", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.loadbalance.manage")) {
            resp.setError(403, "Forbidden", "缺少权限: service.loadbalance.manage");
            return;
        }
        
        ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            resp.setError(500, "ServiceRegistry not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        QString instanceId = req.pathParams.value("instanceId");
        
        QJsonObject body = req.jsonBody();
        bool healthy = body.value("healthy").toBool(true);
        
        LoadBalancer* loadBalancer = serviceRegistry->loadBalancer();
        if (!loadBalancer) {
            resp.setError(500, "LoadBalancer not available");
            return;
        }
        
        loadBalancer->setInstanceHealth(serviceName, instanceId, healthy);
        
        QJsonObject result;
        result["serviceName"] = serviceName;
        result["instanceId"] = instanceId;
        result["healthy"] = healthy;
        result["message"] = QString("实例健康状态已更新为: %1").arg(healthy ? "健康" : "不健康");
        resp.setSuccess(result);
    });
    
    // GET /api/v1/config/versions - 获取配置版本列表
    server->get("/api/v1/config/versions", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.version.view")) {
            resp.setError(403, "Forbidden", "缺少权限: config.version.view");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            resp.setError(400, "Bad Request", "配置版本管理未启用");
            return;
        }
        
        int limit = req.queryParams.value("limit").toInt();
        
        QList<ConfigVersion> versions = configManager->getConfigVersions(limit);
        
        QJsonArray versionsArray;
        for (const ConfigVersion& version : versions) {
            QJsonObject versionObj;
            versionObj["version"] = version.version;
            versionObj["timestamp"] = version.timestamp.toString(Qt::ISODate);
            versionObj["author"] = version.author;
            versionObj["description"] = version.description;
            versionObj["configHash"] = version.configHash;
            versionsArray.append(versionObj);
        }
        
        QJsonObject result;
        result["versions"] = versionsArray;
        result["currentVersion"] = versionManager->currentVersion();
        result["totalCount"] = versions.size();
        
        resp.setSuccess(result);
    });
    
    // GET /api/v1/config/versions/{version} - 获取指定版本
    server->get("/api/v1/config/versions/{version}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.version.view")) {
            resp.setError(403, "Forbidden", "缺少权限: config.version.view");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            resp.setError(400, "Bad Request", "配置版本管理未启用");
            return;
        }
        
        QString versionStr = req.pathParams.value("version");
        bool ok;
        int version = versionStr.toInt(&ok);
        if (!ok) {
            resp.setError(400, "Bad Request", "无效的版本号");
            return;
        }
        
        ConfigVersion versionObj = configManager->getConfigVersion(version);
        if (!versionObj.isValid()) {
            resp.setError(404, "Not Found", QString("版本 %1 不存在").arg(version));
            return;
        }
        
        QJsonObject result;
        result["version"] = versionObj.version;
        result["timestamp"] = versionObj.timestamp.toString(Qt::ISODate);
        result["author"] = versionObj.author;
        result["description"] = versionObj.description;
        result["configHash"] = versionObj.configHash;
        result["config"] = QJsonObject::fromVariantMap(versionObj.config);
        
        resp.setSuccess(result);
    });
    
    // POST /api/v1/config/versions - 创建新版本
    server->post("/api/v1/config/versions", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.version.create")) {
            resp.setError(403, "Forbidden", "缺少权限: config.version.create");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            resp.setError(400, "Bad Request", "配置版本管理未启用");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString author = body.value("author").toString(userId);
        QString description = body.value("description").toString();
        int version = body.value("version").toInt(0);
        
        int newVersion = configManager->createConfigVersion(author, description);
        if (newVersion <= 0) {
            resp.setError(500, "Internal Server Error", "创建版本失败");
            return;
        }
        
        QJsonObject result;
        result["version"] = newVersion;
        result["message"] = "配置版本创建成功";
        resp.setSuccess(result);
    });
    
    // POST /api/v1/config/versions/{version}/rollback - 回滚到指定版本
    server->post("/api/v1/config/versions/{version}/rollback", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.version.rollback")) {
            resp.setError(403, "Forbidden", "缺少权限: config.version.rollback");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            resp.setError(400, "Bad Request", "配置版本管理未启用");
            return;
        }
        
        QString versionStr = req.pathParams.value("version");
        bool ok;
        int version = versionStr.toInt(&ok);
        if (!ok) {
            resp.setError(400, "Bad Request", "无效的版本号");
            return;
        }
        
        if (!configManager->rollbackConfig(version)) {
            resp.setError(500, "Internal Server Error", "回滚失败");
            return;
        }
        
        QJsonObject result;
        result["version"] = version;
        result["message"] = QString("配置已回滚到版本 %1").arg(version);
        resp.setSuccess(result);
    });
    
    // GET /api/v1/config/versions/{version1}/compare/{version2} - 对比两个版本
    server->get("/api/v1/config/versions/{version1}/compare/{version2}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "config.version.view")) {
            resp.setError(403, "Forbidden", "缺少权限: config.version.view");
            return;
        }
        
        ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            resp.setError(500, "ConfigManager not available");
            return;
        }
        
        ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            resp.setError(400, "Bad Request", "配置版本管理未启用");
            return;
        }
        
        QString version1Str = req.pathParams.value("version1");
        QString version2Str = req.pathParams.value("version2");
        bool ok1, ok2;
        int version1 = version1Str.toInt(&ok1);
        int version2 = version2Str.toInt(&ok2);
        
        if (!ok1 || !ok2) {
            resp.setError(400, "Bad Request", "无效的版本号");
            return;
        }
        
        QList<ConfigDiff> diffs = configManager->compareConfigVersions(version1, version2);
        
        QJsonArray diffsArray;
        for (const ConfigDiff& diff : diffs) {
            QJsonObject diffObj;
            diffObj["key"] = diff.key;
            diffObj["changeType"] = diff.changeType;
            if (diff.oldValue.isValid()) {
                diffObj["oldValue"] = QJsonValue::fromVariant(diff.oldValue);
            }
            if (diff.newValue.isValid()) {
                diffObj["newValue"] = QJsonValue::fromVariant(diff.newValue);
            }
            diffsArray.append(diffObj);
        }
        
        QJsonObject result;
        result["version1"] = version1;
        result["version2"] = version2;
        result["diffs"] = diffsArray;
        result["diffCount"] = diffs.size();
        
        resp.setSuccess(result);
    });
    
    // POST /api/v1/services/{name}/async - 异步调用服务
    server->post("/api/v1/services/{name}/async", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.call")) {
            resp.setError(403, "Forbidden", "缺少权限: service.call");
            return;
        }
        
        ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            resp.setError(500, "ServiceRegistry not available");
            return;
        }
        
        AsyncServiceCall* asyncCall = serviceRegistry->asyncServiceCall();
        if (!asyncCall) {
            resp.setError(500, "AsyncServiceCall not available");
            return;
        }
        
        QString serviceName = req.pathParams.value("name");
        if (serviceName.isEmpty()) {
            resp.setError(400, "Bad Request", "Service name is required");
            return;
        }
        
        QJsonObject body = req.jsonBody();
        QString method = body.value("method").toString();
        QVariantList args;
        if (body.contains("args") && body["args"].isArray()) {
            QJsonArray argsArray = body["args"].toArray();
            for (const QJsonValue& value : argsArray) {
                args.append(value.toVariant());
            }
        }
        int timeout = body.value("timeout").toInt(5000);
        
        if (method.isEmpty()) {
            resp.setError(400, "Bad Request", "Method name is required");
            return;
        }
        
        // 启动异步调用
        ServiceFuture* future = asyncCall->callAsync(serviceName, method, args, timeout);
        
        // 立即返回，不等待结果
        QJsonObject result;
        result["async"] = true;
        result["serviceName"] = serviceName;
        result["method"] = method;
        result["message"] = "异步调用已启动，使用futureId查询结果";
        
        // 生成futureId（简化实现，使用Future指针地址的哈希）
        QString futureId = QString::number(reinterpret_cast<quintptr>(future), 16);
        result["futureId"] = futureId;
        
        // 存储Future（简化实现，实际应该使用更持久化的存储）
        // 这里暂时不存储，客户端需要轮询或使用WebSocket获取结果
        
        resp.setSuccess(result);
    });
    
    // GET /api/v1/services/async/{futureId} - 查询异步调用结果
    server->get("/api/v1/services/async/{futureId}", [framework](const HttpRequest& req, HttpResponse& resp) {
        QString userId = getUserIdFromRequest(framework, req);
        
        // 权限检查
        RBACManager* rbac = framework->rbacManager();
        if (rbac && !rbac->checkPermission(userId, "service.call")) {
            resp.setError(403, "Forbidden", "缺少权限: service.call");
            return;
        }
        
        QString futureIdStr = req.pathParams.value("futureId");
        bool ok;
        quintptr futurePtr = futureIdStr.toULongLong(&ok, 16);
        
        if (!ok) {
            resp.setError(400, "Bad Request", "Invalid future ID");
            return;
        }
        
        ServiceFuture* future = reinterpret_cast<ServiceFuture*>(futurePtr);
        if (!future) {
            resp.setError(404, "Not Found", "Future not found");
            return;
        }
        
        QJsonObject result;
        result["futureId"] = futureIdStr;
        result["finished"] = future->isFinished();
        
        if (future->isFinished()) {
            result["success"] = future->isSuccess();
            if (future->isSuccess()) {
                result["result"] = QJsonValue::fromVariant(future->result());
            } else {
                result["error"] = future->error();
            }
            result["elapsedMs"] = future->elapsedMs();
        }
        
        resp.setSuccess(result);
    });
}

} // namespace Core
} // namespace Eagle
