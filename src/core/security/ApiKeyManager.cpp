#include "eagle/core/ApiKeyManager.h"
#include "ApiKeyManager_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QDateTime>
#include <QtCore/QUuid>
#include <QtCore/QCryptographicHash>
#include <QtCore/QByteArray>

namespace Eagle {
namespace Core {

ApiKeyManager::ApiKeyManager(QObject* parent)
    : QObject(parent)
    , d(new ApiKeyManager::Private)
{
    Logger::info("ApiKeyManager", "API密钥管理器初始化完成");
}

ApiKeyManager::~ApiKeyManager()
{
    delete d;
}

ApiKey ApiKeyManager::createKey(const QString& userId, const QString& description,
                                 const QDateTime& expiresAt, const QStringList& permissions)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    ApiKey key;
    key.keyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    key.keyValue = generateKeyValue();
    key.userId = userId;
    key.description = description;
    key.permissions = permissions;
    key.enabled = true;
    
    if (expiresAt.isValid()) {
        key.expiresAt = expiresAt;
    } else if (d->defaultExpirationDays > 0) {
        key.expiresAt = QDateTime::currentDateTime().addDays(d->defaultExpirationDays);
    }
    
    d->keys[key.keyId] = key;
    d->keyValueToId[key.keyValue] = key.keyId;
    
    Logger::info("ApiKeyManager", QString("创建API密钥: %1 (用户: %2)").arg(key.keyId, userId));
    emit keyCreated(key.keyId, userId);
    
    return key;
}

bool ApiKeyManager::removeKey(const QString& keyId)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->keys.contains(keyId)) {
        return false;
    }
    
    ApiKey key = d->keys[keyId];
    d->keyValueToId.remove(key.keyValue);
    d->keys.remove(keyId);
    
    Logger::info("ApiKeyManager", QString("删除API密钥: %1").arg(keyId));
    emit keyRemoved(keyId);
    
    return true;
}

bool ApiKeyManager::updateKey(const ApiKey& key)
{
    if (!key.isValid()) {
        return false;
    }
    
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->keys.contains(key.keyId)) {
        return false;
    }
    
    ApiKey oldKey = d->keys[key.keyId];
    
    // 如果密钥值改变，更新映射
    if (oldKey.keyValue != key.keyValue) {
        d->keyValueToId.remove(oldKey.keyValue);
        d->keyValueToId[key.keyValue] = key.keyId;
    }
    
    d->keys[key.keyId] = key;
    
    Logger::info("ApiKeyManager", QString("更新API密钥: %1").arg(key.keyId));
    emit keyUpdated(key.keyId);
    
    return true;
}

ApiKey ApiKeyManager::getKey(const QString& keyId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->keys.value(keyId);
}

ApiKey ApiKeyManager::getKeyByValue(const QString& keyValue) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->keyValueToId.contains(keyValue)) {
        return ApiKey();
    }
    
    QString keyId = d->keyValueToId[keyValue];
    return d->keys.value(keyId);
}

QStringList ApiKeyManager::getAllKeyIds() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->keys.keys();
}

QStringList ApiKeyManager::getKeysByUser(const QString& userId) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QStringList keyIds;
    for (auto it = d->keys.begin(); it != d->keys.end(); ++it) {
        if (it.value().userId == userId) {
            keyIds.append(it.key());
        }
    }
    return keyIds;
}

bool ApiKeyManager::validateKey(const QString& keyValue) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->keyValueToId.contains(keyValue)) {
        return false;
    }
    
    QString keyId = d->keyValueToId[keyValue];
    const ApiKey& key = d->keys[keyId];
    
    bool valid = key.enabled && !key.isExpired();
    
    emit const_cast<ApiKeyManager*>(this)->keyValidated(keyId, valid);
    
    return valid;
}

bool ApiKeyManager::checkPermission(const QString& keyValue, const QString& permission) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->keyValueToId.contains(keyValue)) {
        return false;
    }
    
    QString keyId = d->keyValueToId[keyValue];
    const ApiKey& key = d->keys[keyId];
    
    if (!key.enabled || key.isExpired()) {
        return false;
    }
    
    // 检查权限列表
    if (key.permissions.isEmpty()) {
        return true;  // 没有权限限制，允许所有操作
    }
    
    return key.permissions.contains(permission);
}

void ApiKeyManager::setKeyExpiration(int days)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->defaultExpirationDays = days;
    Logger::info("ApiKeyManager", QString("设置默认密钥过期天数: %1").arg(days));
}

int ApiKeyManager::getKeyExpiration() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->defaultExpirationDays;
}

QString ApiKeyManager::generateKeyValue() const
{
    // 生成格式：eagle_<随机UUID>_<时间戳>
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // 使用SHA256生成更安全的密钥
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(uuid.toUtf8());
    hash.addData(QByteArray::number(timestamp));
    QByteArray hashResult = hash.result();
    
    // 转换为Base64，但使用URL安全的字符
    QString keyValue = QString("eagle_%1_%2")
        .arg(uuid.left(16))  // 使用UUID的前16位
        .arg(QString::fromLatin1(hashResult.toBase64().left(32)));  // Base64的前32位
    
    return keyValue;
}

} // namespace Core
} // namespace Eagle
