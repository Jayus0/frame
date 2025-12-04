#ifndef EAGLE_CORE_HOTRELOADMANAGER_H
#define EAGLE_CORE_HOTRELOADMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

class PluginManager;

/**
 * @brief 热重载状态
 */
enum class HotReloadStatus {
    Idle,           // 空闲
    Saving,         // 保存状态中
    Unloading,      // 卸载中
    Loading,        // 加载中
    Restoring,      // 恢复状态中
    Success,        // 成功
    Failed          // 失败
};

/**
 * @brief 热重载结果
 */
struct HotReloadResult {
    bool success;               // 是否成功
    HotReloadStatus status;     // 状态
    QString errorMessage;       // 错误信息
    QDateTime startTime;        // 开始时间
    QDateTime endTime;          // 结束时间
    qint64 durationMs;          // 耗时（毫秒）
    
    HotReloadResult()
        : success(false)
        , status(HotReloadStatus::Idle)
        , durationMs(0)
    {
        startTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 插件状态快照
 */
struct PluginStateSnapshot {
    QString pluginId;           // 插件ID
    QVariantMap config;         // 配置
    QStringList loadedServices; // 已加载的服务
    QVariantMap metadata;       // 元数据
    QDateTime snapshotTime;     // 快照时间
    
    PluginStateSnapshot()
    {
        snapshotTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 热重载管理器
 * 
 * 负责插件的热重载功能，包括：
 * - 保存插件状态
 * - 卸载插件
 * - 重新加载插件
 * - 恢复插件状态
 */
class HotReloadManager : public QObject {
    Q_OBJECT
    
public:
    explicit HotReloadManager(PluginManager* pluginManager, QObject* parent = nullptr);
    ~HotReloadManager();
    
    // 热重载操作
    HotReloadResult reloadPlugin(const QString& pluginId, bool force = false);
    HotReloadResult reloadPlugins(const QStringList& pluginIds, bool force = false);
    
    // 状态管理
    bool savePluginState(const QString& pluginId);
    bool restorePluginState(const QString& pluginId);
    PluginStateSnapshot getPluginState(const QString& pluginId) const;
    bool hasPluginState(const QString& pluginId) const;
    
    // 配置
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setAutoSaveState(bool autoSave);
    bool isAutoSaveState() const;
    void setStateStoragePath(const QString& path);
    QString stateStoragePath() const;
    
    // 查询
    QStringList getReloadablePlugins() const;
    bool isPluginReloadable(const QString& pluginId) const;
    HotReloadStatus getStatus(const QString& pluginId) const;
    
signals:
    void reloadStarted(const QString& pluginId);
    void reloadFinished(const QString& pluginId, bool success);
    void reloadFailed(const QString& pluginId, const QString& error);
    void stateSaved(const QString& pluginId);
    void stateRestored(const QString& pluginId);
    
private slots:
    void onPluginUnloaded(const QString& pluginId);
    void onPluginLoaded(const QString& pluginId);
    
private:
    Q_DISABLE_COPY(HotReloadManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    bool checkDependencies(const QString& pluginId) const;
    bool checkReloadable(const QString& pluginId) const;
    QString generateStateFilePath(const QString& pluginId) const;
    bool saveStateToFile(const QString& pluginId, const PluginStateSnapshot& snapshot);
    PluginStateSnapshot loadStateFromFile(const QString& pluginId) const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::HotReloadStatus)
Q_DECLARE_METATYPE(Eagle::Core::HotReloadResult)
Q_DECLARE_METATYPE(Eagle::Core::PluginStateSnapshot)

#endif // EAGLE_CORE_HOTRELOADMANAGER_H
