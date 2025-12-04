#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QStandardPaths>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <iostream>
#include "eagle/core/Framework.h"
#include "eagle/core/BackupManager.h"
#include "eagle/core/TestRunner.h"
#include "eagle/core/TestCaseBase.h"
#include "eagle/core/HotReloadManager.h"
#include "eagle/core/FailoverManager.h"

/**
 * @brief Eagle Framework CLI工具
 * 
 * 提供项目创建、插件生成、配置管理、调试等功能
 */
class EagleCLI {
public:
    EagleCLI() {}
    
    int run(int argc, char *argv[]) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("eagle-cli");
        app.setApplicationVersion("1.0.0");
        
        QCommandLineParser parser;
        parser.setApplicationDescription("Eagle Framework Command Line Tool");
        parser.addHelpOption();
        parser.addVersionOption();
        
        // 添加子命令
        QCommandLineOption createOption("create", "Create project or plugin");
        parser.addOption(createOption);
        
        QCommandLineOption configOption("config", "Manage configuration");
        parser.addOption(configOption);
        
        QCommandLineOption debugOption("debug", "Debug commands");
        parser.addOption(debugOption);
        
        QCommandLineOption backupOption("backup", "Backup management");
        parser.addOption(backupOption);
        
        QCommandLineOption testOption("test", "Run tests");
        parser.addOption(testOption);
        
        QCommandLineOption hotreloadOption("hotreload", "Hot reload plugins");
        parser.addOption(hotreloadOption);
        
        QCommandLineOption failoverOption("failover", "Failover management");
        parser.addOption(failoverOption);
        
        // 解析命令行参数
        parser.process(app);
        
        QStringList args = parser.positionalArguments();
        QString command = args.isEmpty() ? QString() : args.first();
        
        // 处理命令
        if (parser.isSet(createOption) || command == "create") {
            return handleCreate(args.mid(1));
        } else if (parser.isSet(configOption) || command == "config") {
            return handleConfig(args.mid(1));
        } else if (parser.isSet(debugOption) || command == "debug") {
            return handleDebug(args.mid(1));
        } else if (parser.isSet(backupOption) || command == "backup") {
            return handleBackup(args.mid(1));
        } else if (parser.isSet(testOption) || command == "test") {
            return handleTest(args.mid(1));
        } else if (parser.isSet(hotreloadOption) || command == "hotreload") {
            return handleHotReload(args.mid(1));
        } else if (parser.isSet(failoverOption) || command == "failover") {
            return handleFailover(args.mid(1));
        } else if (command.isEmpty()) {
            parser.showHelp(0);
            return 0;
        } else {
            std::cerr << "Unknown command: " << command.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli --help' for usage information." << std::endl;
            return 1;
        }
    }
    
