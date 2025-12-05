#ifndef EAGLE_CORE_SYSTEMHEALTH_H
#define EAGLE_CORE_SYSTEMHEALTH_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

/**
 * @brief 服务健康状态
 */
enum class ServiceHealthStatus {
    Healthy,      // 健康
    Unhealthy,    // 不健康
    Unknown,      // 未知
    Degraded      // 降级
};

/**
 * @brief 服务健康信息
 */
struct ServiceHealthInfo {
    QString serviceName;              // 服务名称
    QString version;                  // 服务版本
    ServiceHealthStatus status;       // 健康状态
    qint64 responseTimeMs;           // 响应时间（毫秒）
    QString errorMessage;             // 错误信息（如果有）
    QDateTime lastCheckTime;         // 最后检查时间
    
    ServiceHealthInfo()
        : status(ServiceHealthStatus::Unknown)
        , responseTimeMs(-1)
    {}
};

/**
 * @brief 系统健康报告
 */
struct SystemHealthReport {
    QString overallStatus;                    // 整体状态（healthy/unhealthy/degraded）
    double healthScore;                       // 健康度（0.0-1.0）
    QDateTime timestamp;                      // 报告时间
    
    // 系统资源
    QJsonObject systemResources;             // CPU、内存等系统资源
    
    // 插件健康
    QJsonObject plugins;                     // 插件统计信息
    
    // 服务健康
    QList<ServiceHealthInfo> services;       // 所有服务的健康信息
    int healthyServicesCount;                // 健康服务数量
    int unhealthyServicesCount;               // 不健康服务数量
    int unknownServicesCount;                 // 未知状态服务数量
    int degradedServicesCount;                // 降级服务数量
    
    // 组件状态
    QMap<QString, bool> components;          // 各组件状态（true=正常，false=异常）
    
    SystemHealthReport()
        : overallStatus("unknown")
        , healthScore(0.0)
        , healthyServicesCount(0)
        , unhealthyServicesCount(0)
        , unknownServicesCount(0)
        , degradedServicesCount(0)
    {}
};

/**
 * @brief 系统健康检查管理器
 */
class SystemHealthManager {
public:
    /**
     * @brief 获取系统健康报告
     */
    static SystemHealthReport getSystemHealth(class Framework* framework);
    
    /**
     * @brief 将健康报告转换为JSON
     */
    static QJsonObject toJson(const SystemHealthReport& report);
    
    /**
     * @brief 从JSON创建健康报告
     */
    static SystemHealthReport fromJson(const QJsonObject& json);
    
private:
    /**
     * @brief 检查服务健康
     */
    static ServiceHealthInfo checkServiceHealth(const QString& serviceName, 
                                                class ServiceRegistry* registry);
    
    /**
     * @brief 计算健康度分数
     */
    static double calculateHealthScore(const SystemHealthReport& report);
    
    /**
     * @brief 确定整体状态
     */
    static QString determineOverallStatus(const SystemHealthReport& report);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_SYSTEMHEALTH_H
