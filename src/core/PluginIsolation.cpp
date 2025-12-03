#include "eagle/core/PluginIsolation.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QMap>
#include <exception>

namespace Eagle {
namespace Core {

struct ResourceLimits {
    int maxMemoryMB = -1;
    int maxCpuPercent = -1;
};

struct PluginIsolationData {
    int exceptionCount = 0;
    ResourceLimits limits;
    std::function<void(const QString&)> exceptionHandler;
};

static QMap<QString, PluginIsolationData> s_pluginData;
static QMutex s_mutex;

bool PluginIsolation::executeIsolated(const QString& pluginId, std::function<bool()> func)
{
    try {
        bool result = func();
        
        // 如果执行成功，重置异常计数
        if (result) {
            QMutexLocker locker(&s_mutex);
            PluginIsolationData& data = s_pluginData[pluginId];
            data.exceptionCount = 0;
        }
        
        return result;
    } catch (const std::exception& e) {
        QMutexLocker locker(&s_mutex);
        PluginIsolationData& data = s_pluginData[pluginId];
        data.exceptionCount++;
        
        QString error = QString("插件 %1 发生异常: %2").arg(pluginId, e.what());
        Logger::error("PluginIsolation", error);
        
        if (data.exceptionHandler) {
            data.exceptionHandler(error);
        }
        
        return false;
    } catch (...) {
        QMutexLocker locker(&s_mutex);
        PluginIsolationData& data = s_pluginData[pluginId];
        data.exceptionCount++;
        
        QString error = QString("插件 %1 发生未知异常").arg(pluginId);
        Logger::error("PluginIsolation", error);
        
        if (data.exceptionHandler) {
            data.exceptionHandler(error);
        }
        
        return false;
    }
}

void PluginIsolation::setResourceLimits(const QString& pluginId, int maxMemoryMB, int maxCpuPercent)
{
    QMutexLocker locker(&s_mutex);
    PluginIsolationData& data = s_pluginData[pluginId];
    data.limits.maxMemoryMB = maxMemoryMB;
    data.limits.maxCpuPercent = maxCpuPercent;
    
    Logger::info("PluginIsolation", QString("设置插件资源限制: %1 (内存: %2MB, CPU: %3%)")
        .arg(pluginId)
        .arg(maxMemoryMB < 0 ? "无限制" : QString::number(maxMemoryMB))
        .arg(maxCpuPercent < 0 ? "无限制" : QString::number(maxCpuPercent)));
}

bool PluginIsolation::checkResourceLimits(const QString& pluginId)
{
    QMutexLocker locker(&s_mutex);
    if (!s_pluginData.contains(pluginId)) {
        return true;  // 没有限制
    }
    
    const PluginIsolationData& data = s_pluginData[pluginId];
    const ResourceLimits& limits = data.limits;
    
    // 简化实现：实际应该检查进程的实际资源使用
    // 这里只是占位实现
    if (limits.maxMemoryMB > 0) {
        // TODO: 检查内存使用
    }
    
    if (limits.maxCpuPercent > 0) {
        // TODO: 检查CPU使用
    }
    
    return true;
}

void PluginIsolation::registerExceptionHandler(const QString& pluginId, std::function<void(const QString&)> handler)
{
    QMutexLocker locker(&s_mutex);
    PluginIsolationData& data = s_pluginData[pluginId];
    data.exceptionHandler = handler;
    Logger::info("PluginIsolation", QString("注册插件异常处理器: %1").arg(pluginId));
}

int PluginIsolation::getExceptionCount(const QString& pluginId)
{
    QMutexLocker locker(&s_mutex);
    if (s_pluginData.contains(pluginId)) {
        return s_pluginData[pluginId].exceptionCount;
    }
    return 0;
}

void PluginIsolation::resetExceptionCount(const QString& pluginId)
{
    QMutexLocker locker(&s_mutex);
    if (s_pluginData.contains(pluginId)) {
        s_pluginData[pluginId].exceptionCount = 0;
        Logger::info("PluginIsolation", QString("重置插件异常计数: %1").arg(pluginId));
    }
}

} // namespace Core
} // namespace Eagle
