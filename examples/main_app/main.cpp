#include <QtWidgets/QApplication>
#include "eagle/core/Framework.h"
#include "eagle/core/Logger.h"
#include "MainWindow.h"
#include <QtCore/QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("Eagle Framework Demo");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Eagle");
    
    // 初始化框架
    Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
    if (!framework->initialize()) {
        Eagle::Core::Logger::error("Main", "框架初始化失败");
        return -1;
    }
    
    // 创建主窗口
    MainWindow window;
    window.show();
    
    // 运行应用
    int ret = app.exec();
    
    // 关闭框架
    framework->shutdown();
    Eagle::Core::Framework::destroy();
    
    return ret;
}
