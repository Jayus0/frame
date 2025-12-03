#ifndef SAMPLEPLUGIN_H
#define SAMPLEPLUGIN_H

#include "eagle/core/IPlugin.h"
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

class SamplePlugin : public Eagle::Core::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.eagle.framework.IPlugin/1.0")
    Q_INTERFACES(Eagle::Core::IPlugin)
    
public:
    SamplePlugin(QObject* parent = nullptr);
    ~SamplePlugin();
    
    // IPlugin interface
    Eagle::Core::PluginMetadata metadata() const override;
    bool initialize(const Eagle::Core::PluginContext& context) override;
    void shutdown() override;
    void configure(const QVariantMap& config) override;
    QWidget* createWidget(QWidget* parent = nullptr) override;
    QList<QAction*> menuActions() const override;
    QList<Eagle::Core::ServiceDescriptor> services() const override;
    
private slots:
    void onButtonClicked();
    
private:
    QWidget* m_widget;
    QLabel* m_label;
    QPushButton* m_button;
    Eagle::Core::PluginContext m_context;
};

#endif // SAMPLEPLUGIN_H
