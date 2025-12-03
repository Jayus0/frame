#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include "eagle/core/Framework.h"

QT_BEGIN_NAMESPACE
class QListWidget;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
private slots:
    void onLoadPlugin();
    void onUnloadPlugin();
    void onRefreshPlugins();
    void onPluginSelected();
    
private:
    void setupUI();
    void updatePluginList();
    
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QListWidget* m_pluginList;
    QPushButton* m_loadButton;
    QPushButton* m_unloadButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    
    Eagle::Core::Framework* m_framework;
};

#endif // MAINWINDOW_H
