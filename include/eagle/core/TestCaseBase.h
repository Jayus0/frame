#ifndef EAGLE_CORE_TESTCASEBASE_H
#define EAGLE_CORE_TESTCASEBASE_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <functional>

namespace Eagle {
namespace Core {

/**
 * @brief 测试结果
 */
enum class TestResult {
    Pass,       // 通过
    Fail,       // 失败
    Skip,       // 跳过
    Error       // 错误
};

/**
 * @brief 测试用例信息
 */
struct TestCaseInfo {
    QString name;               // 测试用例名称
    QString className;          // 测试类名
    QString description;         // 描述
    TestResult result;           // 结果
    QString errorMessage;        // 错误信息
    qint64 durationMs;           // 执行时间（毫秒）
    QDateTime startTime;         // 开始时间
    QDateTime endTime;           // 结束时间
    
    TestCaseInfo()
        : result(TestResult::Pass)
        , durationMs(0)
    {
        startTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 测试套件信息
 */
struct TestSuiteInfo {
    QString name;               // 套件名称
    QStringList testCases;       // 测试用例列表
    int totalCount;              // 总数
    int passCount;               // 通过数
    int failCount;               // 失败数
    int skipCount;               // 跳过数
    int errorCount;              // 错误数
    qint64 totalDurationMs;      // 总执行时间
    
    TestSuiteInfo()
        : totalCount(0)
        , passCount(0)
        , failCount(0)
        , skipCount(0)
        , errorCount(0)
        , totalDurationMs(0)
    {
    }
};

/**
 * @brief 测试用例基类
 * 
 * 所有测试用例都应该继承此类
 */
class TestCaseBase : public QObject {
    Q_OBJECT
    
public:
    explicit TestCaseBase(QObject* parent = nullptr);
    virtual ~TestCaseBase();
    
    // 测试用例信息
    QString testName() const;
    QString testDescription() const;
    void setTestName(const QString& name);
    void setTestDescription(const QString& description);
    
    // 测试生命周期钩子
    virtual void setUp();        // 测试前准备
    virtual void tearDown();     // 测试后清理
    
    // 断言方法
    void assertTrue(bool condition, const QString& message = QString());
    void assertFalse(bool condition, const QString& message = QString());
    void assertEqual(const QVariant& expected, const QVariant& actual, const QString& message = QString());
    void assertNotEqual(const QVariant& expected, const QVariant& actual, const QString& message = QString());
    void assertNull(const QVariant& value, const QString& message = QString());
    void assertNotNull(const QVariant& value, const QString& message = QString());
    void assertContains(const QString& container, const QString& substring, const QString& message = QString());
    void assertThrows(std::function<void()> func, const QString& message = QString());
    
    // 跳过测试
    void skipTest(const QString& reason = QString());
    
    // 测试结果
    TestResult result() const;
    QString errorMessage() const;
    qint64 durationMs() const;
    
signals:
    void testStarted(const QString& testName);
    void testFinished(const QString& testName, TestResult result);
    void testFailed(const QString& testName, const QString& error);
    
protected:
    void fail(const QString& message);
    void pass();
    
private:
    QString m_testName;
    QString m_testDescription;
    TestResult m_result;
    QString m_errorMessage;
    QDateTime m_startTime;
    QDateTime m_endTime;
    bool m_hasFailed;
};

/**
 * @brief 测试工具类
 */
class TestUtils {
public:
    // 生成测试数据
    static QString generateRandomString(int length = 10);
    static int generateRandomInt(int min = 0, int max = 100);
    
    // 文件操作
    static QString createTempFile(const QString& content = QString());
    static QString createTempDir();
    static bool removeTempFile(const QString& path);
    static bool removeTempDir(const QString& path);
    
    // 等待工具
    static void wait(int ms);
    static bool waitForCondition(std::function<bool()> condition, int timeoutMs = 5000, int intervalMs = 100);
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::TestResult)
Q_DECLARE_METATYPE(Eagle::Core::TestCaseInfo)
Q_DECLARE_METATYPE(Eagle::Core::TestSuiteInfo)

#endif // EAGLE_CORE_TESTCASEBASE_H
