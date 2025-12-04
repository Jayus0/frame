#include "eagle/core/DiagnosticManager.h"
#include "DiagnosticManager_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#ifdef __linux__
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <cstdlib>
#include <cstring>
#include <QtCore/QRegExp>

namespace Eagle {
namespace Core {

DiagnosticManager::DiagnosticManager(QObject* parent)
    : QObject(parent)
    , d(new DiagnosticManager::Private)
{
    // 连接死锁检测定时器
    connect(d->deadlockDetectionTimer, &QTimer::timeout,
            this, &DiagnosticManager::onDeadlockDetectionTimer, Qt::QueuedConnection);
    
    Logger::info("DiagnosticManager", "诊断管理器初始化完成");
}

DiagnosticManager::~DiagnosticManager()
{
    delete d;
}

StackTrace DiagnosticManager::captureStackTrace(const QString& message)
{
    if (!isEnabled()) {
        return StackTrace();
    }
    
    StackTrace trace;
    trace.id = generateTraceId();
    trace.message = message;
    trace.threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    trace.threadName = QThread::currentThread()->objectName();
    if (trace.threadName.isEmpty()) {
        trace.threadName = QString("Thread-%1").arg(trace.threadId);
    }
    
    // 收集堆栈帧
    collectStackFrames(trace.frames);
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->stackTraces[trace.id] = trace;
    
    // 限制数量
    if (d->stackTraces.size() > d->maxStackTraces) {
        QString oldestId = d->stackTraces.keys().first();
        d->stackTraces.remove(oldestId);
    }
    
    emit stackTraceCaptured(trace.id);
    Logger::info("DiagnosticManager", QString("堆栈跟踪已捕获: %1").arg(trace.id));
    
    return trace;
}

QStringList DiagnosticManager::getStackTraceIds() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->stackTraces.keys();
}

StackTrace DiagnosticManager::getStackTrace(const QString& traceId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->stackTraces.value(traceId);
}

bool DiagnosticManager::saveStackTrace(const QString& traceId, const QString& filePath) const
{
    StackTrace trace = getStackTrace(traceId);
    if (trace.id.isEmpty()) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("DiagnosticManager", QString("无法创建堆栈跟踪文件: %1").arg(filePath));
        return false;
    }
    
    QTextStream stream(&file);
    stream << trace.toString();
    file.close();
    
    Logger::info("DiagnosticManager", QString("堆栈跟踪已保存: %1").arg(filePath));
    return true;
}

MemorySnapshot DiagnosticManager::captureMemorySnapshot(const QString& name)
{
    if (!isEnabled()) {
        return MemorySnapshot();
    }
    
    MemorySnapshot snapshot;
    snapshot.id = generateSnapshotId();
    
#ifdef __linux__
    // 获取系统内存信息（Linux）
    QFile memInfo("/proc/meminfo");
    if (memInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&memInfo);
        QString content = stream.readAll();
        memInfo.close();
        
        // 解析内存信息（简化实现）
        QStringList lines = content.split('\n');
        for (const QString& line : lines) {
            if (line.startsWith("MemTotal:")) {
                QString value = line.split(QRegExp("\\s+")).value(1);
                snapshot.totalMemory = value.toLongLong() * 1024;  // KB to bytes
            } else if (line.startsWith("MemAvailable:")) {
                QString value = line.split(QRegExp("\\s+")).value(1);
                snapshot.freeMemory = value.toLongLong() * 1024;
            }
        }
        snapshot.usedMemory = snapshot.totalMemory - snapshot.freeMemory;
    }
    
    // 获取进程内存信息
    QFile statusFile("/proc/self/status");
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&statusFile);
        QString content = stream.readAll();
        statusFile.close();
        
        QStringList lines = content.split('\n');
        for (const QString& line : lines) {
            if (line.startsWith("VmRSS:")) {
                QString value = line.split(QRegExp("\\s+")).value(1);
                snapshot.heapSize = value.toLongLong() * 1024;  // KB to bytes
            }
        }
    }
#else
    // 其他平台：使用Qt的内存信息
    snapshot.totalMemory = 0;
    snapshot.usedMemory = 0;
    snapshot.freeMemory = 0;
    snapshot.heapSize = 0;
