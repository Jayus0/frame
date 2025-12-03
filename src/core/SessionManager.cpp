#include "eagle/core/SessionManager.h"
#include "SessionManager_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>
#include <QtCore/QTimer>

namespace Eagle {
namespace Core {

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
    , d(new SessionManager::Private)
{
    d->cleanupTimer = new QTimer(this);
    d->cleanupTimer->setInterval(60000);  // 每分钟清理一次
    connect(d->cleanupTimer, &QTimer::timeout, this, &SessionManager::onCleanupTimer);
    d->cleanupTimer->start();
    
    Logger::info("SessionManager", "会话管理器初始化完成");
}

SessionManager::~SessionManager()
{
    delete d;
}

QString SessionManager::createSession(const QString& userId, int timeoutMinutes)
{
    if (userId.isEmpty()) {
        Logger::error("SessionManager", "无法为空的用户ID创建会话");
        return QString();
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 检查用户会话数量限制
    if (d->userSessions.contains(userId)) {
        QStringList& userSessions = d->userSessions[userId];
        if (userSessions.size() >= d->maxSessionsPerUser) {
            // 删除最旧的会话
            QString oldestSessionId = userSessions.first();
            destroySession(oldestSessionId);
        }
    }
    
    // 创建新会话
    Session session;
    session.sessionId = generateSessionId();
    session.userId = userId;
    session.createdAt = QDateTime::currentDateTime();
    session.lastAccessTime = session.createdAt;
    session.active = true;
    
    int timeout = timeoutMinutes > 0 ? timeoutMinutes : d->defaultTimeoutMinutes;
    session.expiresAt = QDateTime::currentDateTime().addSecs(timeout * 60);
    
    d->sessions[session.sessionId] = session;
    
    if (!d->userSessions.contains(userId)) {
        d->userSessions[userId] = QStringList();
    }
    d->userSessions[userId].append(session.sessionId);
    
    Logger::info("SessionManager", QString("创建会话: %1 (用户: %2)").arg(session.sessionId, userId));
    emit sessionCreated(session.sessionId, userId);
    
    return session.sessionId;
}

bool SessionManager::destroySession(const QString& sessionId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        return false;
    }
    
    Session session = d->sessions[sessionId];
    d->sessions.remove(sessionId);
    
    // 从用户会话列表中移除
    if (d->userSessions.contains(session.userId)) {
        d->userSessions[session.userId].removeAll(sessionId);
        if (d->userSessions[session.userId].isEmpty()) {
            d->userSessions.remove(session.userId);
        }
    }
    
    Logger::info("SessionManager", QString("销毁会话: %1").arg(sessionId));
    emit sessionDestroyed(sessionId);
    
    return true;
}

bool SessionManager::validateSession(const QString& sessionId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        return false;
    }
    
    Session& session = d->sessions[sessionId];
    
    // 检查是否过期
    if (session.isExpired()) {
        session.active = false;
        d->sessions.remove(sessionId);
        
        // 从用户会话列表中移除
        if (d->userSessions.contains(session.userId)) {
            d->userSessions[session.userId].removeAll(sessionId);
            if (d->userSessions[session.userId].isEmpty()) {
                d->userSessions.remove(session.userId);
            }
        }
        
        Logger::info("SessionManager", QString("会话已过期: %1").arg(sessionId));
        emit sessionExpired(sessionId);
        return false;
    }
    
    // 更新最后访问时间
    session.touch();
    
    return session.active;
}

Session SessionManager::getSession(const QString& sessionId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        return Session();
    }
    
    Session session = d->sessions[sessionId];
    
    // 检查是否过期
    if (session.isExpired()) {
        return Session();
    }
    
    return session;
}

void SessionManager::setAttribute(const QString& sessionId, const QString& key, const QVariant& value)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        Logger::warning("SessionManager", QString("会话不存在: %1").arg(sessionId));
        return;
    }
    
    d->sessions[sessionId].attributes[key] = value;
}

QVariant SessionManager::getAttribute(const QString& sessionId, const QString& key) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        return QVariant();
    }
    
    return d->sessions[sessionId].attributes.value(key);
}

void SessionManager::removeAttribute(const QString& sessionId, const QString& key)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->sessions.contains(sessionId)) {
        return;
    }
    
    d->sessions[sessionId].attributes.remove(key);
}

QStringList SessionManager::getSessionsByUser(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->userSessions.value(userId);
}

int SessionManager::getActiveSessionCount() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    int count = 0;
    for (const Session& session : d->sessions.values()) {
        if (session.active && !session.isExpired()) {
            count++;
        }
    }
    return count;
}

int SessionManager::getActiveSessionCount(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->userSessions.contains(userId)) {
        return 0;
    }
    
    int count = 0;
    for (const QString& sessionId : d->userSessions[userId]) {
        if (d->sessions.contains(sessionId)) {
            const Session& session = d->sessions[sessionId];
            if (session.active && !session.isExpired()) {
                count++;
            }
        }
    }
    return count;
}

void SessionManager::setDefaultTimeout(int minutes)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->defaultTimeoutMinutes = minutes;
    Logger::info("SessionManager", QString("设置默认会话超时: %1分钟").arg(minutes));
}

int SessionManager::getDefaultTimeout() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->defaultTimeoutMinutes;
}

void SessionManager::setMaxSessionsPerUser(int maxSessions)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->maxSessionsPerUser = maxSessions;
    Logger::info("SessionManager", QString("设置每用户最大会话数: %1").arg(maxSessions));
}

int SessionManager::getMaxSessionsPerUser() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->maxSessionsPerUser;
}

void SessionManager::cleanupExpiredSessions()
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList expiredSessions;
    for (auto it = d->sessions.begin(); it != d->sessions.end(); ++it) {
        if (it.value().isExpired()) {
            expiredSessions.append(it.key());
        }
    }
    
    locker.unlock();
    
    for (const QString& sessionId : expiredSessions) {
        destroySession(sessionId);
        emit sessionExpired(sessionId);
    }
    
    if (!expiredSessions.isEmpty()) {
        Logger::info("SessionManager", QString("清理了 %1 个过期会话").arg(expiredSessions.size()));
    }
}

void SessionManager::cleanupInactiveSessions(int inactiveMinutes)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-inactiveMinutes * 60);
    QStringList inactiveSessions;
    
    for (auto it = d->sessions.begin(); it != d->sessions.end(); ++it) {
        if (it.value().lastAccessTime < cutoff) {
            inactiveSessions.append(it.key());
        }
    }
    
    locker.unlock();
    
    for (const QString& sessionId : inactiveSessions) {
        destroySession(sessionId);
    }
    
    if (!inactiveSessions.isEmpty()) {
        Logger::info("SessionManager", QString("清理了 %1 个非活跃会话").arg(inactiveSessions.size()));
    }
}

void SessionManager::onCleanupTimer()
{
    cleanupExpiredSessions();
}

QString SessionManager::generateSessionId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace Core
} // namespace Eagle
