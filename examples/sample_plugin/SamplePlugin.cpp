#include "SamplePlugin.h"
#include "eagle/core/Logger.h"
#include <QtCore/QJsonObject>

SamplePlugin::SamplePlugin(QObject* parent)
    : IPlugin(parent)
    , m_widget(nullptr)
    , m_label(nullptr)
    , m_button(nullptr)
{
}

SamplePlugin::~SamplePlugin()
{
    shutdown();
}

Eagle::Core::PluginMetadata SamplePlugin::metadata() const
{
    Eagle::Core::PluginMetadata meta;
    meta.pluginId = "com.eagle.sample";
    meta.name = "示例插件";
    meta.version = "1.0.0";
    meta.author = "Eagle Framework Team";
    meta.description = "这是一个示例插件，演示如何使用Eagle框架开发插件";
    meta.dependencies = QStringList(); // 无依赖
    meta.permissions = QStringList({"ui.read", "ui.write"});
    return meta;
}

bool SamplePlugin::initialize(const Eagle::Core::PluginContext& context)
{
    m_context = context;
    Eagle::Core::Logger::info("SamplePlugin", "插件初始化中...");
    
    // 这里可以初始化插件需要的资源
    // 例如：连接服务、订阅事件等
    
    Eagle::Core::Logger::info("SamplePlugin", "插件初始化完成");
    return true;
}

void SamplePlugin::shutdown()
{
    Eagle::Core::Logger::info("SamplePlugin", "插件关闭中...");
    
    if (m_widget) {
        delete m_widget;
        m_widget = nullptr;
    }
    
    Eagle::Core::Logger::info("SamplePlugin", "插件关闭完成");
}

void SamplePlugin::configure(const QVariantMap& config)
{
    Eagle::Core::Logger::info("SamplePlugin", "配置更新");
    // 处理配置更新
    Q_UNUSED(config)
}

QWidget* SamplePlugin::createWidget(QWidget* parent)
{
    if (m_widget) {
        return m_widget;
    }
    
    m_widget = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout(m_widget);
    
    m_label = new QLabel("这是示例插件", m_widget);
    m_button = new QPushButton("点击我", m_widget);
    
    layout->addWidget(m_label);
    layout->addWidget(m_button);
    
    connect(m_button, &QPushButton::clicked, this, &SamplePlugin::onButtonClicked);
    
    Eagle::Core::Logger::info("SamplePlugin", "UI组件创建完成");
    return m_widget;
}

QList<QAction*> SamplePlugin::menuActions() const
{
    QList<QAction*> actions;
    // 可以添加菜单项
    return actions;
}

QList<Eagle::Core::ServiceDescriptor> SamplePlugin::services() const
{
    QList<Eagle::Core::ServiceDescriptor> services;
    
    // 示例：注册一个服务
    Eagle::Core::ServiceDescriptor desc;
    desc.serviceName = "SampleService";
    desc.version = "1.0.0";
    desc.methods = QStringList({"getMessage", "setMessage"});
    desc.endpoints = QStringList({"local"});
    desc.healthCheck = "/health";
    
    services.append(desc);
    
    return services;
}

void SamplePlugin::onButtonClicked()
{
    Eagle::Core::Logger::info("SamplePlugin", "按钮被点击");
    if (m_label) {
        m_label->setText("按钮已被点击！");
    }
}
