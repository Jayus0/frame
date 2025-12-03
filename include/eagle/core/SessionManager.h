#ifndef EAGLE_CORE_SESSIONMANAGER_H
#define EAGLE_CORE_SESSIONMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtCore/QUuid>

namespace Eagle {
namespace Core {

/**
 * @brief 会话信息
 */
struct Session {
    QString sessionId;           // 会话ID
    QString userId;              // 用户ID
    QDateTime createdAt;         // 创建时间
    QDateTime lastAccessTime;    // 最后访问时间
    QDateTime expiresAt;         // 过期时间
    bool active;                 // 是否活跃
    QVariantMap attributes;      // 会话属性
    
    Session()
        : active(true)
    {
        createdAt = QDateTime::currentDateTime();
        lastAccessTime = createdAt;
    }
    
    bool isValid() const {
        return !sessionId.isEmpty() && !userId.isEmpty();
    }
    
    bool isExpired() const {
        if (expiresAt.isNull()) {
            return false;
        }
        return QDateTime::currentDateTime() > expiresAt;
    }
    
    void touch() {
        lastAccessTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 会话管理器
 */
class SessionManager : public QObject {
    Q_OBJECT
    
public:
    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager();
    
    // 会话管理
    QString createSession(const QString& userId, int timeoutMinutes = 30);
    bool destroySession(const QString& sessionId);
    bool validateSession(const QString& sessionId);
    Session getSession(const QString& sessionId) const;
    
    // 会话属性
    void setAttribute(const QString& sessionId, const QString& key, const QVariant& value);
    QVariant getAttribute(const QString& sessionId, const QString& key) const;
    void removeAttribute(const QString& sessionId, const QString& key);
    
    // 会话查询
    QStringList getSessionsByUser(const QString& userId) const;
    int getActiveSessionCount() const;
    int getActiveSessionCount(const QString& userId) const;
    
    // 配置
    void setDefaultTimeout(int minutes);
    int getDefaultTimeout() const;
    void setMaxSessionsPerUser(int maxSessions);
    int getMaxSessionsPerUser() const;
    
    // 清理
    void cleanupExpiredSessions();
    void cleanupInactiveSessions(int inactiveMinutes);
    
signals:
    void sessionCreated(const QString& sessionId, const QString& userId);
    void sessionDestroyed(const QString& sessionId);
    void sessionExpired(const QString& sessionId);
    
private slots:
    void onCleanupTimer();
    
private:
    Q_DISABLE_COPY(SessionManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    QString generateSessionId() const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::Session)

#endif // EAGLE_CORE_SESSIONMANAGER_H