#endif
    
    snapshot.details["name"] = name;
    snapshot.details["pid"] = getpid();
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->memorySnapshots[snapshot.id] = snapshot;
    
    // 限制数量
    if (d->memorySnapshots.size() > d->maxMemorySnapshots) {
        QString oldestId = d->memorySnapshots.keys().first();
        d->memorySnapshots.remove(oldestId);
    }
    
    emit memorySnapshotCaptured(snapshot.id);
    Logger::info("DiagnosticManager", QString("内存快照已捕获: %1 (使用率: %2%)")
        .arg(snapshot.id).arg(snapshot.memoryUsagePercent()));
    
    return snapshot;
}

QStringList DiagnosticManager::getMemorySnapshotIds() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->memorySnapshots.keys();
}

MemorySnapshot DiagnosticManager::getMemorySnapshot(const QString& snapshotId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->memorySnapshots.value(snapshotId);
}

QVariantMap DiagnosticManager::compareSnapshots(const QString& snapshotId1, const QString& snapshotId2) const
{
    MemorySnapshot snap1 = getMemorySnapshot(snapshotId1);
    MemorySnapshot snap2 = getMemorySnapshot(snapshotId2);
    
    if (snap1.id.isEmpty() || snap2.id.isEmpty()) {
        return QVariantMap();
    }
    
    QVariantMap comparison;
    comparison["snapshot1"] = snapshotId1;
    comparison["snapshot2"] = snapshotId2;
    comparison["timeDiff"] = snap1.timestamp.msecsTo(snap2.timestamp);
    
    // 内存差异
    qint64 memoryDiff = snap2.usedMemory - snap1.usedMemory;
    comparison["memoryDiff"] = memoryDiff;
    comparison["memoryDiffPercent"] = snap1.usedMemory > 0 ? (memoryDiff * 100) / snap1.usedMemory : 0;
    
    // 堆大小差异
    qint64 heapDiff = snap2.heapSize - snap1.heapSize;
    comparison["heapDiff"] = heapDiff;
    comparison["heapDiffPercent"] = snap1.heapSize > 0 ? (heapDiff * 100) / snap1.heapSize : 0;
    
    // 对象数量差异
    int objectDiff = snap2.objectCount - snap1.objectCount;
    comparison["objectDiff"] = objectDiff;
    
    // 判断是否有内存泄漏迹象
    bool possibleLeak = (memoryDiff > 0 && heapDiff > 0 && 
                         (heapDiff * 100) / snap1.heapSize > 10);  // 堆增长超过10%
    comparison["possibleLeak"] = possibleLeak;
    
    return comparison;
}

bool DiagnosticManager::saveMemorySnapshot(const QString& snapshotId, const QString& filePath) const
{
    MemorySnapshot snapshot = getMemorySnapshot(snapshotId);
    if (snapshot.id.isEmpty()) {
        return false;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("DiagnosticManager", QString("无法创建内存快照文件: %1").arg(filePath));
        return false;
    }
    
    QJsonObject obj;
    obj["id"] = snapshot.id;
    obj["timestamp"] = snapshot.timestamp.toString(Qt::ISODate);
    obj["totalMemory"] = snapshot.totalMemory;
    obj["usedMemory"] = snapshot.usedMemory;
    obj["freeMemory"] = snapshot.freeMemory;
    obj["heapSize"] = snapshot.heapSize;
    obj["objectCount"] = snapshot.objectCount;
    obj["memoryUsagePercent"] = snapshot.memoryUsagePercent();
    obj["details"] = QJsonObject::fromVariantMap(snapshot.details);
    
    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
    
    Logger::info("DiagnosticManager", QString("内存快照已保存: %1").arg(filePath));
    return true;
}

void DiagnosticManager::setDeadlockDetectionEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->deadlockDetectionEnabled = enabled;
    
    if (enabled) {
        startDeadlockDetection();
    } else {
        stopDeadlockDetection();
    }
    
    Logger::info("DiagnosticManager", QString("死锁检测%1").arg(enabled ? "启用" : "禁用"));
}

bool DiagnosticManager::isDeadlockDetectionEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->deadlockDetectionEnabled;
}

void DiagnosticManager::startDeadlockDetection()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->deadlockDetectionTimer->isActive()) {
        d->deadlockDetectionTimer->start();
        Logger::info("DiagnosticManager", "死锁检测已启动");
    }
}

