#ifndef EAGLE_CORE_RESOURCEMONITOR_H
#define EAGLE_CORE_RESOURCEMONITOR_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

/**
 * @brief 资源使用信息
 */
struct ResourceUsage {
    QString pluginId;       // 插件ID
    qint64 memoryBytes;     // 内存使用（字节）
    double cpuPercent;      // CPU使用率（%）
    qint64 threadCount;     // 线程数
    QDateTime lastUpdate;    // 最后更新时间
    
    ResourceUsage()
        : memoryBytes(0)
        , cpuPercent(0.0)
        , threadCount(0)
    {
        lastUpdate = QDateTime::currentDateTime();
    }
    
    bool isValid() const {
        return !pluginId.isEmpty();
    }
};

/**
 * @brief 资源限制配置
 */
struct ResourceLimits {
    int maxMemoryMB;        // 最大内存（MB，-1表示无限制）
    int maxCpuPercent;      // 最大CPU使用率（%，-1表示无限制）
    int maxThreads;         // 最大线程数（-1表示无限制）
    bool enforceLimits;     // 是否强制执行限制
    
    ResourceLimits()
        : maxMemoryMB(-1)
        , maxCpuPercent(-1)
        , maxThreads(-1)
        , enforceLimits(false)
    {
    }
    
    bool hasLimits() const {
        return maxMemoryMB > 0 || maxCpuPercent > 0 || maxThreads > 0;
    }
};

/**
 * @brief 资源超限事件
 */
struct ResourceLimitExceeded {
    QString pluginId;       // 插件ID
    QString resourceType;   // 资源类型（memory/cpu/threads）
    double currentValue;     // 当前值
    double limitValue;       // 限制值
    QDateTime timestamp;    // 时间戳
    
    ResourceLimitExceeded()
        : currentValue(0.0)
        , limitValue(0.0)
    {
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief 资源监控器
 * 
 * 负责监控插件的资源使用情况，并强制执行资源限制
 */
class ResourceMonitor : public QObject {
    Q_OBJECT
    
public:
    explicit ResourceMonitor(QObject* parent = nullptr);
    ~ResourceMonitor();
    
    // 插件资源监控
    bool registerPlugin(const QString& pluginId, const ResourceLimits& limits = ResourceLimits());
    bool unregisterPlugin(const QString& pluginId);
    void setResourceLimits(const QString& pluginId, const ResourceLimits& limits);
    ResourceLimits getResourceLimits(const QString& pluginId) const;
    
    // 资源使用查询
    ResourceUsage getResourceUsage(const QString& pluginId) const;
    QMap<QString, ResourceUsage> getAllResourceUsage() const;
    bool isResourceLimitExceeded(const QString& pluginId) const;
    
    // 配置
    void setMonitoringEnabled(bool enabled);
    bool isMonitoringEnabled() const;
    void setMonitoringInterval(int intervalMs);
    int monitoringInterval() const;
    void setEnforcementEnabled(bool enabled);
    bool isEnforcementEnabled() const;
    
    // 超限事件查询
    QList<ResourceLimitExceeded> getLimitExceededEvents(const QString& pluginId, int limit = 10) const;
    bool clearLimitExceededEvents(const QString& pluginId);
    
signals:
    void resourceLimitExceeded(const QString& pluginId, const QString& resourceType, double currentValue, double limitValue);
    void resourceUsageUpdated(const QString& pluginId, const ResourceUsage& usage);
    
private slots:
    void onMonitoringTimer();
    
private:
    Q_DISABLE_COPY(ResourceMonitor)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    void updateResourceUsage(const QString& pluginId);
    bool checkResourceLimits(const QString& pluginId, const ResourceUsage& usage, const ResourceLimits& limits) const;
    void handleResourceLimitExceeded(const QString& pluginId, const QString& resourceType, double currentValue, double limitValue);
    qint64 getPluginMemoryUsage(const QString& pluginId) const;
    double getPluginCpuUsage(const QString& pluginId) const;
    qint64 getPluginThreadCount(const QString& pluginId) const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::ResourceUsage)
Q_DECLARE_METATYPE(Eagle::Core::ResourceLimits)
Q_DECLARE_METATYPE(Eagle::Core::ResourceLimitExceeded)

#endif // EAGLE_CORE_RESOURCEMONITOR_H
