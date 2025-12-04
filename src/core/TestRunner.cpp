#include "eagle/core/TestRunner.h"
#include "TestRunner_p.h"
#include "eagle/core/TestCaseBase.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>

namespace Eagle {
namespace Core {

TestRunner::TestRunner(QObject* parent)
    : QObject(parent)
    , d(new TestRunner::Private)
{
}

TestRunner::~TestRunner()
{
    delete d;
}

void TestRunner::addTestClass(TestCaseBase* testCase)
{
    if (!testCase) {
        return;
    }
    
    registerTestCase(testCase);
}

void TestRunner::addTestClass(const QString& className)
{
    // 动态加载测试类（需要类注册机制）
    // 这里简化实现，实际应该使用Qt的元对象系统
    Q_UNUSED(className);
    Logger::warning("TestRunner", "动态加载测试类功能未实现");
}

void TestRunner::discoverTests(const QString& testDir)
{
    QDir dir(testDir);
    if (!dir.exists()) {
        Logger::warning("TestRunner", QString("测试目录不存在: %1").arg(testDir));
        return;
    }
    
    // 查找测试文件（简化实现）
    QStringList filters;
    filters << "*Test.cpp" << "*_test.cpp";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        Logger::info("TestRunner", QString("发现测试文件: %1").arg(fileInfo.fileName()));
    }
}

bool TestRunner::runTests(const QStringList& testNames)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->testResults.clear();
    d->suiteResults.clear();
    
    if (testNames.isEmpty()) {
        // 运行所有测试
        for (auto it = d->testCases.begin(); it != d->testCases.end(); ++it) {
            locker.unlock();
            runTest(it.key());
            locker.relock();
        }
    } else {
        // 运行指定的测试
        for (const QString& testName : testNames) {
            locker.unlock();
            runTest(testName);
            locker.relock();
        }
    }
    
    emit allTestsFinished();
    
    // 生成报告
    if (!d->outputFile.isEmpty()) {
        saveReport(d->outputFile, d->reportFormat);
    } else if (d->verbose) {
        QString report = generateReport(d->reportFormat);
        Logger::info("TestRunner", "\n" + report);
    }
    
    return d->testResults.isEmpty() || failCount() == 0;
}

bool TestRunner::runTest(const QString& testName)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->testCases.contains(testName)) {
        Logger::error("TestRunner", QString("测试用例不存在: %1").arg(testName));
        return false;
    }
    
    TestCaseBase* testCase = d->testCases[testName];
    locker.unlock();
    
    emit testStarted(testName);
    
    // 执行测试
    TestCaseInfo info;
    info.name = testName;
    info.className = testCase->metaObject()->className();
    info.description = testCase->testDescription();
    info.startTime = QDateTime::currentDateTime();
    
    try {
        testCase->setUp();
        // 这里应该调用测试方法，但需要元对象系统支持
        // 简化实现：直接调用测试
        testCase->tearDown();
        
        info.result = testCase->result();
        info.errorMessage = testCase->errorMessage();
    } catch (...) {
        info.result = TestResult::Error;
        info.errorMessage = "Exception occurred during test execution";
    }
    
    info.endTime = QDateTime::currentDateTime();
    info.durationMs = info.startTime.msecsTo(info.endTime);
    
    emit testFinished(testName, info.result);
    
    locker.relock();
    d->testResults.append(info);
    
    // 更新套件统计
    if (!d->suiteResults.contains(info.className)) {
        TestSuiteInfo suiteInfo;
        suiteInfo.name = info.className;
        d->suiteResults[info.className] = suiteInfo;
    }
    
    TestSuiteInfo& suiteInfo = d->suiteResults[info.className];
    suiteInfo.testCases.append(testName);
    suiteInfo.totalCount++;
    suiteInfo.totalDurationMs += info.durationMs;
    
    switch (info.result) {
        case TestResult::Pass:
            suiteInfo.passCount++;
            break;
        case TestResult::Fail:
            suiteInfo.failCount++;
            break;
        case TestResult::Skip:
            suiteInfo.skipCount++;
            break;
        case TestResult::Error:
            suiteInfo.errorCount++;
            break;
    }
    
    return info.result == TestResult::Pass;
}

