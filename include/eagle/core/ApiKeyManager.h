#ifndef EAGLE_CORE_APIKEYMANAGER_H
#define EAGLE_CORE_APIKEYMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QUuid>

namespace Eagle {
namespace Core {

/**
 * @brief API密钥信息
 */
struct ApiKey {
    QString keyId;              // 密钥ID
    QString keyValue;            // 密钥值
    QString userId;              // 关联用户ID
    QString description;         // 描述
    QDateTime createdAt;         // 创建时间
    QDateTime expiresAt;         // 过期时间（空表示永不过期）
    bool enabled;                // 是否启用
    QStringList permissions;     // 权限列表
    
    ApiKey()
        : enabled(true)
    {
        createdAt = QDateTime::currentDateTime();
    }
    
    bool isValid() const {
        return !keyId.isEmpty() && !keyValue.isEmpty();
    }
    
    bool isExpired() const {
        if (expiresAt.isNull()) {
            return false;
        }
        return QDateTime::currentDateTime() > expiresAt;
    }
};

/**
 * @brief API密钥管理器
 */
class ApiKeyManager : public QObject {
    Q_OBJECT
    
public:
    explicit ApiKeyManager(QObject* parent = nullptr);
    ~ApiKeyManager();
    
    // 密钥管理
    ApiKey createKey(const QString& userId, const QString& description = QString(),
                    const QDateTime& expiresAt = QDateTime(), 
                    const QStringList& permissions = QStringList());
    bool removeKey(const QString& keyId);
    bool updateKey(const ApiKey& key);
    ApiKey getKey(const QString& keyId) const;
    ApiKey getKeyByValue(const QString& keyValue) const;
    QStringList getAllKeyIds() const;
    QStringList getKeysByUser(const QString& userId) const;
    
    // 密钥验证
    bool validateKey(const QString& keyValue) const;
    bool checkPermission(const QString& keyValue, const QString& permission) const;
    
    // 配置
    void setKeyExpiration(int days);  // 设置默认过期天数
    int getKeyExpiration() const;
    
signals:
    void keyCreated(const QString& keyId, const QString& userId);
    void keyRemoved(const QString& keyId);
    void keyUpdated(const QString& keyId);
    void keyValidated(const QString& keyId, bool valid);
    
private:
    Q_DISABLE_COPY(ApiKeyManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    QString generateKeyValue() const;
};

} // namespace Core
} // namespace Eagle

Q_DECLARE_METATYPE(Eagle::Core::ApiKey)

#endif // EAGLE_CORE_APIKEYMANAGER_H
