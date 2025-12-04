#ifndef TESTRUNNER_P_H
#define TESTRUNNER_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include "eagle/core/TestRunner.h"
#include "eagle/core/TestCaseBase.h"

namespace Eagle {
namespace Core {

class TestRunner::Private {
public:
    QMap<QString, TestCaseBase*> testCases;  // testName -> TestCase
    QMap<QString, QStringList> testSuites;    // className -> testNames
    QList<TestCaseInfo> testResults;
    QMap<QString, TestSuiteInfo> suiteResults;
    TestReportFormat reportFormat;
    QString outputFile;
    bool verbose;
    mutable QMutex mutex;
    
    Private()
        : reportFormat(TestReportFormat::Console)
        , verbose(false)
    {
    }
};

} // namespace Core
} // namespace Eagle

#endif // TESTRUNNER_P_H