private:
    int handleCreate(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli create <project|plugin> [options]" << std::endl;
            return 1;
        }
        
        QString type = args.first();
        if (type == "project") {
            return createProject(args.mid(1));
        } else if (type == "plugin") {
            return createPlugin(args.mid(1));
        } else {
            std::cerr << "Unknown create type: " << type.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli create project' or 'eagle-cli create plugin'" << std::endl;
            return 1;
        }
    }
    
    int createProject(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli create project <project-name> [options]" << std::endl;
            return 1;
        }
        
        QString projectName = args.first();
        QString projectPath = QDir::current().absoluteFilePath(projectName);
        
        // 检查目录是否存在
        if (QDir(projectPath).exists()) {
            std::cerr << "Error: Directory already exists: " << projectPath.toStdString() << std::endl;
            return 1;
        }
        
        std::cout << "Creating project: " << projectName.toStdString() << std::endl;
        std::cout << "Path: " << projectPath.toStdString() << std::endl;
        
        // 创建项目目录结构
        QDir dir;
        if (!dir.mkpath(projectPath)) {
            std::cerr << "Error: Failed to create project directory" << std::endl;
            return 1;
        }
        
        // 创建子目录
        dir.mkpath(projectPath + "/src");
        dir.mkpath(projectPath + "/include");
        dir.mkpath(projectPath + "/plugins");
        dir.mkpath(projectPath + "/config");
        dir.mkpath(projectPath + "/build");
        
        // 创建CMakeLists.txt
        createProjectCMakeLists(projectPath, projectName);
        
        // 创建main.cpp
        createProjectMainCpp(projectPath, projectName);
        
        // 创建README.md
        createProjectReadme(projectPath, projectName);
        
        // 创建配置文件
        createProjectConfig(projectPath);
        
        std::cout << "Project created successfully!" << std::endl;
        std::cout << "Next steps:" << std::endl;
        std::cout << "  cd " << projectName.toStdString() << std::endl;
        std::cout << "  mkdir build && cd build" << std::endl;
        std::cout << "  cmake .." << std::endl;
        std::cout << "  make" << std::endl;
        
        return 0;
    }
    
    int createPlugin(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli create plugin <plugin-name> [--type=<type>]" << std::endl;
            std::cerr << "  type: service (default), ui, tool" << std::endl;
            return 1;
        }
        
        QString pluginName = args.first();
        QString pluginType = "service";
        
        // 解析选项
        for (int i = 1; i < args.size(); ++i) {
            QString arg = args[i];
            if (arg.startsWith("--type=")) {
                pluginType = arg.mid(7);
            } else if (arg == "--type" && i + 1 < args.size()) {
                pluginType = args[++i];
            }
        }
        
        QString pluginPath = QDir::current().absoluteFilePath(pluginName);
        
        // 检查目录是否存在
        if (QDir(pluginPath).exists()) {
            std::cerr << "Error: Directory already exists: " << pluginPath.toStdString() << std::endl;
            return 1;
        }
        
        std::cout << "Creating plugin: " << pluginName.toStdString() << std::endl;
        std::cout << "Type: " << pluginType.toStdString() << std::endl;
        std::cout << "Path: " << pluginPath.toStdString() << std::endl;
        
        // 创建插件目录结构
        QDir dir;
        if (!dir.mkpath(pluginPath)) {
            std::cerr << "Error: Failed to create plugin directory" << std::endl;
            return 1;
        }
        
        // 创建子目录
        dir.mkpath(pluginPath + "/src");
        dir.mkpath(pluginPath + "/include");
        
        // 创建插件文件
        createPluginFiles(pluginPath, pluginName, pluginType);
        
        std::cout << "Plugin created successfully!" << std::endl;
        
        return 0;
    }
    
    int handleConfig(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli config <get|set|list> [key] [value]" << std::endl;
            return 1;
        }
        
        QString command = args.first();
        if (command == "get") {
            return configGet(args.mid(1));
        } else if (command == "set") {
            return configSet(args.mid(1));
        } else if (command == "list") {
            return configList();
        } else {
            std::cerr << "Unknown config command: " << command.toStdString() << std::endl;
            return 1;
        }
    }
    
    int configGet(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli config get <key>" << std::endl;
            return 1;
        }
        
        QString key = args.first();
        QString configPath = getConfigPath();
        
        QJsonObject config = loadConfig(configPath);
        if (config.contains(key)) {
            QJsonValue value = config[key];
            std::cout << value.toVariant().toString().toStdString() << std::endl;
            return 0;
        } else {
            std::cerr << "Key not found: " << key.toStdString() << std::endl;
            return 1;
        }
    }
    
    int configSet(const QStringList& args) {
        if (args.size() < 2) {
            std::cerr << "Usage: eagle-cli config set <key> <value>" << std::endl;
            return 1;
        }
        
        QString key = args[0];
        QString value = args[1];
        QString configPath = getConfigPath();
        
        QJsonObject config = loadConfig(configPath);
        config[key] = value;
        
        if (saveConfig(configPath, config)) {
            std::cout << "Configuration updated: " << key.toStdString() << " = " << value.toStdString() << std::endl;
            return 0;
        } else {
            std::cerr << "Error: Failed to save configuration" << std::endl;
            return 1;
        }
    }
    
    int configList() {
        QString configPath = getConfigPath();
        QJsonObject config = loadConfig(configPath);
        
        std::cout << "Configuration:" << std::endl;
        for (auto it = config.begin(); it != config.end(); ++it) {
            std::cout << "  " << it.key().toStdString() << " = " 
                      << it.value().toVariant().toString().toStdString() << std::endl;
        }
        
        return 0;
    }
    
    int handleDebug(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli debug <list-plugins|list-services|health>" << std::endl;
            return 1;
        }
        
        QString command = args.first();
        if (command == "list-plugins") {
            return debugListPlugins();
        } else if (command == "list-services") {
            return debugListServices();
        } else if (command == "health") {
            return debugHealth();
        } else {
            std::cerr << "Unknown debug command: " << command.toStdString() << std::endl;
            return 1;
        }
    }
    
    int debugListPlugins() {
        std::cout << "Debug: list-plugins (not implemented yet)" << std::endl;
        std::cout << "This command requires a running Eagle Framework instance." << std::endl;
        return 0;
    }
    
    int debugListServices() {
        std::cout << "Debug: list-services (not implemented yet)" << std::endl;
        std::cout << "This command requires a running Eagle Framework instance." << std::endl;
        return 0;
    }
    
    int debugHealth() {
        std::cout << "Debug: health (not implemented yet)" << std::endl;
        std::cout << "This command requires a running Eagle Framework instance." << std::endl;
        return 0;
    }
    
    // 辅助函数
    void createProjectCMakeLists(const QString& path, const QString& projectName) {
        QFile file(path + "/CMakeLists.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "cmake_minimum_required(VERSION 3.10)\n";
            out << "project(" << projectName << ")\n\n";
            out << "set(CMAKE_CXX_STANDARD 17)\n";
            out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
            out << "# Find Qt5\n";
            out << "find_package(Qt5 REQUIRED COMPONENTS Core Widgets)\n\n";
            out << "# Add executable\n";
            out << "add_executable(" << projectName << " src/main.cpp)\n\n";
            out << "# Link libraries\n";
            out << "target_link_libraries(" << projectName << " Qt5::Core Qt5::Widgets)\n";
            file.close();
        }
    }
    
    void createProjectMainCpp(const QString& path, const QString& projectName) {
        QFile file(path + "/src/main.cpp");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "#include <QtCore/QCoreApplication>\n";
            out << "#include <QtCore/QDebug>\n\n";
            out << "int main(int argc, char *argv[])\n";
            out << "{\n";
            out << "    QCoreApplication app(argc, argv);\n\n";
            out << "    qDebug() << \"Hello from " << projectName << "!\";\n\n";
            out << "    return 0;\n";
            out << "}\n";
            file.close();
        }
    }
    
    void createProjectReadme(const QString& path, const QString& projectName) {
        QFile file(path + "/README.md");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# " << projectName << "\n\n";
            out << "Eagle Framework Project\n\n";
            out << "## Build\n\n";
            out << "```bash\n";
            out << "mkdir build && cd build\n";
            out << "cmake ..\n";
            out << "make\n";
            out << "```\n\n";
            out << "## Run\n\n";
            out << "```bash\n";
            out << "./" << projectName << "\n";
            out << "```\n";
            file.close();
        }
    }
    
    void createProjectConfig(const QString& path) {
        QJsonObject config;
        config["app_name"] = "EagleApp";
        config["version"] = "1.0.0";
        
        QFile file(path + "/config/app.json");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            QJsonDocument doc(config);
            out << doc.toJson();
            file.close();
        }
    }
    
    void createPluginFiles(const QString& path, const QString& pluginName, const QString& type) {
        // 创建CMakeLists.txt
        QFile cmakeFile(path + "/CMakeLists.txt");
        if (cmakeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&cmakeFile);
            out << "cmake_minimum_required(VERSION 3.10)\n";
            out << "project(" << pluginName << ")\n\n";
            out << "set(CMAKE_CXX_STANDARD 17)\n";
            out << "find_package(Qt5 REQUIRED COMPONENTS Core)\n\n";
            out << "add_library(" << pluginName << " SHARED src/" << pluginName << ".cpp)\n";
            out << "target_link_libraries(" << pluginName << " Qt5::Core)\n";
            cmakeFile.close();
        }
        
        // 创建头文件
        QString className = pluginName;
        className[0] = className[0].toUpper();
        
        QFile headerFile(path + "/include/" + pluginName + ".h");
        if (headerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&headerFile);
            out << "#ifndef " << pluginName.toUpper() << "_H\n";
            out << "#define " << pluginName.toUpper() << "_H\n\n";
            out << "#include <QtCore/QObject>\n";
            out << "#include \"eagle/core/IPlugin.h\"\n\n";
            out << "class " << className << " : public QObject, public Eagle::Core::IPlugin\n";
            out << "{\n";
            out << "    Q_OBJECT\n";
            out << "    Q_PLUGIN_METADATA(IID \"com.eagle.plugin\" FILE \"" << pluginName << ".json\")\n";
            out << "    Q_INTERFACES(Eagle::Core::IPlugin)\n\n";
            out << "public:\n";
            out << "    explicit " << className << "(QObject* parent = nullptr);\n";
            out << "    ~" << className << "();\n\n";
            out << "    // IPlugin interface\n";
            out << "    QString name() const override;\n";
            out << "    QString version() const override;\n";
            out << "    bool initialize() override;\n";
            out << "    void shutdown() override;\n";
            out << "};\n\n";
            out << "#endif // " << pluginName.toUpper() << "_H\n";
            headerFile.close();
        }
        
        // 创建源文件
        QFile cppFile(path + "/src/" + pluginName + ".cpp");
        if (cppFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&cppFile);
            out << "#include \"" << pluginName << ".h\"\n";
            out << "#include <QtCore/QDebug>\n\n";
            out << className << "::" << className << "(QObject* parent)\n";
            out << "    : QObject(parent)\n";
            out << "{\n";
            out << "}\n\n";
            out << className << "::~" << className << "()\n";
            out << "{\n";
            out << "}\n\n";
            out << "QString " << className << "::name() const\n";
            out << "{\n";
            out << "    return \"" << pluginName << "\";\n";
            out << "}\n\n";
            out << "QString " << className << "::version() const\n";
            out << "{\n";
            out << "    return \"1.0.0\";\n";
            out << "}\n\n";
            out << "bool " << className << "::initialize()\n";
            out << "{\n";
            out << "    qDebug() << \"" << pluginName << " initialized\";\n";
            out << "    return true;\n";
            out << "}\n\n";
            out << "void " << className << "::shutdown()\n";
            out << "{\n";
            out << "    qDebug() << \"" << pluginName << " shutdown\";\n";
            out << "}\n";
            cppFile.close();
        }
        
        // 创建README.md
        QFile readmeFile(path + "/README.md");
        if (readmeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readmeFile);
            out << "# " << pluginName << "\n\n";
            out << "Eagle Framework Plugin (" << type << ")\n\n";
            out << "## Build\n\n";
            out << "```bash\n";
            out << "mkdir build && cd build\n";
            out << "cmake ..\n";
            out << "make\n";
            out << "```\n";
            readmeFile.close();
        }
    }
    
    QString getConfigPath() {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QDir dir;
        dir.mkpath(configDir + "/eagle");
        return configDir + "/eagle/config.json";
    }
    
    QJsonObject loadConfig(const QString& path) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            return doc.object();
        }
        return QJsonObject();
    }
    
    bool saveConfig(const QString& path, const QJsonObject& config) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonDocument doc(config);
            file.write(doc.toJson());
            return true;
        }
        return false;
    }
    
    int handleTest(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli test <run|create> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        if (subCommand == "run") {
            // 初始化框架
            Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
            if (!framework->initialize()) {
                std::cerr << "Error: Failed to initialize framework" << std::endl;
                return 1;
            }
            
            // 创建测试运行器
            Eagle::Core::TestRunner* runner = new Eagle::Core::TestRunner();
            
            // 配置运行器
            QString outputFile;
            Eagle::Core::TestReportFormat format = Eagle::Core::TestReportFormat::Console;
            bool verbose = false;
            
            for (int i = 1; i < args.size(); ++i) {
                if (args[i] == "--output" || args[i] == "-o") {
                    if (i + 1 < args.size()) {
                        outputFile = args[++i];
                    }
                } else if (args[i] == "--format" || args[i] == "-f") {
                    if (i + 1 < args.size()) {
                        QString formatStr = args[++i].toLower();
                        if (formatStr == "json") {
                            format = Eagle::Core::TestReportFormat::JSON;
                        } else if (formatStr == "html") {
                            format = Eagle::Core::TestReportFormat::HTML;
                        }
                    }
                } else if (args[i] == "--verbose" || args[i] == "-v") {
                    verbose = true;
                }
            }
            
            runner->setReportFormat(format);
            runner->setVerbose(verbose);
            if (!outputFile.isEmpty()) {
                runner->setOutputFile(outputFile);
            }
            
            // 运行测试（这里简化实现，实际应该发现和注册测试类）
            QStringList testNames;
            if (args.size() > 1 && args[1] != "--output" && args[1] != "--format" && args[1] != "--verbose") {
                testNames = args.mid(1);
            }
            
            bool success = runner->runTests(testNames);
            
            // 输出结果
            if (verbose || outputFile.isEmpty()) {
                QString report = runner->generateReport(format);
                std::cout << report.toStdString() << std::endl;
            }
            
            delete runner;
            return success ? 0 : 1;
        } else if (subCommand == "create") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli test create <plugin|config|service> <name>" << std::endl;
                return 1;
            }
            
            QString type = args[1];
            QString name = args.size() > 2 ? args[2] : "Test";
            
            QString templateFile;
            QString outputFile;
            
            if (type == "plugin") {
                templateFile = "templates/test_plugin_test.cpp.template";
                outputFile = QString("tests/%1Test.cpp").arg(name);
            } else if (type == "config") {
                templateFile = "templates/test_config_test.cpp.template";
                outputFile = QString("tests/%1Test.cpp").arg(name);
            } else {
                std::cerr << "Unknown test type: " << type.toStdString() << std::endl;
                return 1;
            }
            
            // 读取模板
            QFile templateFileHandle(templateFile);
            if (!templateFileHandle.exists()) {
                std::cerr << "Error: Template file not found: " << templateFile.toStdString() << std::endl;
                return 1;
            }
            
            if (!templateFileHandle.open(QIODevice::ReadOnly | QIODevice::Text)) {
                std::cerr << "Error: Cannot open template file" << std::endl;
                return 1;
            }
            
            QString content = templateFileHandle.readAll();
            templateFileHandle.close();
            
            // 替换占位符
            content.replace("{{PLUGIN_NAME}}", name);
            content.replace("{{PLUGIN_ID}}", name.toLower());
            
            // 确保输出目录存在
            QDir dir;
            dir.mkpath("tests");
            
            // 写入文件
            QFile outputFileHandle(outputFile);
            if (!outputFileHandle.open(QIODevice::WriteOnly | QIODevice::Text)) {
                std::cerr << "Error: Cannot create test file: " << outputFile.toStdString() << std::endl;
                return 1;
            }
            
            QTextStream out(&outputFileHandle);
            out << content;
            outputFileHandle.close();
            
            std::cout << "Test file created: " << outputFile.toStdString() << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown test command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli test run' or 'eagle-cli test create'" << std::endl;
            return 1;
        }
    }
    
    int handleHotReload(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli hotreload <reload|list> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::HotReloadManager* hotReloadManager = framework->hotReloadManager();
        if (!hotReloadManager) {
            std::cerr << "Error: HotReloadManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "reload") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli hotreload reload <plugin-id> [--force]" << std::endl;
                return 1;
            }
            
            QString pluginId = args[1];
            bool force = args.contains("--force");
            
            Eagle::Core::HotReloadResult result = hotReloadManager->reloadPlugin(pluginId, force);
            
            if (result.success) {
                std::cout << "Plugin reloaded successfully: " << pluginId.toStdString() << std::endl;
                std::cout << "Duration: " << result.durationMs << "ms" << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to reload plugin: " << pluginId.toStdString() << std::endl;
                std::cerr << "Error message: " << result.errorMessage.toStdString() << std::endl;
                return 1;
            }
        } else if (subCommand == "list") {
            QStringList reloadablePlugins = hotReloadManager->getReloadablePlugins();
            std::cout << "Reloadable plugins (" << reloadablePlugins.size() << "):" << std::endl;
            for (const QString& pluginId : reloadablePlugins) {
                Eagle::Core::HotReloadStatus status = hotReloadManager->getStatus(pluginId);
                QString statusStr;
                switch (status) {
                    case Eagle::Core::HotReloadStatus::Idle:
                        statusStr = "Idle";
                        break;
                    case Eagle::Core::HotReloadStatus::Success:
                        statusStr = "Success";
                        break;
                    case Eagle::Core::HotReloadStatus::Failed:
                        statusStr = "Failed";
                        break;
                    default:
                        statusStr = "In Progress";
                        break;
                }
                std::cout << "  " << pluginId.toStdString() << " [" << statusStr.toStdString() << "]" << std::endl;
            }
            return 0;
        } else {
            std::cerr << "Unknown hotreload command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli hotreload reload' or 'eagle-cli hotreload list'" << std::endl;
            return 1;
        }
    }
    
    int handleFailover(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli failover <list|register|nodes|switch|history> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::FailoverManager* failoverManager = framework->failoverManager();
        if (!failoverManager) {
            std::cerr << "Error: FailoverManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "list") {
            QStringList services = failoverManager->getRegisteredServices();
            std::cout << "Registered services (" << services.size() << "):" << std::endl;
            for (const QString& serviceName : services) {
                Eagle::Core::ServiceStatus status = failoverManager->getServiceStatus(serviceName);
                Eagle::Core::ServiceNode primary = failoverManager->getCurrentPrimary(serviceName);
                QString statusStr;
                switch (status) {
                    case Eagle::Core::ServiceStatus::Healthy:
                        statusStr = "Healthy";
                        break;
                    case Eagle::Core::ServiceStatus::Degraded:
                        statusStr = "Degraded";
                        break;
                    case Eagle::Core::ServiceStatus::Unhealthy:
                        statusStr = "Unhealthy";
                        break;
                    case Eagle::Core::ServiceStatus::Failed:
                        statusStr = "Failed";
                        break;
                }
                std::cout << "  " << serviceName.toStdString() << " [" << statusStr.toStdString() 
                          << "] Primary: " << primary.id.toStdString() << std::endl;
            }
            return 0;
        } else if (subCommand == "nodes") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli failover nodes <service-name>" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QList<Eagle::Core::ServiceNode> nodes = failoverManager->getNodes(serviceName);
            std::cout << "Nodes for service " << serviceName.toStdString() << " (" << nodes.size() << "):" << std::endl;
            for (const Eagle::Core::ServiceNode& node : nodes) {
                QString roleStr = (node.role == Eagle::Core::ServiceRole::Primary) ? "Primary" : "Standby";
                QString statusStr;
                switch (node.status) {
                    case Eagle::Core::ServiceStatus::Healthy:
                        statusStr = "Healthy";
                        break;
                    case Eagle::Core::ServiceStatus::Degraded:
                        statusStr = "Degraded";
                        break;
                    case Eagle::Core::ServiceStatus::Unhealthy:
                        statusStr = "Unhealthy";
                        break;
                    case Eagle::Core::ServiceStatus::Failed:
                        statusStr = "Failed";
                        break;
                }
                std::cout << "  " << node.id.toStdString() << " [" << roleStr.toStdString() 
                          << ", " << statusStr.toStdString() << "] " << node.endpoint.toStdString() << std::endl;
            }
            return 0;
        } else if (subCommand == "switch") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli failover switch <service-name> [target-node-id]" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QString targetNodeId = args.size() > 2 ? args[2] : QString();
            
            bool success = failoverManager->performFailover(serviceName, targetNodeId);
            if (success) {
                Eagle::Core::ServiceNode newPrimary = failoverManager->getCurrentPrimary(serviceName);
                std::cout << "Failover successful. New primary: " << newPrimary.id.toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to perform failover" << std::endl;
                return 1;
            }
        } else if (subCommand == "history") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli failover history <service-name> [limit]" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            int limit = args.size() > 2 ? args[2].toInt() : 10;
            
            QList<Eagle::Core::FailoverEvent> events = failoverManager->getFailoverHistory(serviceName, limit);
            std::cout << "Failover history for " << serviceName.toStdString() << " (" << events.size() << "):" << std::endl;
            for (const Eagle::Core::FailoverEvent& event : events) {
                std::cout << "  " << event.timestamp.toString(Qt::ISODate).toStdString() 
                          << " " << event.fromNodeId.toStdString() << " -> " << event.toNodeId.toStdString()
                          << " (" << event.reason.toStdString() << ") " 
                          << (event.success ? "[Success]" : "[Failed]") << std::endl;
            }
            return 0;
        } else {
            std::cerr << "Unknown failover command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli failover list|nodes|switch|history'" << std::endl;
            return 1;
        }
    }
};

int main(int argc, char *argv[])
{
    EagleCLI cli;
    return cli.run(argc, argv);
}
