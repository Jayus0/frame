#ifndef EAGLE_CORE_PERFORMANCEMONITOR_H
#define EAGLE_CORE_PERFORMANCEMONITOR_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

/**
 * @brief 性能指标
 */
struct PerformanceMetric {
    QString name;           // 指标名称
    double value;           // 当前值
    double minValue;        // 最小值
    double maxValue;        // 最大值
    double avgValue;        // 平均值
    int sampleCount;        // 采样次数
    QDateTime lastUpdate;   // 最后更新时间
    
    PerformanceMetric() 
        : value(0.0), minValue(0.0), maxValue(0.0), avgValue(0.0), sampleCount(0)
    {
        lastUpdate = QDateTime::currentDateTime();
    }
    
    void update(double newValue) {
        value = newValue;
        if (sampleCount == 0) {
            minValue = maxValue = avgValue = newValue;
        } else {
            if (newValue < minValue) minValue = newValue;
            if (newValue > maxValue) maxValue = newValue;
            avgValue = (avgValue * sampleCount + newValue) / (sampleCount + 1);
        }
        sampleCount++;
        lastUpdate = QDateTime::currentDateTime();
    }
    
    void reset() {
        value = minValue = maxValue = avgValue = 0.0;
        sampleCount = 0;
        lastUpdate = QDateTime::currentDateTime();
    }
};

/**
 * @brief 性能监控器
 */
class PerformanceMonitor : public QObject {
    Q_OBJECT
    
public:
    explicit PerformanceMonitor(QObject* parent = nullptr);
    ~PerformanceMonitor();
    
    // 指标更新
    void updateMetric(const QString& name, double value);
    void recordPluginLoadTime(const QString& pluginId, qint64 loadTimeMs);
    void recordServiceCallTime(const QString& serviceName, const QString& method, qint64 callTimeMs);
    
    // 指标查询
    PerformanceMetric getMetric(const QString& name) const;
    QMap<QString, PerformanceMetric> getAllMetrics() const;
    
    // 系统资源监控
    double getCpuUsage() const;
    qint64 getMemoryUsage() const;  // 返回字节数
    qint64 getMemoryUsageMB() const;  // 返回MB
    
    // 插件性能
    qint64 getPluginLoadTime(const QString& pluginId) const;
    QMap<QString, qint64> getAllPluginLoadTimes() const;
    
    // 服务性能
    qint64 getServiceCallTime(const QString& serviceName, const QString& method) const;
    QMap<QString, qint64> getServiceCallTimes(const QString& serviceName) const;
    
    // 配置
    void setUpdateInterval(int intervalMs);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // 统计
    void resetMetrics();
    void resetMetric(const QString& name);
    
signals:
    void metricUpdated(const QString& name, double value);
    void cpuUsageChanged(double usage);
    void memoryUsageChanged(qint64 bytes);
    
private slots:
    void onUpdateTimer();
    
private:
    Q_DISABLE_COPY(PerformanceMonitor)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    void updateSystemMetrics();
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::PerformanceMetric)

#endif // EAGLE_CORE_PERFORMANCEMONITOR_H