void DiagnosticManager::stopDeadlockDetection()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (d->deadlockDetectionTimer->isActive()) {
        d->deadlockDetectionTimer->stop();
        Logger::info("DiagnosticManager", "死锁检测已停止");
    }
}

QList<DeadlockInfo> DiagnosticManager::getDeadlocks() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->deadlocks;
}

DeadlockInfo DiagnosticManager::getDeadlock(const QString& deadlockId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    for (const DeadlockInfo& info : d->deadlocks) {
        if (info.id == deadlockId) {
            return info;
        }
    }
    
    return DeadlockInfo();
}

bool DiagnosticManager::clearDeadlocks()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->deadlocks.clear();
    Logger::info("DiagnosticManager", "死锁记录已清除");
    return true;
}

void DiagnosticManager::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->enabled = enabled;
    Logger::info("DiagnosticManager", QString("诊断功能%1").arg(enabled ? "启用" : "禁用"));
}

bool DiagnosticManager::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->enabled;
}

void DiagnosticManager::setMaxStackTraces(int maxCount)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxStackTraces = maxCount;
}

int DiagnosticManager::maxStackTraces() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->maxStackTraces;
}

void DiagnosticManager::setMaxMemorySnapshots(int maxCount)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxMemorySnapshots = maxCount;
}

int DiagnosticManager::maxMemorySnapshots() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->maxMemorySnapshots;
}

void DiagnosticManager::onDeadlockDetectionTimer()
{
    if (!isDeadlockDetectionEnabled()) {
        return;
    }
    
    detectDeadlocks();
}

void DiagnosticManager::collectStackFrames(QList<StackFrame>& frames)
{
#ifdef __linux__
    // 使用backtrace获取堆栈信息（Linux）
    void* buffer[100];
    int size = backtrace(buffer, 100);
    
    if (size == 0) {
        return;
    }
    
    char** symbols = backtrace_symbols(buffer, size);
    if (!symbols) {
        return;
    }
    
    for (int i = 0; i < size; ++i) {
        StackFrame frame;
        frame.address = QString::number(reinterpret_cast<quintptr>(buffer[i]), 16);
        
        // 解析符号信息
        QString symbol = QString::fromLocal8Bit(symbols[i]);
        
        // 尝试解析函数名和地址
        // 格式通常是: /path/to/binary(function+offset) [address]
        int parenPos = symbol.indexOf('(');
        int plusPos = symbol.indexOf('+', parenPos);
        int bracketPos = symbol.indexOf('[', plusPos);
        
        if (parenPos > 0 && plusPos > parenPos) {
            frame.module = symbol.left(parenPos);
            frame.function = symbol.mid(parenPos + 1, plusPos - parenPos - 1);
            
            // 尝试demangle C++函数名
            QByteArray funcName = frame.function.toLocal8Bit();
            int status = 0;
            char* demangled = abi::__cxa_demangle(funcName.constData(), nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                frame.function = QString::fromLocal8Bit(demangled);
                free(demangled);
            }
        } else {
            frame.function = symbol;
        }
        
        frames.append(frame);
    }
    
    free(symbols);
#else
    // 其他平台：简化实现
    StackFrame frame;
    frame.function = "Stack trace not available on this platform";
    frames.append(frame);
#endif
}

void DiagnosticManager::detectDeadlocks()
{
    // 简化实现：检测QMutex死锁
    // 实际应该使用更复杂的死锁检测算法（如等待图）
    
    // 这里简化实现，实际应该：
    // 1. 跟踪所有锁的获取和释放
    // 2. 构建等待图
    // 3. 检测循环等待
    
    // 注意：这是一个复杂的功能，需要深入的系统级支持
    // 这里提供一个基础框架
    
    Q_UNUSED(this);
    // TODO: 实现完整的死锁检测算法
}

QString DiagnosticManager::generateTraceId() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QString("trace_%1_%2").arg(timestamp).arg(uuid);
}

QString DiagnosticManager::generateSnapshotId() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QString("snapshot_%1_%2").arg(timestamp).arg(uuid);
}

QString DiagnosticManager::generateDeadlockId() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QString("deadlock_%1_%2").arg(timestamp).arg(uuid);
}

} // namespace Core
} // namespace Eagle
