#include "eagle/core/TestCaseBase.h"
#include "eagle/core/Logger.h"
#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTemporaryDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <cstdlib>
#include <ctime>

namespace Eagle {
namespace Core {

TestCaseBase::TestCaseBase(QObject* parent)
    : QObject(parent)
    , m_result(TestResult::Pass)
    , m_hasFailed(false)
{
    m_startTime = QDateTime::currentDateTime();
}

TestCaseBase::~TestCaseBase()
{
}

QString TestCaseBase::testName() const
{
    return m_testName.isEmpty() ? metaObject()->className() : m_testName;
}

QString TestCaseBase::testDescription() const
{
    return m_testDescription;
}

void TestCaseBase::setTestName(const QString& name)
{
    m_testName = name;
}

void TestCaseBase::setTestDescription(const QString& description)
{
    m_testDescription = description;
}

void TestCaseBase::setUp()
{
    // 默认实现为空，子类可以重写
}

void TestCaseBase::tearDown()
{
    // 默认实现为空，子类可以重写
}

void TestCaseBase::assertTrue(bool condition, const QString& message)
{
    if (!condition) {
        fail(message.isEmpty() ? "Assertion failed: expected true" : message);
    }
}

void TestCaseBase::assertFalse(bool condition, const QString& message)
{
    if (condition) {
        fail(message.isEmpty() ? "Assertion failed: expected false" : message);
    }
}

void TestCaseBase::assertEqual(const QVariant& expected, const QVariant& actual, const QString& message)
{
    if (expected != actual) {
        QString errorMsg = message.isEmpty() 
            ? QString("Assertion failed: expected %1, got %2")
                .arg(expected.toString()).arg(actual.toString())
            : message;
        fail(errorMsg);
    }
}

void TestCaseBase::assertNotEqual(const QVariant& expected, const QVariant& actual, const QString& message)
{
    if (expected == actual) {
        QString errorMsg = message.isEmpty()
            ? QString("Assertion failed: expected not equal, but both are %1")
                .arg(expected.toString())
            : message;
        fail(errorMsg);
    }
}

void TestCaseBase::assertNull(const QVariant& value, const QString& message)
{
    if (!value.isNull()) {
        fail(message.isEmpty() ? "Assertion failed: expected null" : message);
    }
}

void TestCaseBase::assertNotNull(const QVariant& value, const QString& message)
{
    if (value.isNull()) {
        fail(message.isEmpty() ? "Assertion failed: expected not null" : message);
    }
}

void TestCaseBase::assertContains(const QString& container, const QString& substring, const QString& message)
{
    if (!container.contains(substring)) {
        QString errorMsg = message.isEmpty()
            ? QString("Assertion failed: '%1' does not contain '%2'")
                .arg(container).arg(substring)
            : message;
        fail(errorMsg);
    }
}

void TestCaseBase::assertThrows(std::function<void()> func, const QString& message)
{
    bool threwException = false;
    try {
        func();
    } catch (...) {
        threwException = true;
    }
    
    if (!threwException) {
        fail(message.isEmpty() ? "Assertion failed: expected exception was not thrown" : message);
    }
}

void TestCaseBase::skipTest(const QString& reason)
{
    m_result = TestResult::Skip;
    m_errorMessage = reason;
    m_endTime = QDateTime::currentDateTime();
    emit testFinished(testName(), TestResult::Skip);
}

void TestCaseBase::fail(const QString& message)
{
    if (!m_hasFailed) {
        m_hasFailed = true;
        m_result = TestResult::Fail;
        m_errorMessage = message;
        m_endTime = QDateTime::currentDateTime();
        emit testFailed(testName(), message);
        emit testFinished(testName(), TestResult::Fail);
    }
}

void TestCaseBase::pass()
{
    if (!m_hasFailed && m_result != TestResult::Skip) {
        m_result = TestResult::Pass;
        m_endTime = QDateTime::currentDateTime();
        emit testFinished(testName(), TestResult::Pass);
    }
}

TestResult TestCaseBase::result() const
{
    return m_result;
}

QString TestCaseBase::errorMessage() const
{
    return m_errorMessage;
}

qint64 TestCaseBase::durationMs() const
{
    if (m_startTime.isValid() && m_endTime.isValid()) {
        return m_startTime.msecsTo(m_endTime);
    }
    return 0;
}

// TestUtils 实现
QString TestUtils::generateRandomString(int length)
{
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    QString result;
    result.reserve(length);
    
    std::srand(std::time(nullptr));
    for (int i = 0; i < length; ++i) {
        result.append(alphanum[std::rand() % (sizeof(alphanum) - 1)]);
    }
    
    return result;
}

int TestUtils::generateRandomInt(int min, int max)
{
    std::srand(std::time(nullptr));
    return min + (std::rand() % (max - min + 1));
}

QString TestUtils::createTempFile(const QString& content)
{
    QTemporaryFile tempFile;
    if (tempFile.open()) {
        if (!content.isEmpty()) {
            tempFile.write(content.toUtf8());
        }
        QString path = tempFile.fileName();
        tempFile.setAutoRemove(false);
        return path;
    }
    return QString();
}

QString TestUtils::createTempDir()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString dirName = QString("eagle_test_%1").arg(generateRandomString(8));
    QString dirPath = QDir(baseDir).filePath(dirName);
    
    QDir dir;
    if (dir.mkpath(dirPath)) {
        return dirPath;
    }
    return QString();
}

bool TestUtils::removeTempFile(const QString& path)
{
    return QFile::remove(path);
}

bool TestUtils::removeTempDir(const QString& path)
{
    QDir dir(path);
    return dir.removeRecursively();
}

void TestUtils::wait(int ms)
{
    QThread::msleep(ms);
}

bool TestUtils::waitForCondition(std::function<bool()> condition, int timeoutMs, int intervalMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        if (condition()) {
            return true;
        }
        QThread::msleep(intervalMs);
    }
    
    return false;
}

} // namespace Core
} // namespace Eagle
