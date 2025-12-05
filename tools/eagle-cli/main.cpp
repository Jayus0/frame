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
#include "eagle/core/PluginManager.h"
#include "eagle/core/BackupManager.h"
#include "eagle/core/TestRunner.h"
#include "eagle/core/TestCaseBase.h"
#include "eagle/core/HotReloadManager.h"
#include "eagle/core/FailoverManager.h"
#include "eagle/core/DiagnosticManager.h"
#include "eagle/core/ResourceMonitor.h"
#include "eagle/core/ConfigEncryption.h"
#include "eagle/core/ConfigSchema.h"
#include "eagle/core/ConfigVersion.h"
#include "eagle/core/ConfigFormat.h"
#include "eagle/core/PluginSignature.h"
#include "eagle/core/LoadBalancer.h"
#include "eagle/core/AsyncServiceCall.h"
#include "eagle/core/SslConfig.h"
#include "eagle/core/SystemHealth.h"

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
        
        QCommandLineOption diagnosticOption("diagnostic", "Diagnostic tools");
        parser.addOption(diagnosticOption);
        
        QCommandLineOption resourceOption("resource", "Resource monitoring");
        parser.addOption(resourceOption);
        
        QCommandLineOption dependencyOption("dependency", "Dependency management");
        parser.addOption(dependencyOption);
        
        QCommandLineOption encryptionOption("encryption", "Encryption management");
        parser.addOption(encryptionOption);
        
        QCommandLineOption schemaOption("schema", "Schema validation");
        parser.addOption(schemaOption);
        
        QCommandLineOption signatureOption("signature", "Plugin signature management");
        parser.addOption(signatureOption);
        
        QCommandLineOption loadbalanceOption("loadbalance", "Load balancing management");
        parser.addOption(loadbalanceOption);
        
        QCommandLineOption configVersionOption("config-version", "Config version management");
        parser.addOption(configVersionOption);
        
        QCommandLineOption asyncOption("async", "Async service call");
        parser.addOption(asyncOption);
        
        QCommandLineOption sslOption("ssl", "SSL/TLS management");
        parser.addOption(sslOption);
        
        QCommandLineOption healthOption("health", "System health check");
        parser.addOption(healthOption);
        
        QCommandLineOption pluginsOption("plugins", "Plugin management");
        parser.addOption(pluginsOption);
        
        // 解析命令行参数
        parser.process(app);
        
        QStringList args = parser.positionalArguments();
        QString command = args.isEmpty() ? QString() : args.first();
        
        // 处理命令
        if (parser.isSet(createOption) || command == "create") {
            return handleCreate(args.mid(1));
        } else if (parser.isSet(configOption) || command == "config") {
            // 检查是否是config format子命令
            if (args.size() > 1 && args[1] == "format") {
                return handleConfigFormat(args.mid(2));
            }
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
        } else if (parser.isSet(diagnosticOption) || command == "diagnostic") {
            return handleDiagnostic(args.mid(1));
        } else if (parser.isSet(resourceOption) || command == "resource") {
            return handleResource(args.mid(1));
        } else if (parser.isSet(dependencyOption) || command == "dependency") {
            return handleDependency(args.mid(1));
        } else if (parser.isSet(encryptionOption) || command == "encryption") {
            return handleEncryption(args.mid(1));
        } else if (parser.isSet(schemaOption) || command == "schema") {
            return handleSchema(args.mid(1));
        } else if (parser.isSet(signatureOption) || command == "signature") {
            return handleSignature(args.mid(1));
        } else if (parser.isSet(loadbalanceOption) || command == "loadbalance") {
            return handleLoadBalance(args.mid(1));
        } else if (parser.isSet(configVersionOption) || command == "config-version" || command == "config") {
            return handleConfigVersion(args.mid(1));
        } else if (parser.isSet(asyncOption) || command == "async") {
            return handleAsync(args.mid(1));
        } else if (parser.isSet(sslOption) || command == "ssl" || command == "tls") {
            return handleSsl(args.mid(1));
        } else if (parser.isSet(healthOption) || command == "health") {
            return handleHealth(args.mid(1));
        } else if (parser.isSet(pluginsOption) || command == "plugins") {
            return handlePlugins(args.mid(1));
        } else if (command == "audit") {
            return handleAudit(args.mid(1));
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
    
    int handleDiagnostic(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli diagnostic <stacktrace|memory|deadlock> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::DiagnosticManager* diagnosticManager = framework->diagnosticManager();
        if (!diagnosticManager) {
            std::cerr << "Error: DiagnosticManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "stacktrace") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli diagnostic stacktrace <capture|list|show> [options]" << std::endl;
                return 1;
            }
            
            QString action = args[1];
            if (action == "capture") {
                QString message = args.size() > 2 ? args[2] : QString();
                Eagle::Core::StackTrace trace = diagnosticManager->captureStackTrace(message);
                std::cout << "Stack trace captured: " << trace.id.toStdString() << std::endl;
                std::cout << trace.toString().toStdString() << std::endl;
                return 0;
            } else if (action == "list") {
                QStringList traceIds = diagnosticManager->getStackTraceIds();
                std::cout << "Stack traces (" << traceIds.size() << "):" << std::endl;
                for (const QString& traceId : traceIds) {
                    Eagle::Core::StackTrace trace = diagnosticManager->getStackTrace(traceId);
                    std::cout << "  " << traceId.toStdString() << " [" 
                              << trace.timestamp.toString(Qt::ISODate).toStdString() << "] "
                              << trace.threadName.toStdString() << std::endl;
                }
                return 0;
            } else if (action == "show") {
                if (args.size() < 3) {
                    std::cerr << "Usage: eagle-cli diagnostic stacktrace show <trace-id>" << std::endl;
                    return 1;
                }
                
                QString traceId = args[2];
                Eagle::Core::StackTrace trace = diagnosticManager->getStackTrace(traceId);
                if (trace.id.isEmpty()) {
                    std::cerr << "Error: Stack trace not found: " << traceId.toStdString() << std::endl;
                    return 1;
                }
                
                std::cout << trace.toString().toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown stacktrace action: " << action.toStdString() << std::endl;
                return 1;
            }
        } else if (subCommand == "memory") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli diagnostic memory <snapshot|list|compare> [options]" << std::endl;
                return 1;
            }
            
            QString action = args[1];
            if (action == "snapshot") {
                QString name = args.size() > 2 ? args[2] : QString();
                Eagle::Core::MemorySnapshot snapshot = diagnosticManager->captureMemorySnapshot(name);
                std::cout << "Memory snapshot captured: " << snapshot.id.toStdString() << std::endl;
                std::cout << "Total Memory: " << (snapshot.totalMemory / 1024 / 1024) << " MB" << std::endl;
                std::cout << "Used Memory: " << (snapshot.usedMemory / 1024 / 1024) << " MB" << std::endl;
                std::cout << "Heap Size: " << (snapshot.heapSize / 1024 / 1024) << " MB" << std::endl;
                std::cout << "Usage: " << snapshot.memoryUsagePercent() << "%" << std::endl;
                return 0;
            } else if (action == "list") {
                QStringList snapshotIds = diagnosticManager->getMemorySnapshotIds();
                std::cout << "Memory snapshots (" << snapshotIds.size() << "):" << std::endl;
                for (const QString& snapshotId : snapshotIds) {
                    Eagle::Core::MemorySnapshot snapshot = diagnosticManager->getMemorySnapshot(snapshotId);
                    std::cout << "  " << snapshotId.toStdString() << " [" 
                              << snapshot.timestamp.toString(Qt::ISODate).toStdString() << "] "
                              << (snapshot.usedMemory / 1024 / 1024) << " MB (" 
                              << snapshot.memoryUsagePercent() << "%)" << std::endl;
                }
                return 0;
            } else if (action == "compare") {
                if (args.size() < 4) {
                    std::cerr << "Usage: eagle-cli diagnostic memory compare <snapshot-id1> <snapshot-id2>" << std::endl;
                    return 1;
                }
                
                QString snapshotId1 = args[2];
                QString snapshotId2 = args[3];
                QVariantMap comparison = diagnosticManager->compareSnapshots(snapshotId1, snapshotId2);
                
                if (comparison.isEmpty()) {
                    std::cerr << "Error: Failed to compare snapshots" << std::endl;
                    return 1;
                }
                
                std::cout << "Memory Comparison:" << std::endl;
                std::cout << "  Memory Diff: " << (comparison["memoryDiff"].toLongLong() / 1024 / 1024) << " MB" << std::endl;
                std::cout << "  Heap Diff: " << (comparison["heapDiff"].toLongLong() / 1024 / 1024) << " MB" << std::endl;
                std::cout << "  Possible Leak: " << (comparison["possibleLeak"].toBool() ? "Yes" : "No") << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown memory action: " << action.toStdString() << std::endl;
                return 1;
            }
        } else if (subCommand == "deadlock") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli diagnostic deadlock <list|enable|disable>" << std::endl;
                return 1;
            }
            
            QString action = args[1];
            if (action == "list") {
                QList<Eagle::Core::DeadlockInfo> deadlocks = diagnosticManager->getDeadlocks();
                std::cout << "Deadlocks (" << deadlocks.size() << "):" << std::endl;
                for (const Eagle::Core::DeadlockInfo& deadlock : deadlocks) {
                    std::cout << "  " << deadlock.id.toStdString() << " [" 
                              << deadlock.timestamp.toString(Qt::ISODate).toStdString() << "]" << std::endl;
                    std::cout << "    Threads: " << deadlock.threadIds.join(", ").toStdString() << std::endl;
                    std::cout << "    Locks: " << deadlock.lockIds.join(", ").toStdString() << std::endl;
                }
                return 0;
            } else if (action == "enable") {
                diagnosticManager->setDeadlockDetectionEnabled(true);
                std::cout << "Deadlock detection enabled" << std::endl;
                return 0;
            } else if (action == "disable") {
                diagnosticManager->setDeadlockDetectionEnabled(false);
                std::cout << "Deadlock detection disabled" << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown deadlock action: " << action.toStdString() << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown diagnostic command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli diagnostic stacktrace|memory|deadlock'" << std::endl;
            return 1;
        }
    }
    
    int handleResource(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli resource <show|set|events> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ResourceMonitor* resourceMonitor = framework->resourceMonitor();
        if (!resourceMonitor) {
            std::cerr << "Error: ResourceMonitor not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "show") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli resource show <plugin-id>" << std::endl;
                return 1;
            }
            
            QString pluginId = args[1];
            Eagle::Core::ResourceUsage usage = resourceMonitor->getResourceUsage(pluginId);
            Eagle::Core::ResourceLimits limits = resourceMonitor->getResourceLimits(pluginId);
            
            if (!usage.isValid()) {
                std::cerr << "Error: Plugin not found or not monitored: " << pluginId.toStdString() << std::endl;
                return 1;
            }
            
            std::cout << "Resource Usage for " << pluginId.toStdString() << ":" << std::endl;
            std::cout << "  Memory: " << (usage.memoryBytes / 1024 / 1024) << " MB" << std::endl;
            std::cout << "  CPU: " << usage.cpuPercent << "%" << std::endl;
            std::cout << "  Threads: " << usage.threadCount << std::endl;
            std::cout << "  Last Update: " << usage.lastUpdate.toString(Qt::ISODate).toStdString() << std::endl;
            std::cout << std::endl;
            std::cout << "Resource Limits:" << std::endl;
            std::cout << "  Max Memory: " << (limits.maxMemoryMB < 0 ? "Unlimited" : QString::number(limits.maxMemoryMB).append(" MB").toStdString()) << std::endl;
            std::cout << "  Max CPU: " << (limits.maxCpuPercent < 0 ? "Unlimited" : QString::number(limits.maxCpuPercent).append("%").toStdString()) << std::endl;
            std::cout << "  Max Threads: " << (limits.maxThreads < 0 ? "Unlimited" : QString::number(limits.maxThreads).toStdString()) << std::endl;
            std::cout << "  Enforcement: " << (limits.enforceLimits ? "Enabled" : "Disabled") << std::endl;
            std::cout << "  Limit Exceeded: " << (resourceMonitor->isResourceLimitExceeded(pluginId) ? "Yes" : "No") << std::endl;
            return 0;
        } else if (subCommand == "set") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli resource set <plugin-id> [--memory MB] [--cpu PERCENT] [--threads COUNT] [--enforce]" << std::endl;
                return 1;
            }
            
            QString pluginId = args[1];
            Eagle::Core::ResourceLimits limits;
            
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--memory" && i + 1 < args.size()) {
                    limits.maxMemoryMB = args[++i].toInt();
                } else if (args[i] == "--cpu" && i + 1 < args.size()) {
                    limits.maxCpuPercent = args[++i].toInt();
                } else if (args[i] == "--threads" && i + 1 < args.size()) {
                    limits.maxThreads = args[++i].toInt();
                } else if (args[i] == "--enforce") {
                    limits.enforceLimits = true;
                }
            }
            
            resourceMonitor->setResourceLimits(pluginId, limits);
            std::cout << "Resource limits set for " << pluginId.toStdString() << std::endl;
            return 0;
        } else if (subCommand == "events") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli resource events <plugin-id> [limit]" << std::endl;
                return 1;
            }
            
            QString pluginId = args[1];
            int limit = args.size() > 2 ? args[2].toInt() : 10;
            
            QList<Eagle::Core::ResourceLimitExceeded> events = resourceMonitor->getLimitExceededEvents(pluginId, limit);
            std::cout << "Resource limit exceeded events for " << pluginId.toStdString() << " (" << events.size() << "):" << std::endl;
            for (const Eagle::Core::ResourceLimitExceeded& event : events) {
                std::cout << "  " << event.timestamp.toString(Qt::ISODate).toStdString() 
                          << " [" << event.resourceType.toStdString() << "] "
                          << event.currentValue << " > " << event.limitValue << std::endl;
            }
            return 0;
        } else {
            std::cerr << "Unknown resource command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli resource show|set|events'" << std::endl;
            return 1;
        }
    }
    
    int handleDependency(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli dependency <check|circular> [plugin-id]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::PluginManager* pluginManager = framework->pluginManager();
        if (!pluginManager) {
            std::cerr << "Error: PluginManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "check") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli dependency check <plugin-id>" << std::endl;
                return 1;
            }
            
            QString pluginId = args[1];
            Eagle::Core::PluginMetadata meta = pluginManager->getPluginMetadata(pluginId);
            if (!meta.isValid()) {
                std::cerr << "Error: Plugin not found: " << pluginId.toStdString() << std::endl;
                return 1;
            }
            
            QStringList allDeps = pluginManager->resolveDependencies(pluginId);
            QStringList directDeps = meta.dependencies;
            
            std::cout << "Dependencies for " << pluginId.toStdString() << ":" << std::endl;
            std::cout << "  Direct dependencies (" << directDeps.size() << "):" << std::endl;
            for (const QString& dep : directDeps) {
                std::cout << "    - " << dep.toStdString() << std::endl;
            }
            std::cout << "  All dependencies (" << allDeps.size() << "):" << std::endl;
            for (const QString& dep : allDeps) {
                std::cout << "    - " << dep.toStdString() << std::endl;
            }
            
            // 检查循环依赖
            QStringList cyclePath;
            bool hasCycle = pluginManager->detectCircularDependencies(pluginId, cyclePath);
            std::cout << "  Circular dependency: " << (hasCycle ? "Yes" : "No") << std::endl;
            if (hasCycle) {
                std::cout << "  Cycle path: " << cyclePath.join(" -> ").toStdString() << std::endl;
            }
            
            return 0;
        } else if (subCommand == "circular") {
            QList<QStringList> allCycles = pluginManager->detectAllCircularDependencies();
            
            if (allCycles.isEmpty()) {
                std::cout << "No circular dependencies detected." << std::endl;
                return 0;
            }
            
            std::cout << "Circular dependencies found (" << allCycles.size() << "):" << std::endl;
            for (int i = 0; i < allCycles.size(); ++i) {
                const QStringList& cycle = allCycles[i];
                std::cout << "  Cycle " << (i + 1) << ": " << cycle.join(" -> ").toStdString() << std::endl;
            }
            
            return 0;
        } else {
            std::cerr << "Unknown dependency command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli dependency check|circular'" << std::endl;
            return 1;
        }
    }
    
    int handleEncryption(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli encryption <info|generate|rotate> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        if (subCommand == "info") {
            Eagle::Core::KeyVersion version = Eagle::Core::ConfigEncryption::getCurrentKeyVersion();
            std::cout << "Encryption Information:" << std::endl;
            std::cout << "  Version: " << version.version << std::endl;
            std::cout << "  Algorithm: " << (version.algorithm == Eagle::Core::EncryptionAlgorithm::AES256 ? "AES256" : "XOR") << std::endl;
            std::cout << "  Key ID: " << version.keyId.toStdString() << std::endl;
            std::cout << "  PBKDF2 Iterations: " << version.pbkdf2Iterations << std::endl;
            std::cout << "  Has Salt: " << (!version.salt.isEmpty() ? "Yes" : "No") << std::endl;
            return 0;
        } else if (subCommand == "generate") {
            int length = 32;
            if (args.size() > 1) {
                length = args[1].toInt();
                if (length < 16 || length > 64) {
                    std::cerr << "Error: Key length must be between 16 and 64 bytes" << std::endl;
                    return 1;
                }
            }
            
            QString newKey = Eagle::Core::ConfigEncryption::generateKey(length);
            std::cout << "Generated key (length " << length << "):" << std::endl;
            std::cout << newKey.toStdString() << std::endl;
            std::cout << std::endl;
            std::cout << "Warning: Save this key securely! It cannot be recovered." << std::endl;
            return 0;
        } else if (subCommand == "rotate") {
            if (args.size() < 3) {
                std::cerr << "Usage: eagle-cli encryption rotate <old-key> <new-key>" << std::endl;
                return 1;
            }
            
            QString oldKey = args[1];
            QString newKey = args[2];
            
            // 这里需要访问ConfigManager来轮换所有配置
            // 简化实现：仅设置新密钥
            Eagle::Core::ConfigEncryption::setDefaultKey(newKey);
            std::cout << "Encryption key rotated successfully." << std::endl;
            std::cout << "Note: Existing encrypted values need to be re-encrypted manually." << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown encryption command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli encryption info|generate|rotate'" << std::endl;
            return 1;
        }
    }
    
    int handleSchema(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli schema <validate|load|info> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            std::cerr << "Error: ConfigManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "validate") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli schema validate <config-file> [schema-file]" << std::endl;
                return 1;
            }
            
            QString configFile = args[1];
            QString schemaFile = args.size() > 2 ? args[2] : configManager->schemaPath();
            
            if (schemaFile.isEmpty()) {
                std::cerr << "Error: Schema file path is required" << std::endl;
                return 1;
            }
            
            // 加载配置
            QVariantMap config;
            QFile file(configFile);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                if (doc.isObject()) {
                    config = doc.object().toVariantMap();
                }
                file.close();
            } else {
                std::cerr << "Error: Cannot open config file: " << configFile.toStdString() << std::endl;
                return 1;
            }
            
            // 验证
            Eagle::Core::ConfigSchema schema;
            if (!schema.loadFromFile(schemaFile)) {
                std::cerr << "Error: Cannot load schema file: " << schemaFile.toStdString() << std::endl;
                return 1;
            }
            
            Eagle::Core::SchemaValidationResult result = schema.validate(config);
            
            if (result.valid) {
                std::cout << "Configuration is valid!" << std::endl;
                return 0;
            } else {
                std::cerr << "Configuration validation failed with " << result.errors.size() << " error(s):" << std::endl;
                for (const Eagle::Core::SchemaValidationError& error : result.errors) {
                    std::cerr << "  [" << error.path.toStdString() << "] " 
                              << error.code.toStdString() << ": " 
                              << error.message.toStdString() << std::endl;
                }
                return 1;
            }
        } else if (subCommand == "load") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli schema load <schema-file>" << std::endl;
                return 1;
            }
            
            QString schemaFile = args[1];
            configManager->setSchemaPath(schemaFile);
            std::cout << "Schema path set to: " << schemaFile.toStdString() << std::endl;
            return 0;
        } else if (subCommand == "info") {
            QString schemaPath = configManager->schemaPath();
            if (schemaPath.isEmpty()) {
                std::cout << "No schema path set." << std::endl;
                return 0;
            }
            
            std::cout << "Schema Path: " << schemaPath.toStdString() << std::endl;
            
            Eagle::Core::ConfigSchema schema;
            if (schema.loadFromFile(schemaPath)) {
                std::cout << "Title: " << schema.title().toStdString() << std::endl;
                std::cout << "Description: " << schema.description().toStdString() << std::endl;
                std::cout << "Valid: Yes" << std::endl;
            } else {
                std::cout << "Valid: No (cannot load schema file)" << std::endl;
            }
            return 0;
        } else {
            std::cerr << "Unknown schema command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli schema validate|load|info'" << std::endl;
            return 1;
        }
    }
    
    int handleSignature(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli signature <sign|verify|certificates> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        if (subCommand == "sign") {
            if (args.size() < 4) {
                std::cerr << "Usage: eagle-cli signature sign <plugin-path> <private-key-path> <output-path> [certificate-path] [algorithm]" << std::endl;
                return 1;
            }
            
            QString pluginPath = args[1];
            QString privateKeyPath = args[2];
            QString outputPath = args[3];
            QString certificatePath = args.size() > 4 ? args[4] : QString();
            QString algorithmStr = args.size() > 5 ? args[5] : "RSA-SHA256";
            
            Eagle::Core::SignatureAlgorithm algorithm = Eagle::Core::SignatureAlgorithm::RSA_SHA256;
            if (algorithmStr == "RSA-SHA512") {
                algorithm = Eagle::Core::SignatureAlgorithm::RSA_SHA512;
            } else if (algorithmStr == "SHA256") {
                algorithm = Eagle::Core::SignatureAlgorithm::SHA256;
            }
            
            bool success = Eagle::Core::PluginSignatureVerifier::sign(pluginPath, privateKeyPath, certificatePath, outputPath, algorithm);
            if (success) {
                std::cout << "Plugin signed successfully: " << outputPath.toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to sign plugin" << std::endl;
                return 1;
            }
        } else if (subCommand == "verify") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli signature verify <plugin-path> [signature-path] [crl-path]" << std::endl;
                return 1;
            }
            
            QString pluginPath = args[1];
            QString signaturePath = args.size() > 2 ? args[2] : QString();
            QString crlPath = args.size() > 3 ? args[3] : QString();
            
            if (signaturePath.isEmpty()) {
                signaturePath = Eagle::Core::PluginSignatureVerifier::findSignatureFile(pluginPath);
                if (signaturePath.isEmpty()) {
                    std::cerr << "Error: Signature file not found" << std::endl;
                    return 1;
                }
            }
            
            Eagle::Core::PluginSignature signature = Eagle::Core::PluginSignatureVerifier::loadFromFile(signaturePath);
            if (!signature.isValid()) {
                std::cerr << "Error: Invalid signature file" << std::endl;
                return 1;
            }
            
            // 检查撤销列表
            if (!crlPath.isEmpty() && Eagle::Core::PluginSignatureVerifier::isRevoked(signature, crlPath)) {
                std::cerr << "Error: Signature has been revoked" << std::endl;
                return 1;
            }
            
            bool valid = Eagle::Core::PluginSignatureVerifier::verify(pluginPath, signature);
            if (valid) {
                std::cout << "Signature verification: PASSED" << std::endl;
                std::cout << "  Signer: " << signature.signer.toStdString() << std::endl;
                std::cout << "  Algorithm: " << (signature.algorithm == Eagle::Core::SignatureAlgorithm::RSA_SHA256 ? "RSA-SHA256" :
                              signature.algorithm == Eagle::Core::SignatureAlgorithm::RSA_SHA512 ? "RSA-SHA512" : "SHA256").toStdString() << std::endl;
                std::cout << "  Sign Time: " << signature.signTime.toString(Qt::ISODate).toStdString() << std::endl;
                if (signature.certificate.isValid()) {
                    std::cout << "  Certificate Subject: " << signature.certificate.subject.toStdString() << std::endl;
                    std::cout << "  Certificate Issuer: " << signature.certificate.issuer.toStdString() << std::endl;
                }
                return 0;
            } else {
                std::cerr << "Signature verification: FAILED" << std::endl;
                return 1;
            }
        } else if (subCommand == "certificates") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli signature certificates <list|add|remove> [options]" << std::endl;
                return 1;
            }
            
            QString certCommand = args[1];
            
            if (certCommand == "list") {
                QStringList trustedRoots = Eagle::Core::PluginSignatureVerifier::getTrustedRootCertificates();
                std::cout << "Trusted Root Certificates (" << trustedRoots.size() << "):" << std::endl;
                for (const QString& rootPath : trustedRoots) {
                    Eagle::Core::CertificateInfo cert = Eagle::Core::PluginSignatureVerifier::loadCertificate(rootPath);
                    std::cout << "  " << rootPath.toStdString() << std::endl;
                    if (cert.isValid()) {
                        std::cout << "    Subject: " << cert.subject.toStdString() << std::endl;
                        std::cout << "    Issuer: " << cert.issuer.toStdString() << std::endl;
                    }
                }
                return 0;
            } else if (certCommand == "add") {
                if (args.size() < 3) {
                    std::cerr << "Usage: eagle-cli signature certificates add <certificate-path>" << std::endl;
                    return 1;
                }
                
                QString certPath = args[2];
                QStringList trustedRoots = Eagle::Core::PluginSignatureVerifier::getTrustedRootCertificates();
                if (!trustedRoots.contains(certPath)) {
                    trustedRoots.append(certPath);
                    Eagle::Core::PluginSignatureVerifier::setTrustedRootCertificates(trustedRoots);
                    std::cout << "Certificate added: " << certPath.toStdString() << std::endl;
                } else {
                    std::cout << "Certificate already in trusted list" << std::endl;
                }
                return 0;
            } else if (certCommand == "remove") {
                if (args.size() < 3) {
                    std::cerr << "Usage: eagle-cli signature certificates remove <certificate-path>" << std::endl;
                    return 1;
                }
                
                QString certPath = args[2];
                QStringList trustedRoots = Eagle::Core::PluginSignatureVerifier::getTrustedRootCertificates();
                trustedRoots.removeAll(certPath);
                Eagle::Core::PluginSignatureVerifier::setTrustedRootCertificates(trustedRoots);
                std::cout << "Certificate removed: " << certPath.toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown certificate command: " << certCommand.toStdString() << std::endl;
                std::cerr << "Use 'eagle-cli signature certificates list|add|remove'" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown signature command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli signature sign|verify|certificates'" << std::endl;
            return 1;
        }
    }
    
    int handleLoadBalance(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli loadbalance <show|set|instances> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            std::cerr << "Error: ServiceRegistry not available" << std::endl;
            return 1;
        }
        
        Eagle::Core::LoadBalancer* loadBalancer = serviceRegistry->loadBalancer();
        if (!loadBalancer) {
            std::cerr << "Error: LoadBalancer not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "show") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli loadbalance show <service-name>" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QString algorithm = serviceRegistry->getLoadBalanceAlgorithm(serviceName);
            QList<Eagle::Core::ServiceInstance> instances = loadBalancer->getInstances(serviceName);
            
            std::cout << "Load Balance Configuration for " << serviceName.toStdString() << ":" << std::endl;
            std::cout << "  Algorithm: " << algorithm.toStdString() << std::endl;
            std::cout << "  Enabled: " << (serviceRegistry->isLoadBalanceEnabled() ? "Yes" : "No") << std::endl;
            std::cout << "  Instances: " << instances.size() << std::endl;
            std::cout << std::endl;
            
            for (const Eagle::Core::ServiceInstance& instance : instances) {
                QString instanceId = loadBalancer->getInstanceIdByProvider(serviceName, instance.provider);
                std::cout << "  Instance: " << instanceId.toStdString() << std::endl;
                std::cout << "    Version: " << instance.descriptor.version.toStdString() << std::endl;
                std::cout << "    Weight: " << instance.weight << std::endl;
                std::cout << "    Active Connections: " << instance.activeConnections << std::endl;
                std::cout << "    Total Requests: " << instance.totalRequests << std::endl;
                std::cout << "    Healthy: " << (instance.healthy ? "Yes" : "No") << std::endl;
                std::cout << std::endl;
            }
            return 0;
        } else if (subCommand == "set") {
            if (args.size() < 3) {
                std::cerr << "Usage: eagle-cli loadbalance set <service-name> <algorithm> [--enabled]" << std::endl;
                std::cerr << "  Algorithms: round_robin, weighted_round_robin, least_connections, random, ip_hash" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QString algorithm = args[2];
            bool enabled = true;
            
            for (int i = 3; i < args.size(); ++i) {
                if (args[i] == "--enabled") {
                    enabled = true;
                } else if (args[i] == "--disabled") {
                    enabled = false;
                }
            }
            
            serviceRegistry->setLoadBalanceAlgorithm(serviceName, algorithm);
            serviceRegistry->setLoadBalanceEnabled(enabled);
            
            std::cout << "Load balance configuration updated for " << serviceName.toStdString() << std::endl;
            return 0;
        } else if (subCommand == "instances") {
            if (args.size() < 3) {
                std::cerr << "Usage: eagle-cli loadbalance instances <service-name> <weight|health> [instance-id] [value]" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QString action = args[2];
            
            if (action == "weight") {
                if (args.size() < 5) {
                    std::cerr << "Usage: eagle-cli loadbalance instances <service-name> weight <instance-id> <weight>" << std::endl;
                    return 1;
                }
                
                QString instanceId = args[3];
                int weight = args[4].toInt();
                
                loadBalancer->setInstanceWeight(serviceName, instanceId, weight);
                std::cout << "Instance weight updated: " << instanceId.toStdString() << " -> " << weight << std::endl;
                return 0;
            } else if (action == "health") {
                if (args.size() < 5) {
                    std::cerr << "Usage: eagle-cli loadbalance instances <service-name> health <instance-id> <true|false>" << std::endl;
                    return 1;
                }
                
                QString instanceId = args[3];
                bool healthy = args[4].toLower() == "true" || args[4] == "1";
                
                loadBalancer->setInstanceHealth(serviceName, instanceId, healthy);
                std::cout << "Instance health updated: " << instanceId.toStdString() 
                          << " -> " << (healthy ? "healthy" : "unhealthy") << std::endl;
                return 0;
            } else {
                std::cerr << "Unknown action: " << action.toStdString() << std::endl;
                std::cerr << "Use 'eagle-cli loadbalance instances <service-name> weight|health'" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown loadbalance command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli loadbalance show|set|instances'" << std::endl;
            return 1;
        }
    }
    
    int handleConfigVersion(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli config-version <list|show|create|rollback|compare> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            std::cerr << "Error: ConfigManager not available" << std::endl;
            return 1;
        }
        
        Eagle::Core::ConfigVersionManager* versionManager = configManager->versionManager();
        if (!versionManager || !versionManager->isEnabled()) {
            std::cerr << "Error: Config version management is not enabled" << std::endl;
            return 1;
        }
        
        if (subCommand == "list") {
            int limit = 0;
            if (args.size() > 1) {
                limit = args[1].toInt();
            }
            
            QList<Eagle::Core::ConfigVersion> versions = configManager->getConfigVersions(limit);
            
            std::cout << "Config Versions:" << std::endl;
            std::cout << "Current Version: " << versionManager->currentVersion() << std::endl;
            std::cout << "Total Versions: " << versions.size() << std::endl;
            std::cout << std::endl;
            
            for (const Eagle::Core::ConfigVersion& version : versions) {
                std::cout << "Version " << version.version << ":" << std::endl;
                std::cout << "  Timestamp: " << version.timestamp.toString(Qt::ISODate).toStdString() << std::endl;
                std::cout << "  Author: " << version.author.toStdString() << std::endl;
                std::cout << "  Description: " << version.description.toStdString() << std::endl;
                std::cout << "  Hash: " << version.configHash.toStdString() << std::endl;
                std::cout << std::endl;
            }
            return 0;
        } else if (subCommand == "show") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli config-version show <version>" << std::endl;
                return 1;
            }
            
            bool ok;
            int version = args[1].toInt(&ok);
            if (!ok) {
                std::cerr << "Error: Invalid version number" << std::endl;
                return 1;
            }
            
            Eagle::Core::ConfigVersion versionObj = configManager->getConfigVersion(version);
            if (!versionObj.isValid()) {
                std::cerr << "Error: Version " << version << " not found" << std::endl;
                return 1;
            }
            
            std::cout << "Version " << versionObj.version << ":" << std::endl;
            std::cout << "  Timestamp: " << versionObj.timestamp.toString(Qt::ISODate).toStdString() << std::endl;
            std::cout << "  Author: " << versionObj.author.toStdString() << std::endl;
            std::cout << "  Description: " << versionObj.description.toStdString() << std::endl;
            std::cout << "  Hash: " << versionObj.configHash.toStdString() << std::endl;
            std::cout << "  Config Keys: " << versionObj.config.keys().size() << std::endl;
            return 0;
        } else if (subCommand == "create") {
            QString author = "system";
            QString description;
            
            for (int i = 1; i < args.size(); ++i) {
                if (args[i] == "--author" && i + 1 < args.size()) {
                    author = args[++i];
                } else if (args[i] == "--description" && i + 1 < args.size()) {
                    description = args[++i];
                }
            }
            
            int newVersion = configManager->createConfigVersion(author, description);
            if (newVersion <= 0) {
                std::cerr << "Error: Failed to create version" << std::endl;
                return 1;
            }
            
            std::cout << "Created config version: " << newVersion << std::endl;
            return 0;
        } else if (subCommand == "rollback") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli config-version rollback <version>" << std::endl;
                return 1;
            }
            
            bool ok;
            int version = args[1].toInt(&ok);
            if (!ok) {
                std::cerr << "Error: Invalid version number" << std::endl;
                return 1;
            }
            
            if (!configManager->rollbackConfig(version)) {
                std::cerr << "Error: Failed to rollback to version " << version << std::endl;
                return 1;
            }
            
            std::cout << "Config rolled back to version: " << version << std::endl;
            return 0;
        } else if (subCommand == "compare") {
            if (args.size() < 3) {
                std::cerr << "Usage: eagle-cli config-version compare <version1> <version2>" << std::endl;
                return 1;
            }
            
            bool ok1, ok2;
            int version1 = args[1].toInt(&ok1);
            int version2 = args[2].toInt(&ok2);
            
            if (!ok1 || !ok2) {
                std::cerr << "Error: Invalid version numbers" << std::endl;
                return 1;
            }
            
            QList<Eagle::Core::ConfigDiff> diffs = configManager->compareConfigVersions(version1, version2);
            
            std::cout << "Comparing version " << version1 << " with version " << version2 << ":" << std::endl;
            std::cout << "Total differences: " << diffs.size() << std::endl;
            std::cout << std::endl;
            
            for (const Eagle::Core::ConfigDiff& diff : diffs) {
                std::cout << "Key: " << diff.key.toStdString() << std::endl;
                std::cout << "  Change Type: " << diff.changeType.toStdString() << std::endl;
                if (diff.oldValue.isValid()) {
                    std::cout << "  Old Value: " << diff.oldValue.toString().toStdString() << std::endl;
                }
                if (diff.newValue.isValid()) {
                    std::cout << "  New Value: " << diff.newValue.toString().toStdString() << std::endl;
                }
                std::cout << std::endl;
            }
            return 0;
        } else {
            std::cerr << "Unknown config-version command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli config-version list|show|create|rollback|compare'" << std::endl;
            return 1;
        }
    }
    
    int handleAsync(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli async <call|wait|batch> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ServiceRegistry* serviceRegistry = framework->serviceRegistry();
        if (!serviceRegistry) {
            std::cerr << "Error: ServiceRegistry not available" << std::endl;
            return 1;
        }
        
        Eagle::Core::AsyncServiceCall* asyncCall = serviceRegistry->asyncServiceCall();
        if (!asyncCall) {
            std::cerr << "Error: AsyncServiceCall not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "call") {
            if (args.size() < 3) {
                std::cerr << "Usage: eagle-cli async call <service-name> <method> [args...] [--timeout <ms>]" << std::endl;
                return 1;
            }
            
            QString serviceName = args[1];
            QString method = args[2];
            QVariantList callArgs;
            int timeout = 5000;
            
            for (int i = 3; i < args.size(); ++i) {
                if (args[i] == "--timeout" && i + 1 < args.size()) {
                    timeout = args[++i].toInt();
                } else {
                    callArgs.append(args[i]);
                }
            }
            
            std::cout << "Calling service asynchronously: " << serviceName.toStdString() 
                      << "::" << method.toStdString() << std::endl;
            
            Eagle::Core::ServiceFuture* future = asyncCall->callAsync(serviceName, method, callArgs, timeout);
            
            // 等待结果
            Eagle::Core::ServiceCallResult result = future->wait();
            
            if (result.success) {
                std::cout << "Result: " << result.result.toString().toStdString() << std::endl;
                std::cout << "Elapsed: " << result.elapsedMs << "ms" << std::endl;
                return 0;
            } else {
                std::cerr << "Error: " << result.error.toStdString() << std::endl;
                std::cerr << "Elapsed: " << result.elapsedMs << "ms" << std::endl;
                return 1;
            }
        } else if (subCommand == "wait") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli async wait <future-id> [--timeout <ms>]" << std::endl;
                return 1;
            }
            
            QString futureIdStr = args[1];
            bool ok;
            quintptr futurePtr = futureIdStr.toULongLong(&ok, 16);
            
            if (!ok) {
                std::cerr << "Error: Invalid future ID" << std::endl;
                return 1;
            }
            
            int timeout = -1;
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--timeout" && i + 1 < args.size()) {
                    timeout = args[++i].toInt();
                }
            }
            
            Eagle::Core::ServiceFuture* future = reinterpret_cast<Eagle::Core::ServiceFuture*>(futurePtr);
            if (!future) {
                std::cerr << "Error: Future not found" << std::endl;
                return 1;
            }
            
            Eagle::Core::ServiceCallResult result = future->wait(timeout);
            
            if (result.success) {
                std::cout << "Result: " << result.result.toString().toStdString() << std::endl;
                std::cout << "Elapsed: " << result.elapsedMs << "ms" << std::endl;
                return 0;
            } else {
                std::cerr << "Error: " << result.error.toStdString() << std::endl;
                std::cerr << "Elapsed: " << result.elapsedMs << "ms" << std::endl;
                return 1;
            }
        } else if (subCommand == "batch") {
            std::cerr << "Batch async calls not yet implemented in CLI" << std::endl;
            return 1;
        } else {
            std::cerr << "Unknown async command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli async call|wait|batch'" << std::endl;
            return 1;
        }
    }
    
    int handleConfigFormat(const QStringList& args) {
        if (args.isEmpty()) {
            std::cerr << "Usage: eagle-cli config format <load|save|convert> [options]" << std::endl;
            return 1;
        }
        
        QString subCommand = args.first();
        
        // 初始化框架
        Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
        if (!framework->initialize()) {
            std::cerr << "Error: Failed to initialize framework" << std::endl;
            return 1;
        }
        
        Eagle::Core::ConfigManager* configManager = framework->configManager();
        if (!configManager) {
            std::cerr << "Error: ConfigManager not available" << std::endl;
            return 1;
        }
        
        if (subCommand == "load") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli config format load <file-path> [--format json|yaml|ini|auto]" << std::endl;
                return 1;
            }
            
            QString filePath = args[1];
            QString formatStr = "auto";
            
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--format" && i + 1 < args.size()) {
                    formatStr = args[++i].toLower();
                }
            }
            
            Eagle::Core::ConfigFormat format = Eagle::Core::ConfigFormat::JSON;
            if (formatStr == "yaml" || formatStr == "yml") {
                format = Eagle::Core::ConfigFormat::YAML;
            } else if (formatStr == "ini" || formatStr == "conf" || formatStr == "cfg") {
                format = Eagle::Core::ConfigFormat::INI;
            } else if (formatStr == "auto") {
                format = Eagle::Core::ConfigFormatParser::formatFromExtension(filePath);
            }
            
            bool success = configManager->loadFromFile(filePath, Eagle::Core::ConfigManager::Global, format);
            
            if (success) {
                std::cout << "Config loaded successfully from: " << filePath.toStdString() << std::endl;
                std::cout << "Format: " << formatStr.toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to load config from: " << filePath.toStdString() << std::endl;
                return 1;
            }
        } else if (subCommand == "save") {
            if (args.size() < 2) {
                std::cerr << "Usage: eagle-cli config format save <file-path> [--format json|yaml|ini|auto]" << std::endl;
                return 1;
            }
            
            QString filePath = args[1];
            QString formatStr = "auto";
            
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--format" && i + 1 < args.size()) {
                    formatStr = args[++i].toLower();
                }
            }
            
            Eagle::Core::ConfigFormat format = Eagle::Core::ConfigFormat::JSON;
            if (formatStr == "yaml" || formatStr == "yml") {
                format = Eagle::Core::ConfigFormat::YAML;
            } else if (formatStr == "ini" || formatStr == "conf" || formatStr == "cfg") {
                format = Eagle::Core::ConfigFormat::INI;
            } else if (formatStr == "auto") {
                format = Eagle::Core::ConfigFormatParser::formatFromExtension(filePath);
            }
            
            bool success = configManager->saveToFile(filePath, Eagle::Core::ConfigManager::Global, format);
            
            if (success) {
                std::cout << "Config saved successfully to: " << filePath.toStdString() << std::endl;
                std::cout << "Format: " << formatStr.toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to save config to: " << filePath.toStdString() << std::endl;
                return 1;
            }
        } else if (subCommand == "convert") {
            if (args.size() < 4) {
                std::cerr << "Usage: eagle-cli config format convert <input-file> <output-file> [--source-format json|yaml|ini] [--target-format json|yaml|ini]" << std::endl;
                return 1;
            }
            
            QString inputFile = args[1];
            QString outputFile = args[2];
            QString sourceFormatStr = "auto";
            QString targetFormatStr = "auto";
            
            for (int i = 3; i < args.size(); ++i) {
                if (args[i] == "--source-format" && i + 1 < args.size()) {
                    sourceFormatStr = args[++i].toLower();
                } else if (args[i] == "--target-format" && i + 1 < args.size()) {
                    targetFormatStr = args[++i].toLower();
                }
            }
            
            Eagle::Core::ConfigFormat sourceFormat = Eagle::Core::ConfigFormatParser::formatFromExtension(inputFile);
            if (sourceFormatStr != "auto") {
                if (sourceFormatStr == "yaml" || sourceFormatStr == "yml") {
                    sourceFormat = Eagle::Core::ConfigFormat::YAML;
                } else if (sourceFormatStr == "ini" || sourceFormatStr == "conf" || sourceFormatStr == "cfg") {
                    sourceFormat = Eagle::Core::ConfigFormat::INI;
                } else if (sourceFormatStr == "json") {
                    sourceFormat = Eagle::Core::ConfigFormat::JSON;
                }
            }
            
            Eagle::Core::ConfigFormat targetFormat = Eagle::Core::ConfigFormatParser::formatFromExtension(outputFile);
            if (targetFormatStr != "auto") {
                if (targetFormatStr == "yaml" || targetFormatStr == "yml") {
                    targetFormat = Eagle::Core::ConfigFormat::YAML;
                } else if (targetFormatStr == "ini" || targetFormatStr == "conf" || targetFormatStr == "cfg") {
                    targetFormat = Eagle::Core::ConfigFormat::INI;
                } else if (targetFormatStr == "json") {
                    targetFormat = Eagle::Core::ConfigFormat::JSON;
                }
            }
            
            QVariantMap config = Eagle::Core::ConfigFormatParser::loadFromFile(inputFile, sourceFormat);
            if (config.isEmpty()) {
                std::cerr << "Error: Failed to load config from: " << inputFile.toStdString() << std::endl;
                return 1;
            }
            
            bool success = Eagle::Core::ConfigFormatParser::saveToFile(config, outputFile, targetFormat);
            
            if (success) {
                std::cout << "Config converted successfully:" << std::endl;
                std::cout << "  From: " << inputFile.toStdString() << " (" << sourceFormatStr.toStdString() << ")" << std::endl;
                std::cout << "  To: " << outputFile.toStdString() << " (" << targetFormatStr.toStdString() << ")" << std::endl;
                return 0;
            } else {
                std::cerr << "Error: Failed to save config to: " << outputFile.toStdString() << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown config format command: " << subCommand.toStdString() << std::endl;
            std::cerr << "Use 'eagle-cli config format load|save|convert'" << std::endl;
            return 1;
        }
    }
};

