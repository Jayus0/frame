#ifndef EAGLE_CORE_FAILOVERMANAGER_H
#define EAGLE_CORE_FAILOVERMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

class ServiceRegistry;

/**
 * @brief 服务角色
 */
enum class ServiceRole {
    Primary,    // 主服务
    Standby     // 备服务
};

/**
 * @brief 服务状态
 */
enum class ServiceStatus {
    Healthy,    // 健康
    Degraded,   // 降级
    Unhealthy,  // 不健康
    Failed      // 失败
};

/**
 * @brief 故障转移模式
 */
enum class FailoverMode {
    Automatic,  // 自动切换
    Manual,     // 手动切换
    Disabled    // 禁用
};

/**
 * @brief 服务节点信息
 */
struct ServiceNode {
    QString id;                 // 节点ID
    QString name;               // 节点名称
    ServiceRole role;           // 角色
    ServiceStatus status;       // 状态
    QString endpoint;           // 服务端点
    QDateTime lastHealthCheck;  // 最后健康检查时间
    int consecutiveFailures;    // 连续失败次数
    QVariantMap metadata;       // 元数据
    
    ServiceNode()
        : role(ServiceRole::Standby)
        , status(ServiceStatus::Healthy)
        , consecutiveFailures(0)
    {
        lastHealthCheck = QDateTime::currentDateTime();
    }
    
    bool isValid() const {
        return !id.isEmpty() && !endpoint.isEmpty();
    }
};

/**
 * @brief 故障转移配置
 */
struct FailoverConfig {
    QString serviceName;        // 服务名称
    FailoverMode mode;          // 故障转移模式
    int healthCheckIntervalMs;  // 健康检查间隔（毫秒）
    int failureThreshold;       // 失败阈值（连续失败次数）
    int recoveryThreshold;     // 恢复阈值（连续成功次数）
    bool enableStateSync;       // 启用状态同步
    int stateSyncIntervalMs;    // 状态同步间隔（毫秒）
    QStringList primaryNodes;   // 主节点列表
    QStringList standbyNodes;   // 备节点列表
    
    FailoverConfig()
        : mode(FailoverMode::Automatic)
        , healthCheckIntervalMs(5000)
        , failureThreshold(3)
        , recoveryThreshold(2)
        , enableStateSync(true)
        , stateSyncIntervalMs(10000)
    {
    }
    
    bool isValid() const {
        return !serviceName.isEmpty() && 
               !primaryNodes.isEmpty() && 
               !standbyNodes.isEmpty();
    }
};

/**
 * @brief 故障转移事件
 */
struct FailoverEvent {
    QString serviceName;        // 服务名称
    QString fromNodeId;         // 源节点ID
    QString toNodeId;           // 目标节点ID
    QString reason;             // 原因
    QDateTime timestamp;        // 时间戳
    bool success;               // 是否成功
    
    FailoverEvent()
        : success(false)
    {
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief 故障转移管理器
 * 
 * 负责管理主备服务切换、故障检测和状态同步
 */
class FailoverManager : public QObject {
    Q_OBJECT
    
public:
    explicit FailoverManager(ServiceRegistry* serviceRegistry, QObject* parent = nullptr);
    ~FailoverManager();
    
    // 配置管理
    bool registerService(const FailoverConfig& config);
    bool unregisterService(const QString& serviceName);
    FailoverConfig getServiceConfig(const QString& serviceName) const;
    QStringList getRegisteredServices() const;
    
    // 节点管理
    bool addNode(const QString& serviceName, const ServiceNode& node);
    bool removeNode(const QString& serviceName, const QString& nodeId);
    ServiceNode getNode(const QString& serviceName, const QString& nodeId) const;
    QList<ServiceNode> getNodes(const QString& serviceName) const;
    
    // 故障转移操作
    bool performFailover(const QString& serviceName, const QString& targetNodeId = QString());
    bool switchToPrimary(const QString& serviceName, const QString& nodeId);
    bool switchToStandby(const QString& serviceName, const QString& nodeId);
    
    // 状态查询
    ServiceNode getCurrentPrimary(const QString& serviceName) const;
    QList<ServiceNode> getStandbyNodes(const QString& serviceName) const;
    ServiceStatus getServiceStatus(const QString& serviceName) const;
    QList<FailoverEvent> getFailoverHistory(const QString& serviceName, int limit = 10) const;
    
    // 控制
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setServiceEnabled(const QString& serviceName, bool enabled);
    bool isServiceEnabled(const QString& serviceName) const;
    
signals:
    void failoverTriggered(const QString& serviceName, const QString& fromNode, const QString& toNode);
    void failoverCompleted(const QString& serviceName, bool success);
    void nodeStatusChanged(const QString& serviceName, const QString& nodeId, ServiceStatus status);
    void healthCheckFailed(const QString& serviceName, const QString& nodeId);
    
private slots:
    void onHealthCheckTimer();
    void onStateSyncTimer();
    
private:
    Q_DISABLE_COPY(FailoverManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    bool performHealthCheck(const QString& serviceName, const QString& nodeId);
    bool checkNodeHealth(const ServiceNode& node) const;
    void updateNodeStatus(const QString& serviceName, const QString& nodeId, ServiceStatus status);
    void triggerFailover(const QString& serviceName, const QString& reason);
    bool syncState(const QString& serviceName, const ServiceNode& from, const ServiceNode& to);
    void recordFailoverEvent(const FailoverEvent& event);
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::ServiceRole)
Q_DECLARE_METATYPE(Eagle::Core::ServiceStatus)
Q_DECLARE_METATYPE(Eagle::Core::FailoverMode)
Q_DECLARE_METATYPE(Eagle::Core::ServiceNode)
Q_DECLARE_METATYPE(Eagle::Core::FailoverConfig)
Q_DECLARE_METATYPE(Eagle::Core::FailoverEvent)

#endif // EAGLE_CORE_FAILOVERMANAGER_H
