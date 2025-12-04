#ifndef EAGLE_CORE_DIAGNOSTICMANAGER_H
#define EAGLE_CORE_DIAGNOSTICMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>
#include <QtCore/QStack>
#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <functional>

namespace Eagle {
namespace Core {

/**
 * @brief 堆栈帧信息
 */
struct StackFrame {
    QString function;       // 函数名
    QString file;           // 文件名
    int line;               // 行号
    QString address;        // 地址
    QString module;         // 模块名
    
    StackFrame()
        : line(0)
    {
    }
    
    QString toString() const {
        if (!file.isEmpty() && line > 0) {
            return QString("%1 (%2:%3)").arg(function).arg(file).arg(line);
        } else if (!function.isEmpty()) {
            return function;
        } else {
            return address;
        }
    }
};

/**
 * @brief 堆栈跟踪信息
 */
struct StackTrace {
    QString id;             // 跟踪ID
    QString threadId;       // 线程ID
    QString threadName;     // 线程名称
    QDateTime timestamp;   // 时间戳
    QString message;        // 消息
    QList<StackFrame> frames;  // 堆栈帧列表
    QVariantMap context;    // 上下文信息
    
    StackTrace()
    {
        timestamp = QDateTime::currentDateTime();
    }
    
    QString toString() const {
        QString result;
        result += QString("Thread: %1 (%2)\n").arg(threadName).arg(threadId);
        result += QString("Time: %1\n").arg(timestamp.toString(Qt::ISODate));
        if (!message.isEmpty()) {
            result += QString("Message: %1\n").arg(message);
        }
        result += "Stack trace:\n";
        for (int i = 0; i < frames.size(); ++i) {
            result += QString("  #%1 %2\n").arg(i).arg(frames[i].toString());
        }
        return result;
    }
};

/**
 * @brief 内存快照信息
 */
struct MemorySnapshot {
    QString id;             // 快照ID
    QDateTime timestamp;    // 时间戳
    qint64 totalMemory;     // 总内存（字节）
    qint64 usedMemory;      // 已用内存（字节）
    qint64 freeMemory;      // 空闲内存（字节）
    qint64 heapSize;        // 堆大小（字节）
    int objectCount;        // 对象数量
    QVariantMap details;    // 详细信息
    
    MemorySnapshot()
        : totalMemory(0)
        , usedMemory(0)
        , freeMemory(0)
        , heapSize(0)
        , objectCount(0)
    {
        timestamp = QDateTime::currentDateTime();
    }
    
    qint64 memoryUsagePercent() const {
        if (totalMemory == 0) return 0;
        return (usedMemory * 100) / totalMemory;
    }
};

/**
 * @brief 死锁信息
 */
struct DeadlockInfo {
    QString id;             // 死锁ID
    QDateTime timestamp;    // 时间戳
    QStringList threadIds;   // 涉及的线程ID列表
    QStringList lockIds;     // 涉及的锁ID列表
    QString description;     // 描述
    QVariantMap details;     // 详细信息
    
    DeadlockInfo()
    {
        timestamp = QDateTime::currentDateTime();
    }
    
    QString toString() const {
        QString result;
        result += QString("Deadlock detected at %1\n").arg(timestamp.toString(Qt::ISODate));
        result += QString("Threads: %1\n").arg(threadIds.join(", "));
        result += QString("Locks: %1\n").arg(lockIds.join(", "));
        if (!description.isEmpty()) {
            result += QString("Description: %1\n").arg(description);
        }
        return result;
    }
};

/**
 * @brief 诊断管理器
 * 
 * 提供堆栈跟踪、内存快照、死锁检测等诊断功能
 */
class DiagnosticManager : public QObject {
    Q_OBJECT
    
public:
    explicit DiagnosticManager(QObject* parent = nullptr);
    ~DiagnosticManager();
    
    // 堆栈跟踪
    StackTrace captureStackTrace(const QString& message = QString());
    QStringList getStackTraceIds() const;
    StackTrace getStackTrace(const QString& traceId) const;
    bool saveStackTrace(const QString& traceId, const QString& filePath) const;
    
    // 内存快照
    MemorySnapshot captureMemorySnapshot(const QString& name = QString());
    QStringList getMemorySnapshotIds() const;
    MemorySnapshot getMemorySnapshot(const QString& snapshotId) const;
    QVariantMap compareSnapshots(const QString& snapshotId1, const QString& snapshotId2) const;
    bool saveMemorySnapshot(const QString& snapshotId, const QString& filePath) const;
    
    // 死锁检测
    void setDeadlockDetectionEnabled(bool enabled);
    bool isDeadlockDetectionEnabled() const;
    void startDeadlockDetection();
    void stopDeadlockDetection();
    QList<DeadlockInfo> getDeadlocks() const;
    DeadlockInfo getDeadlock(const QString& deadlockId) const;
    bool clearDeadlocks();
    
    // 配置
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setMaxStackTraces(int maxCount);
    int maxStackTraces() const;
    void setMaxMemorySnapshots(int maxCount);
    int maxMemorySnapshots() const;
    
signals:
    void stackTraceCaptured(const QString& traceId);
    void memorySnapshotCaptured(const QString& snapshotId);
    void deadlockDetected(const QString& deadlockId);
    
private slots:
    void onDeadlockDetectionTimer();
    
private:
    Q_DISABLE_COPY(DiagnosticManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    void collectStackFrames(QList<StackFrame>& frames);
    void detectDeadlocks();
    QString generateTraceId() const;
    QString generateSnapshotId() const;
    QString generateDeadlockId() const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::StackFrame)
Q_DECLARE_METATYPE(Eagle::Core::StackTrace)
Q_DECLARE_METATYPE(Eagle::Core::MemorySnapshot)
Q_DECLARE_METATYPE(Eagle::Core::DeadlockInfo)

#endif // EAGLE_CORE_DIAGNOSTICMANAGER_H