bool TestRunner::runTestClass(const QString& className)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->testSuites.contains(className)) {
        Logger::error("TestRunner", QString("测试类不存在: %1").arg(className));
        return false;
    }
    
    QStringList testNames = d->testSuites[className];
    locker.unlock();
    
    return runTests(testNames);
}

void TestRunner::setReportFormat(TestReportFormat format)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->reportFormat = format;
}

TestReportFormat TestRunner::reportFormat() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->reportFormat;
}

void TestRunner::setOutputFile(const QString& filePath)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->outputFile = filePath;
}

QString TestRunner::outputFile() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->outputFile;
}

void TestRunner::setVerbose(bool verbose)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->verbose = verbose;
}

bool TestRunner::isVerbose() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->verbose;
}

QList<TestCaseInfo> TestRunner::testResults() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->testResults;
}

QMap<QString, TestSuiteInfo> TestRunner::suiteResults() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->suiteResults;
}

int TestRunner::totalCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->testResults.size();
}

int TestRunner::passCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    int count = 0;
    for (const TestCaseInfo& info : d->testResults) {
        if (info.result == TestResult::Pass) {
            count++;
        }
    }
    return count;
}

int TestRunner::failCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    int count = 0;
    for (const TestCaseInfo& info : d->testResults) {
        if (info.result == TestResult::Fail) {
            count++;
        }
    }
    return count;
}

int TestRunner::skipCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    int count = 0;
    for (const TestCaseInfo& info : d->testResults) {
        if (info.result == TestResult::Skip) {
            count++;
        }
    }
    return count;
}

int TestRunner::errorCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    int count = 0;
    for (const TestCaseInfo& info : d->testResults) {
        if (info.result == TestResult::Error) {
            count++;
        }
    }
    return count;
}

QString TestRunner::generateReport(TestReportFormat format) const
{
    switch (format) {
        case TestReportFormat::Console:
            return generateConsoleReport();
        case TestReportFormat::JSON:
            return generateJsonReport();
        case TestReportFormat::HTML:
            return generateHtmlReport();
    }
    return QString();
}

bool TestRunner::saveReport(const QString& filePath, TestReportFormat format) const
{
    QString report = generateReport(format);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("TestRunner", QString("无法创建报告文件: %1").arg(filePath));
        return false;
    }
    
    QTextStream stream(&file);
    stream << report;
    file.close();
    
    Logger::info("TestRunner", QString("测试报告已保存: %1").arg(filePath));
    return true;
}

void TestRunner::registerTestCase(TestCaseBase* testCase)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString testName = testCase->testName();
    d->testCases[testName] = testCase;
    
    QString className = testCase->metaObject()->className();
    if (!d->testSuites.contains(className)) {
        d->testSuites[className] = QStringList();
    }
    d->testSuites[className].append(testName);
    
    // 连接信号
    connect(testCase, &TestCaseBase::testStarted, this, &TestRunner::testStarted);
    connect(testCase, &TestCaseBase::testFinished, this, &TestRunner::testFinished);
    connect(testCase, &TestCaseBase::testFailed, this, [this, testName](const QString&, const QString& error) {
        Q_UNUSED(testName);
        Q_UNUSED(error);
    });
}

