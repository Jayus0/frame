#include "eagle/core/PluginIsolation.h"
#include "eagle/core/Logger.h"
#include "eagle/core/ResourceMonitor.h"
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QMap>
#include <QtCore/QCoreApplication>
#include <exception>

namespace Eagle {
namespace Core {

// 使用ResourceMonitor中的ResourceLimits，不再重复定义
struct PluginIsolationData {
    int exceptionCount = 0;
    int maxMemoryMB = -1;  // 保持兼容性
    int maxCpuPercent = -1;  // 保持兼容性
    std::function<void(const QString&)> exceptionHandler;
};

static QMap<QString, PluginIsolationData> s_pluginData;
static QMutex s_mutex;
static ResourceMonitor* s_resourceMonitor = nullptr;

// 初始化资源监控器
static void initResourceMonitor()
{
    if (!s_resourceMonitor) {
        s_resourceMonitor = new ResourceMonitor(QCoreApplication::instance());
    }
}

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
    initResourceMonitor();
    
    QMutexLocker locker(&s_mutex);
    PluginIsolationData& data = s_pluginData[pluginId];
    data.maxMemoryMB = maxMemoryMB;
    data.maxCpuPercent = maxCpuPercent;
    
    // 同步到ResourceMonitor
    if (s_resourceMonitor) {
        ResourceLimits limits;
        limits.maxMemoryMB = maxMemoryMB;
        limits.maxCpuPercent = maxCpuPercent;
        limits.enforceLimits = true;
        s_resourceMonitor->setResourceLimits(pluginId, limits);
    }
    
    Logger::info("PluginIsolation", QString("设置插件资源限制: %1 (内存: %2MB, CPU: %3%)")
        .arg(pluginId)
        .arg(maxMemoryMB < 0 ? "无限制" : QString::number(maxMemoryMB))
        .arg(maxCpuPercent < 0 ? "无限制" : QString::number(maxCpuPercent)));
}

bool PluginIsolation::checkResourceLimits(const QString& pluginId)
{
    initResourceMonitor();
    
    QMutexLocker locker(&s_mutex);
    if (!s_pluginData.contains(pluginId)) {
        return true;  // 没有限制
    }
    
    const PluginIsolationData& data = s_pluginData[pluginId];
    
    // 如果没有限制，直接返回
    if (data.maxMemoryMB < 0 && data.maxCpuPercent < 0) {
        return true;
    }
    
    locker.unlock();
    
    // 使用ResourceMonitor检查资源限制
    if (s_resourceMonitor) {
        return !s_resourceMonitor->isResourceLimitExceeded(pluginId);
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
