#ifndef EAGLE_CORE_PLUGINISOLATION_H
#define EAGLE_CORE_PLUGINISOLATION_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <functional>
#include "Logger.h"

namespace Eagle {
namespace Core {

/**
 * @brief 插件隔离管理器
 * 
 * 提供插件异常隔离和资源隔离功能
 */
class PluginIsolation {
public:
    /**
     * @brief 在隔离环境中执行代码
     * @param pluginId 插件ID
     * @param func 要执行的函数
     * @return 执行结果，如果发生异常返回false
     */
    static bool executeIsolated(const QString& pluginId, std::function<bool()> func);
    
    /**
     * @brief 在隔离环境中执行代码并返回结果
     * @param pluginId 插件ID
     * @param func 要执行的函数
     * @param defaultValue 异常时的默认值
     * @return 执行结果或默认值
     */
    template<typename T>
    static T executeIsolated(const QString& pluginId, std::function<T()> func, const T& defaultValue = T());
    
    /**
     * @brief 设置插件的资源限制
     * @param pluginId 插件ID
     * @param maxMemoryMB 最大内存（MB，-1表示无限制）
     * @param maxCpuPercent 最大CPU使用率（%，-1表示无限制）
     */
    static void setResourceLimits(const QString& pluginId, int maxMemoryMB = -1, int maxCpuPercent = -1);
    
    /**
     * @brief 检查插件是否超出资源限制
     * @param pluginId 插件ID
     * @return 是否超出限制
     */
    static bool checkResourceLimits(const QString& pluginId);
    
    /**
     * @brief 注册异常处理器
     * @param pluginId 插件ID
     * @param handler 异常处理函数
     */
    static void registerExceptionHandler(const QString& pluginId, std::function<void(const QString&)> handler);
    
    /**
     * @brief 获取插件的异常统计
     * @param pluginId 插件ID
     * @return 异常次数
     */
    static int getExceptionCount(const QString& pluginId);
    
    /**
     * @brief 重置插件的异常统计
     * @param pluginId 插件ID
     */
    static void resetExceptionCount(const QString& pluginId);
};

// 模板实现
template<typename T>
T PluginIsolation::executeIsolated(const QString& pluginId, std::function<T()> func, const T& defaultValue)
{
    try {
        return func();
    } catch (const std::exception& e) {
        Logger::error("PluginIsolation", QString("插件 %1 发生异常: %2").arg(pluginId, e.what()));
        registerExceptionHandler(pluginId, [](const QString& error) {
            Logger::error("PluginIsolation", QString("异常: %1").arg(error));
        });
        return defaultValue;
    } catch (...) {
        Logger::error("PluginIsolation", QString("插件 %1 发生未知异常").arg(pluginId));
        return defaultValue;
    }
}

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_PLUGINISOLATION_H