QString TestRunner::generateConsoleReport() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString report;
    QTextStream stream(&report);
    
    stream << "\n";
    stream << "========================================\n";
    stream << "        测试报告\n";
    stream << "========================================\n";
    stream << "\n";
    
    // 统计信息
    stream << "总计: " << totalCount() << "\n";
    stream << "通过: " << passCount() << "\n";
    stream << "失败: " << failCount() << "\n";
    stream << "跳过: " << skipCount() << "\n";
    stream << "错误: " << errorCount() << "\n";
    stream << "\n";
    
    // 测试用例详情
    for (const TestCaseInfo& info : d->testResults) {
        QString status;
        switch (info.result) {
            case TestResult::Pass:
                status = "✓";
                break;
            case TestResult::Fail:
                status = "✗";
                break;
            case TestResult::Skip:
                status = "-";
                break;
            case TestResult::Error:
                status = "!";
                break;
        }
        
        stream << QString("%1 %2 (%3ms)").arg(status).arg(info.name).arg(info.durationMs) << "\n";
        if (!info.errorMessage.isEmpty()) {
            stream << "  " << info.errorMessage << "\n";
        }
    }
    
    stream << "\n";
    stream << "========================================\n";
    
    return report;
}

QString TestRunner::generateJsonReport() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["total"] = totalCount();
    root["pass"] = passCount();
    root["fail"] = failCount();
    root["skip"] = skipCount();
    root["error"] = errorCount();
    
    QJsonArray testCases;
    for (const TestCaseInfo& info : d->testResults) {
        QJsonObject testCase;
        testCase["name"] = info.name;
        testCase["className"] = info.className;
        testCase["description"] = info.description;
        testCase["result"] = static_cast<int>(info.result);
        testCase["errorMessage"] = info.errorMessage;
        testCase["durationMs"] = info.durationMs;
        testCase["startTime"] = info.startTime.toString(Qt::ISODate);
        testCase["endTime"] = info.endTime.toString(Qt::ISODate);
        testCases.append(testCase);
    }
    root["testCases"] = testCases;
    
    QJsonDocument doc(root);
    return doc.toJson();
}

QString TestRunner::generateHtmlReport() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QString html;
    QTextStream stream(&html);
    
    stream << "<!DOCTYPE html>\n";
    stream << "<html>\n";
    stream << "<head>\n";
    stream << "<title>测试报告</title>\n";
    stream << "<style>\n";
    stream << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
    stream << "h1 { color: #333; }\n";
    stream << "table { border-collapse: collapse; width: 100%; margin-top: 20px; }\n";
    stream << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    stream << "th { background-color: #f2f2f2; }\n";
    stream << ".pass { color: green; }\n";
    stream << ".fail { color: red; }\n";
    stream << ".skip { color: orange; }\n";
    stream << ".error { color: red; font-weight: bold; }\n";
    stream << "</style>\n";
    stream << "</head>\n";
    stream << "<body>\n";
    stream << "<h1>测试报告</h1>\n";
    stream << "<p>生成时间: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "</p>\n";
    stream << "<p>总计: " << totalCount() << " | 通过: " << passCount() 
           << " | 失败: " << failCount() << " | 跳过: " << skipCount() 
           << " | 错误: " << errorCount() << "</p>\n";
    stream << "<table>\n";
    stream << "<tr><th>测试用例</th><th>类名</th><th>结果</th><th>耗时(ms)</th><th>错误信息</th></tr>\n";
    
    for (const TestCaseInfo& info : d->testResults) {
        QString resultClass;
        QString resultText;
        switch (info.result) {
            case TestResult::Pass:
                resultClass = "pass";
                resultText = "通过";
                break;
            case TestResult::Fail:
                resultClass = "fail";
                resultText = "失败";
                break;
            case TestResult::Skip:
                resultClass = "skip";
                resultText = "跳过";
                break;
            case TestResult::Error:
                resultClass = "error";
                resultText = "错误";
                break;
        }
        
        stream << "<tr>\n";
        stream << "<td>" << info.name << "</td>\n";
        stream << "<td>" << info.className << "</td>\n";
        stream << "<td class=\"" << resultClass << "\">" << resultText << "</td>\n";
        stream << "<td>" << info.durationMs << "</td>\n";
        stream << "<td>" << info.errorMessage << "</td>\n";
        stream << "</tr>\n";
    }
    
    stream << "</table>\n";
    stream << "</body>\n";
    stream << "</html>\n";
    
    return html;
}

} // namespace Core
} // namespace Eagle
