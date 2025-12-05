#include "eagle/core/SystemHealth.h"
#include "eagle/core/Framework.h"
#include "eagle/core/ServiceRegistry.h"
#include "eagle/core/PluginManager.h"
#include "eagle/core/PerformanceMonitor.h"
#include "eagle/core/ResourceMonitor.h"
#include "eagle/core/ConfigManager.h"
#include "eagle/core/Logger.h"
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>

namespace Eagle {
namespace Core {

SystemHealthReport SystemHealthManager::getSystemHealth(Framework* framework)
{
    SystemHealthReport report;
    report.timestamp = QDateTime::currentDateTime();
    
    if (!framework) {
        report.overallStatus = "unknown";
        report.healthScore = 0.0;
        return report;
    }
    
    // 检查系统资源
    PerformanceMonitor* monitor = framework->performanceMonitor();
    if (monitor) {
        QJsonObject system;
        system["cpuUsage"] = monitor->getCpuUsage();
        system["memoryUsageMB"] = monitor->getMemoryUsageMB();
        report.systemResources = system;
        report.components["PerformanceMonitor"] = true;
    } else {
        report.components["PerformanceMonitor"] = false;
    }
    
    // 检查插件状态
    PluginManager* pluginManager = framework->pluginManager();
    if (pluginManager) {
        QJsonObject plugins;
        QStringList availablePlugins = pluginManager->availablePlugins();
        int loadedCount = 0;
        for (const QString& id : availablePlugins) {
            if (pluginManager->isPluginLoaded(id)) {
                loadedCount++;
            }
        }
        plugins["total"] = availablePlugins.size();
        plugins["loaded"] = loadedCount;
        plugins["unloaded"] = availablePlugins.size() - loadedCount;
        report.plugins = plugins;
        report.components["PluginManager"] = true;
    } else {
        report.components["PluginManager"] = false;
    }
    
    // 检查所有服务的健康状态
    ServiceRegistry* serviceRegistry = framework->serviceRegistry();
    if (serviceRegistry) {
        QStringList services = serviceRegistry->availableServices();
        QElapsedTimer timer;
        
        for (const QString& serviceName : services) {
            timer.restart();
            ServiceHealthInfo healthInfo = checkServiceHealth(serviceName, serviceRegistry);
            healthInfo.responseTimeMs = timer.elapsed();
            healthInfo.lastCheckTime = QDateTime::currentDateTime();
            
            report.services.append(healthInfo);
            
            switch (healthInfo.status) {
            case ServiceHealthStatus::Healthy:
                report.healthyServicesCount++;
                break;
            case ServiceHealthStatus::Unhealthy:
                report.unhealthyServicesCount++;
                break;
            case ServiceHealthStatus::Degraded:
                report.degradedServicesCount++;
                break;
            case ServiceHealthStatus::Unknown:
            default:
                report.unknownServicesCount++;
                break;
            }
        }
        
        report.components["ServiceRegistry"] = true;
    } else {
        report.components["ServiceRegistry"] = false;
    }
    
    // 检查其他组件
    report.components["ConfigManager"] = (framework->configManager() != nullptr);
    report.components["ResourceMonitor"] = (framework->resourceMonitor() != nullptr);
    report.components["ApiServer"] = (framework->apiServer() != nullptr);
    
    // 计算健康度分数
    report.healthScore = calculateHealthScore(report);
    
    // 确定整体状态
    report.overallStatus = determineOverallStatus(report);
    
    return report;
}

ServiceHealthInfo SystemHealthManager::checkServiceHealth(const QString& serviceName, 
                                                          ServiceRegistry* registry)
{
    ServiceHealthInfo info;
    info.serviceName = serviceName;
    
    if (!registry) {
        info.status = ServiceHealthStatus::Unknown;
        info.errorMessage = "ServiceRegistry not available";
        return info;
    }
    
    // 获取服务描述符
    ServiceDescriptor descriptor = registry->getServiceDescriptor(serviceName);
    if (descriptor.name.isEmpty()) {
        info.status = ServiceHealthStatus::Unknown;
        info.errorMessage = "Service not found";
        return info;
    }
    
    info.version = descriptor.version;
    
    // 检查服务是否可用
    QObject* service = registry->findService(serviceName);
    if (!service) {
        info.status = ServiceHealthStatus::Unhealthy;
        info.errorMessage = "Service object not found";
        return info;
    }
    
    // 尝试调用健康检查方法（如果存在）
    const QMetaObject* metaObj = service->metaObject();
    int healthCheckIndex = metaObj->indexOfMethod("healthCheck()");
    
    if (healthCheckIndex >= 0) {
        // 服务实现了healthCheck方法
        QMetaMethod healthCheckMethod = metaObj->method(healthCheckIndex);
        QVariant result;
        bool success = healthCheckMethod.invoke(service, Q_RETURN_ARG(QVariant, result));
        
        if (success) {
            if (result.type() == QVariant::Bool) {
                bool isHealthy = result.toBool();
                info.status = isHealthy ? ServiceHealthStatus::Healthy : ServiceHealthStatus::Unhealthy;
            } else if (result.type() == QVariant::Map) {
                QVariantMap healthMap = result.toMap();
                bool isHealthy = healthMap.value("healthy", false).toBool();
                QString status = healthMap.value("status", "unknown").toString();
                
                if (status == "degraded") {
                    info.status = ServiceHealthStatus::Degraded;
                } else {
                    info.status = isHealthy ? ServiceHealthStatus::Healthy : ServiceHealthStatus::Unhealthy;
                }
                info.errorMessage = healthMap.value("message", "").toString();
            } else {
                // 使用默认健康检查
                bool isHealthy = registry->checkServiceHealth(serviceName);
                info.status = isHealthy ? ServiceHealthStatus::Healthy : ServiceHealthStatus::Unhealthy;
            }
        } else {
            // 调用失败，使用默认健康检查
            bool isHealthy = registry->checkServiceHealth(serviceName);
            info.status = isHealthy ? ServiceHealthStatus::Healthy : ServiceHealthStatus::Unhealthy;
            info.errorMessage = "Health check method invocation failed";
        }
    } else {
        // 服务没有实现healthCheck方法，使用默认健康检查
        bool isHealthy = registry->checkServiceHealth(serviceName);
        info.status = isHealthy ? ServiceHealthStatus::Healthy : ServiceHealthStatus::Unhealthy;
    }
    
    return info;
}

double SystemHealthManager::calculateHealthScore(const SystemHealthReport& report)
{
    if (report.services.isEmpty()) {
        // 没有服务，检查组件状态
        int totalComponents = report.components.size();
        if (totalComponents == 0) {
            return 0.0;
        }
        
        int healthyComponents = 0;
        for (bool isHealthy : report.components.values()) {
            if (isHealthy) {
                healthyComponents++;
            }
        }
        
        return static_cast<double>(healthyComponents) / totalComponents;
    }
    
    // 计算服务健康度
    int totalServices = report.services.size();
    int healthyServices = report.healthyServicesCount;
    int degradedServices = report.degradedServicesCount;
    
    // 健康服务权重1.0，降级服务权重0.5，不健康服务权重0.0
    double score = (healthyServices * 1.0 + degradedServices * 0.5) / totalServices;
    
    // 考虑组件状态（如果组件不健康，降低分数）
    int totalComponents = report.components.size();
    if (totalComponents > 0) {
        int unhealthyComponents = 0;
        for (bool isHealthy : report.components.values()) {
            if (!isHealthy) {
                unhealthyComponents++;
            }
        }
        double componentScore = 1.0 - (static_cast<double>(unhealthyComponents) / totalComponents * 0.2);
        score *= componentScore;
    }
    
    return qBound(0.0, score, 1.0);
}

QString SystemHealthManager::determineOverallStatus(const SystemHealthReport& report)
{
    if (report.healthScore >= 0.95) {
        return "healthy";
    } else if (report.healthScore >= 0.70) {
        return "degraded";
    } else if (report.healthScore >= 0.50) {
        return "unhealthy";
    } else {
        return "critical";
    }
}

QJsonObject SystemHealthManager::toJson(const SystemHealthReport& report)
{
    QJsonObject json;
    json["overallStatus"] = report.overallStatus;
    json["healthScore"] = report.healthScore;
    json["timestamp"] = report.timestamp.toString(Qt::ISODate);
    
    // 系统资源
    json["systemResources"] = report.systemResources;
    
    // 插件信息
    json["plugins"] = report.plugins;
    
    // 服务健康信息
    QJsonArray servicesArray;
    for (const ServiceHealthInfo& info : report.services) {
        QJsonObject serviceJson;
        serviceJson["serviceName"] = info.serviceName;
        serviceJson["version"] = info.version;
        
        QString statusStr;
        switch (info.status) {
        case ServiceHealthStatus::Healthy:
            statusStr = "healthy";
            break;
        case ServiceHealthStatus::Unhealthy:
            statusStr = "unhealthy";
            break;
        case ServiceHealthStatus::Degraded:
            statusStr = "degraded";
            break;
        case ServiceHealthStatus::Unknown:
        default:
            statusStr = "unknown";
            break;
        }
        serviceJson["status"] = statusStr;
        serviceJson["responseTimeMs"] = info.responseTimeMs;
        if (!info.errorMessage.isEmpty()) {
            serviceJson["errorMessage"] = info.errorMessage;
        }
        serviceJson["lastCheckTime"] = info.lastCheckTime.toString(Qt::ISODate);
        
        servicesArray.append(serviceJson);
    }
    json["services"] = servicesArray;
    
    // 服务统计
    QJsonObject servicesStats;
    servicesStats["total"] = report.services.size();
    servicesStats["healthy"] = report.healthyServicesCount;
    servicesStats["unhealthy"] = report.unhealthyServicesCount;
    servicesStats["degraded"] = report.degradedServicesCount;
    servicesStats["unknown"] = report.unknownServicesCount;
    json["servicesStats"] = servicesStats;
    
    // 组件状态
    QJsonObject components;
    for (auto it = report.components.begin(); it != report.components.end(); ++it) {
        components[it.key()] = it.value();
    }
    json["components"] = components;
    
    return json;
}

SystemHealthReport SystemHealthManager::fromJson(const QJsonObject& json)
{
    SystemHealthReport report;
    
    report.overallStatus = json.value("overallStatus").toString("unknown");
    report.healthScore = json.value("healthScore").toDouble(0.0);
    report.timestamp = QDateTime::fromString(json.value("timestamp").toString(), Qt::ISODate);
    
    report.systemResources = json.value("systemResources").toObject();
    report.plugins = json.value("plugins").toObject();
    
    QJsonArray servicesArray = json.value("services").toArray();
    for (const QJsonValue& value : servicesArray) {
        QJsonObject serviceJson = value.toObject();
        ServiceHealthInfo info;
        info.serviceName = serviceJson.value("serviceName").toString();
        info.version = serviceJson.value("version").toString();
        
        QString statusStr = serviceJson.value("status").toString("unknown");
        if (statusStr == "healthy") {
            info.status = ServiceHealthStatus::Healthy;
        } else if (statusStr == "unhealthy") {
            info.status = ServiceHealthStatus::Unhealthy;
        } else if (statusStr == "degraded") {
            info.status = ServiceHealthStatus::Degraded;
        } else {
            info.status = ServiceHealthStatus::Unknown;
        }
        
        info.responseTimeMs = serviceJson.value("responseTimeMs").toInt(-1);
        info.errorMessage = serviceJson.value("errorMessage").toString();
        info.lastCheckTime = QDateTime::fromString(serviceJson.value("lastCheckTime").toString(), Qt::ISODate);
        
        report.services.append(info);
    }
    
    QJsonObject servicesStats = json.value("servicesStats").toObject();
    report.healthyServicesCount = servicesStats.value("healthy").toInt(0);
    report.unhealthyServicesCount = servicesStats.value("unhealthy").toInt(0);
    report.degradedServicesCount = servicesStats.value("degraded").toInt(0);
    report.unknownServicesCount = servicesStats.value("unknown").toInt(0);
    
    QJsonObject components = json.value("components").toObject();
    for (auto it = components.begin(); it != components.end(); ++it) {
        report.components[it.key()] = it.value().toBool(false);
    }
    
    return report;
}

} // namespace Core
} // namespace Eagle