int handlePlugins(const QStringList& args) {
    if (args.isEmpty() || args.first() != "list") {
        std::cerr << "Usage: eagle-cli plugins list [--category=<category>]" << std::endl;
        std::cerr << "Categories: ui, service, tool" << std::endl;
        return 1;
    }
    
    // 初始化框架
    Eagle::Core::Framework* framework = Eagle::Core::Framework::instance();
    if (!framework->initialize()) {
        std::cerr << "Error: Failed to initialize framework" << std::endl;
        return 1;
    }
    
    Eagle::Core::PluginManager* pluginManager = framework->pluginManager();
    if (!pluginManager) {
        std::cerr << "Error: PluginManager not available" << std::endl;
        return 1;
    }
    
    // 解析分类参数
    QString categoryFilter;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i].startsWith("--category=")) {
            categoryFilter = args[i].mid(11);
        } else if (args[i] == "--category" && i + 1 < args.size()) {
            categoryFilter = args[++i];
        }
    }
    
    QStringList pluginIds;
    if (!categoryFilter.isEmpty()) {
        // 按分类筛选
        Eagle::Core::PluginCategory category = Eagle::Core::PluginMetadata::categoryFromString(categoryFilter);
        pluginIds = pluginManager->pluginsByCategory(category);
    } else {
        // 获取所有插件
        pluginIds = pluginManager->availablePlugins();
    }
    
    // 显示插件列表
    std::cout << "=== Plugin List ===" << std::endl;
    if (!categoryFilter.isEmpty()) {
        std::cout << "Category: " << categoryFilter.toStdString() << std::endl;
    }
    std::cout << "Total: " << pluginIds.size() << std::endl;
    std::cout << std::endl;
    
    for (const QString& pluginId : pluginIds) {
        Eagle::Core::PluginMetadata metadata = pluginManager->getPluginMetadata(pluginId);
        std::cout << "ID: " << pluginId.toStdString() << std::endl;
        std::cout << "  Name: " << metadata.name.toStdString() << std::endl;
        std::cout << "  Version: " << metadata.version.toStdString() << std::endl;
        std::cout << "  Category: " << Eagle::Core::PluginMetadata::categoryToString(metadata.category).toStdString() << std::endl;
        std::cout << "  Author: " << metadata.author.toStdString() << std::endl;
        std::cout << "  Description: " << metadata.description.toStdString() << std::endl;
        std::cout << "  Loaded: " << (pluginManager->isPluginLoaded(pluginId) ? "Yes" : "No") << std::endl;
        std::cout << std::endl;
    }
    
    // 显示分类统计
    QMap<Eagle::Core::PluginCategory, int> stats = pluginManager->categoryStatistics();
    std::cout << "=== Category Statistics ===" << std::endl;
    std::cout << "UI: " << stats[Eagle::Core::PluginCategory::UI] << std::endl;
    std::cout << "Service: " << stats[Eagle::Core::PluginCategory::Service] << std::endl;
    std::cout << "Tool: " << stats[Eagle::Core::PluginCategory::Tool] << std::endl;
    
    return 0;
}

