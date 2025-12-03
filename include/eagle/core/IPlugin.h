#ifndef EAGLE_CORE_IPLUGIN_H
#define EAGLE_CORE_IPLUGIN_H

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>
#include <QtWidgets/QAction>

QT_BEGIN_NAMESPACE
class QWidget;
class QAction;
QT_END_NAMESPACE

namespace Eagle {
namespace Core {

// 插件元数据
struct PluginMetadata {
    QString pluginId;
    QString name;
    QString version;
    QString author;
    QString description;
    QStringList dependencies;
    QStringList permissions;
    QString configSchema;
    
    bool isValid() const {
        return !pluginId.isEmpty() && !name.isEmpty() && !version.isEmpty();
    }
};

// 插件上下文
struct PluginContext {
    QString pluginPath;
    QString configPath;
    QVariantMap globalConfig;
    QObject* eventBus;
    QObject* serviceRegistry;
};

// 服务描述符
struct ServiceDescriptor {
    QString serviceName;
    QString version;
    QStringList methods;
    QStringList endpoints;
    QString healthCheck;
    
    bool isValid() const {
        return !serviceName.isEmpty() && !version.isEmpty();
    }
};

// 基础插件接口
class IPlugin : public QObject {
    Q_OBJECT
    
public:
    explicit IPlugin(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IPlugin() = default;
    
    // 元数据
    virtual PluginMetadata metadata() const = 0;
    
    // 生命周期
    virtual bool initialize(const PluginContext& context) = 0;
    virtual void shutdown() = 0;
    
    // 配置
    virtual void configure(const QVariantMap& config) = 0;
    
    // UI支持
    virtual QWidget* createWidget(QWidget* parent = nullptr) {
        Q_UNUSED(parent)
        return nullptr;
    }
    
    virtual QList<QAction*> menuActions() const {
        return QList<QAction*>();
    }
    
    // 服务注册
    virtual QList<ServiceDescriptor> services() const {
        return QList<ServiceDescriptor>();
    }
    
    // 健康检查
    virtual bool isHealthy() const {
        return true;
    }
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_INTERFACE(Eagle::Core::IPlugin, "com.eagle.framework.IPlugin/1.0")

#endif // EAGLE_CORE_IPLUGIN_H
