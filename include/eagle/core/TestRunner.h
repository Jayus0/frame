#ifndef EAGLE_CORE_TESTRUNNER_H
#define EAGLE_CORE_TESTRUNNER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QMap>
#include "TestCaseBase.h"

namespace Eagle {
namespace Core {

class TestCaseBase;

/**
 * @brief 测试报告格式
 */
enum class TestReportFormat {
    Console,    // 控制台输出
    JSON,       // JSON格式
    HTML        // HTML格式
};

/**
 * @brief 测试运行器
 * 
 * 负责发现、运行测试用例并生成报告
 */
class TestRunner : public QObject {
    Q_OBJECT
    
public:
    explicit TestRunner(QObject* parent = nullptr);
    ~TestRunner();
    
    // 测试发现
    void addTestClass(TestCaseBase* testCase);
    void addTestClass(const QString& className);
    void discoverTests(const QString& testDir);
    
    // 测试运行
    bool runTests(const QStringList& testNames = QStringList());
    bool runTest(const QString& testName);
    bool runTestClass(const QString& className);
    
    // 配置
    void setReportFormat(TestReportFormat format);
    TestReportFormat reportFormat() const;
    void setOutputFile(const QString& filePath);
    QString outputFile() const;
    void setVerbose(bool verbose);
    bool isVerbose() const;
    
    // 测试结果
    QList<TestCaseInfo> testResults() const;
    QMap<QString, TestSuiteInfo> suiteResults() const;
    int totalCount() const;
    int passCount() const;
    int failCount() const;
    int skipCount() const;
    int errorCount() const;
    
    // 报告生成
    QString generateReport(TestReportFormat format = TestReportFormat::Console) const;
    bool saveReport(const QString& filePath, TestReportFormat format = TestReportFormat::JSON) const;
    
signals:
    void testStarted(const QString& testName);
    void testFinished(const QString& testName, TestResult result);
    void allTestsFinished();
    
private:
    Q_DISABLE_COPY(TestRunner)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    void registerTestCase(TestCaseBase* testCase);
    QString generateConsoleReport() const;
    QString generateJsonReport() const;
    QString generateHtmlReport() const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::TestReportFormat)

#endif // EAGLE_CORE_TESTRUNNER_H