int handleAudit(const QStringList& args) {
    if (args.isEmpty()) {
        std::cerr << "Usage: eagle-cli audit <command> [options]" << std::endl;
        std::cerr << "Commands:" << std::endl;
        std::cerr << "  verify [--file=<path>]  - Verify log integrity" << std::endl;
        std::cerr << "  report [--file=<path>]  - Get integrity report" << std::endl;
        std::cerr << "  config <enable|disable> - Configure tamper protection" << std::endl;
        return 1;
    }
    
    QString subCommand = args.first();
    
    if (!Framework::instance()) {
        Framework::instance()->initialize();
    }
    
    Framework* framework = Framework::instance();
    if (!framework || !framework->auditLogManager()) {
        std::cerr << "Error: Framework or AuditLogManager not initialized" << std::endl;
        return 1;
    }
    
    AuditLogManager* auditLog = framework->auditLogManager();
    
    if (subCommand == "verify") {
        QString logFilePath;
        for (int i = 1; i < args.size(); ++i) {
            if (args[i].startsWith("--file=")) {
                logFilePath = args[i].mid(7);
            } else if (args[i] == "--file" && i + 1 < args.size()) {
                logFilePath = args[++i];
            }
        }
        
        bool isValid = auditLog->verifyLogIntegrity(logFilePath);
        
        std::cout << "Log Integrity Verification:" << std::endl;
        std::cout << "  Status: " << (isValid ? "VALID" : "INVALID") << std::endl;
        std::cout << "  Tamper Protection: " << (auditLog->isTamperProtectionEnabled() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "  Last Entry Hash: " << auditLog->getLastEntryHash().toStdString() << std::endl;
        
        if (!isValid) {
            std::cerr << "Warning: Log integrity verification failed!" << std::endl;
            return 1;
        }
        
        return 0;
    } else if (subCommand == "report") {
        QString logFilePath;
        for (int i = 1; i < args.size(); ++i) {
            if (args[i].startsWith("--file=")) {
                logFilePath = args[i].mid(7);
            } else if (args[i] == "--file" && i + 1 < args.size()) {
                logFilePath = args[++i];
            }
        }
        
        QVariantMap report = auditLog->getIntegrityReport(logFilePath);
        
        std::cout << "=== Audit Log Integrity Report ===" << std::endl;
        std::cout << "Log File: " << report.value("logFilePath").toString().toStdString() << std::endl;
        std::cout << "Tamper Protection: " << (report.value("tamperProtectionEnabled").toBool() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "File Exists: " << (report.value("fileExists").toBool() ? "Yes" : "No") << std::endl;
        std::cout << "Valid: " << (report.value("isValid").toBool() ? "Yes" : "No") << std::endl;
        std::cout << "Total Entries: " << report.value("totalEntries").toInt() << std::endl;
        std::cout << "Valid Entries: " << report.value("validEntries").toInt() << std::endl;
        std::cout << "Invalid Entries: " << report.value("invalidEntries").toInt() << std::endl;
        std::cout << "First Entry Hash: " << report.value("firstEntryHash").toString().toStdString() << std::endl;
        std::cout << "Last Entry Hash: " << report.value("lastEntryHash").toString().toStdString() << std::endl;
        
        QStringList errors = report.value("errors").toStringList();
        if (!errors.isEmpty()) {
            std::cout << std::endl << "Errors:" << std::endl;
            for (const QString& error : errors) {
                std::cout << "  - " << error.toStdString() << std::endl;
            }
        }
        
        std::cout << "Verification Time: " << report.value("verificationTime").toString().toStdString() << std::endl;
        
        return report.value("isValid").toBool() ? 0 : 1;
    } else if (subCommand == "config") {
        if (args.size() < 2) {
            std::cerr << "Usage: eagle-cli audit config <enable|disable>" << std::endl;
            return 1;
        }
        
        QString action = args[1].toLower();
        bool enabled = (action == "enable" || action == "1" || action == "true");
        
        auditLog->setTamperProtectionEnabled(enabled);
        
        std::cout << "Tamper protection " << (enabled ? "enabled" : "disabled") << std::endl;
        return 0;
    } else {
        std::cerr << "Unknown command: " << subCommand.toStdString() << std::endl;
        return 1;
    }
}

int main(int argc, char *argv[])
{
    EagleCLI cli;
    return cli.run(argc, argv);
}
