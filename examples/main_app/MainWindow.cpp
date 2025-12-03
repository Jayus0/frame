#include "MainWindow.h"
#include "eagle/core/Logger.h"
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QDialog>
#include <QtCore/QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_pluginList(nullptr)
    , m_loadButton(nullptr)
    , m_unloadButton(nullptr)
    , m_refreshButton(nullptr)
    , m_statusLabel(nullptr)
    , m_framework(Eagle::Core::Framework::instance())
{
    setupUI();
    updatePluginList();
    
    setWindowTitle("Eagle Framework - 插件管理演示");
    resize(800, 600);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // 状态标签
    m_statusLabel = new QLabel("就绪", this);
    m_mainLayout->addWidget(m_statusLabel);
    
    // 插件列表
    m_pluginList = new QListWidget(this);
    m_mainLayout->addWidget(m_pluginList);
    
    // 按钮布局
    m_buttonLayout = new QHBoxLayout();
    m_loadButton = new QPushButton("加载插件", this);
    m_unloadButton = new QPushButton("卸载插件", this);
    m_refreshButton = new QPushButton("刷新列表", this);
    
    m_buttonLayout->addWidget(m_loadButton);
    m_buttonLayout->addWidget(m_unloadButton);
    m_buttonLayout->addWidget(m_refreshButton);
    m_buttonLayout->addStretch();
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // 连接信号
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::onLoadPlugin);
    connect(m_unloadButton, &QPushButton::clicked, this, &MainWindow::onUnloadPlugin);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshPlugins);
    connect(m_pluginList, &QListWidget::itemDoubleClicked, this, &MainWindow::onPluginSelected);
    
    // 连接框架信号（使用QueuedConnection避免死锁）
    if (m_framework && m_framework->pluginManager()) {
        connect(m_framework->pluginManager(), &Eagle::Core::PluginManager::pluginLoaded,
                this, [this](const QString& pluginId) {
            m_statusLabel->setText(QString("插件已加载: %1").arg(pluginId));
            QTimer::singleShot(0, this, [this]() { updatePluginList(); });  // 延迟更新，避免在信号处理中调用
        }, Qt::QueuedConnection);
        
        connect(m_framework->pluginManager(), &Eagle::Core::PluginManager::pluginUnloaded,
                this, [this](const QString& pluginId) {
            m_statusLabel->setText(QString("插件已卸载: %1").arg(pluginId));
            QTimer::singleShot(0, this, [this]() { updatePluginList(); });
        }, Qt::QueuedConnection);
        
        connect(m_framework->pluginManager(), &Eagle::Core::PluginManager::pluginError,
                this, [this](const QString& pluginId, const QString& error) {
            m_statusLabel->setText(QString("错误: %1 - %2").arg(pluginId, error));
            QMessageBox::warning(this, "插件错误", QString("插件 %1 发生错误:\n%2").arg(pluginId, error));
        }, Qt::QueuedConnection);
    }
}

void MainWindow::updatePluginList()
{
    if (!m_framework || !m_framework->pluginManager()) {
        return;
    }
    
    m_pluginList->clear();
    
    QStringList plugins = m_framework->pluginManager()->availablePlugins();
    
    if (plugins.isEmpty()) {
        // 如果没有插件，显示提示信息
        QListWidgetItem* item = new QListWidgetItem("未找到插件", m_pluginList);
        item->setForeground(Qt::gray);
        item->setData(Qt::UserRole, QString());
        
        // 显示插件扫描路径信息
        QStringList scanPaths = m_framework->pluginManager()->pluginPaths();
        QString pathsInfo = scanPaths.isEmpty() ? "未配置插件路径" : 
                           QString("扫描路径: %1").arg(scanPaths.join(", "));
        m_statusLabel->setText(QString("未找到插件。%1").arg(pathsInfo));
    } else {
        for (const QString& pluginId : plugins) {
            Eagle::Core::PluginMetadata meta = m_framework->pluginManager()->getPluginMetadata(pluginId);
            bool isLoaded = m_framework->pluginManager()->isPluginLoaded(pluginId);
            
            QString displayText = QString("%1 (%2) - %3")
                .arg(meta.name, meta.version, isLoaded ? "[已加载]" : "[未加载]");
            
            QListWidgetItem* item = new QListWidgetItem(displayText, m_pluginList);
            item->setData(Qt::UserRole, pluginId);
            item->setData(Qt::UserRole + 1, isLoaded);
            
            if (isLoaded) {
                item->setForeground(Qt::green);
            } else {
                item->setForeground(Qt::black);
            }
        }
    }
}

void MainWindow::onLoadPlugin()
{
    QListWidgetItem* item = m_pluginList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个插件");
        return;
    }
    
    QString pluginId = item->data(Qt::UserRole).toString();
    if (pluginId.isEmpty()) {
        return;
    }
    
    // 禁用按钮，防止重复点击
    m_loadButton->setEnabled(false);
    m_statusLabel->setText(QString("正在加载插件: %1...").arg(pluginId));
    QApplication::processEvents();  // 更新界面
    
    if (m_framework && m_framework->pluginManager()) {
        // 在后台线程中加载（实际上还是在主线程，但这样可以避免界面卡死）
        QTimer::singleShot(0, this, [this, pluginId]() {
            bool success = m_framework->pluginManager()->loadPlugin(pluginId);
            m_loadButton->setEnabled(true);
            if (!success) {
                m_statusLabel->setText(QString("加载插件失败: %1").arg(pluginId));
                QMessageBox::warning(this, "错误", QString("加载插件失败: %1\n请查看日志获取详细信息").arg(pluginId));
            }
            // updatePluginList 会通过信号槽自动调用
        });
    } else {
        m_loadButton->setEnabled(true);
    }
}

void MainWindow::onUnloadPlugin()
{
    QListWidgetItem* item = m_pluginList->currentItem();
    if (!item) {
        QMessageBox::information(this, "提示", "请先选择一个插件");
        return;
    }
    
    QString pluginId = item->data(Qt::UserRole).toString();
    if (pluginId.isEmpty()) {
        return;
    }
    
    if (m_framework && m_framework->pluginManager()) {
        if (m_framework->pluginManager()->unloadPlugin(pluginId)) {
            m_statusLabel->setText(QString("正在卸载插件: %1").arg(pluginId));
        } else {
            QMessageBox::warning(this, "错误", QString("卸载插件失败: %1").arg(pluginId));
        }
    }
}

void MainWindow::onRefreshPlugins()
{
    if (m_framework && m_framework->pluginManager()) {
        m_framework->pluginManager()->scanPlugins();
        updatePluginList();
        m_statusLabel->setText("插件列表已刷新");
    }
}

void MainWindow::onPluginSelected()
{
    QListWidgetItem* item = m_pluginList->currentItem();
    if (!item) {
        return;
    }
    
    QString pluginId = item->data(Qt::UserRole).toString();
    if (pluginId.isEmpty()) {
        return;
    }
    
    if (m_framework && m_framework->pluginManager()) {
        Eagle::Core::IPlugin* plugin = m_framework->pluginManager()->getPlugin(pluginId);
        if (plugin) {
            QWidget* widget = plugin->createWidget(this);
            if (widget) {
                QDialog* dialog = new QDialog(this);
                QVBoxLayout* layout = new QVBoxLayout(dialog);
                layout->addWidget(widget);
                dialog->setWindowTitle(QString("插件: %1").arg(pluginId));
                dialog->exec();
                delete dialog;
            }
        }
    }
}
