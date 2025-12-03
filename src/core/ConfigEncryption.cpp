#include "eagle/core/ConfigEncryption.h"
#include "eagle/core/Logger.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QRandomGenerator>

namespace Eagle {
namespace Core {

static QString s_defaultKey;

QString ConfigEncryption::encrypt(const QString& value, const QString& key)
{
    if (value.isEmpty()) {
        return value;
    }
    
    QString actualKey = key.isEmpty() ? getDefaultKey() : key;
    QByteArray keyData = deriveKey(actualKey);
    QByteArray data = value.toUtf8();
    
    // 简单的XOR加密（生产环境应使用AES）
    QByteArray encrypted;
    encrypted.resize(data.size());
    for (int i = 0; i < data.size(); ++i) {
        encrypted[i] = data[i] ^ keyData[i % keyData.size()];
    }
    
    // 添加简单的校验和
    QByteArray hash = QCryptographicHash::hash(encrypted + keyData, QCryptographicHash::Sha256);
    encrypted.append(hash.left(4));  // 添加4字节校验
    
    return QString::fromUtf8(encrypted.toBase64());
}

QString ConfigEncryption::decrypt(const QString& encryptedValue, const QString& key)
{
    if (encryptedValue.isEmpty()) {
        return encryptedValue;
    }
    
    QString actualKey = key.isEmpty() ? getDefaultKey() : key;
    QByteArray keyData = deriveKey(actualKey);
    
    QByteArray encrypted = QByteArray::fromBase64(encryptedValue.toUtf8());
    if (encrypted.size() < 4) {
        Logger::error("ConfigEncryption", "加密数据格式错误");
        return QString();
    }
    
    // 验证校验和
    QByteArray data = encrypted.left(encrypted.size() - 4);
    QByteArray checksum = encrypted.right(4);
    QByteArray expectedHash = QCryptographicHash::hash(data + keyData, QCryptographicHash::Sha256);
    
    if (checksum != expectedHash.left(4)) {
        Logger::error("ConfigEncryption", "校验和验证失败，数据可能被篡改");
        return QString();
    }
    
    // 解密
    QByteArray decrypted;
    decrypted.resize(data.size());
    for (int i = 0; i < data.size(); ++i) {
        decrypted[i] = data[i] ^ keyData[i % keyData.size()];
    }
    
    return QString::fromUtf8(decrypted);
}

QVariantMap ConfigEncryption::encryptConfig(const QVariantMap& config,
                                            const QStringList& sensitiveKeys,
                                            const QString& key)
{
    QVariantMap result = config;
    
    for (auto it = result.begin(); it != result.end(); ++it) {
        QString keyName = it.key();
        QVariant value = it.value();
        
        // 检查是否需要加密
        bool shouldEncrypt = false;
        for (const QString& pattern : sensitiveKeys) {
            if (keyName.contains(pattern.replace("*", ""), Qt::CaseInsensitive)) {
                shouldEncrypt = true;
                break;
            }
        }
        
        if (shouldEncrypt) {
            if (value.type() == QVariant::String) {
                QString encrypted = encrypt(value.toString(), key);
                result[keyName] = QString("ENC:%1").arg(encrypted);  // 添加前缀标识
            } else if (value.type() == QVariant::Map) {
                // 递归处理嵌套Map
                result[keyName] = encryptConfig(value.toMap(), sensitiveKeys, key);
            }
        } else if (value.type() == QVariant::Map) {
            // 递归处理嵌套Map
            result[keyName] = encryptConfig(value.toMap(), sensitiveKeys, key);
        }
    }
    
    return result;
}

QVariantMap ConfigEncryption::decryptConfig(const QVariantMap& config,
                                            const QStringList& sensitiveKeys,
                                            const QString& key)
{
    QVariantMap result = config;
    
    for (auto it = result.begin(); it != result.end(); ++it) {
        QString keyName = it.key();
        QVariant value = it.value();
        
        if (value.type() == QVariant::String) {
            QString strValue = value.toString();
            // 检查是否是加密值
            if (strValue.startsWith("ENC:")) {
                QString encrypted = strValue.mid(4);
                QString decrypted = decrypt(encrypted, key);
                if (!decrypted.isEmpty()) {
                    result[keyName] = decrypted;
                } else {
                    Logger::warning("ConfigEncryption", QString("解密失败: %1").arg(keyName));
                }
            }
        } else if (value.type() == QVariant::Map) {
            // 递归处理嵌套Map
            result[keyName] = decryptConfig(value.toMap(), sensitiveKeys, key);
        }
    }
    
    return result;
}

void ConfigEncryption::setDefaultKey(const QString& key)
{
    s_defaultKey = key;
}

QString ConfigEncryption::generateKey(int length)
{
    QByteArray key;
    key.resize(length);
    
    QRandomGenerator* rng = QRandomGenerator::global();
    for (int i = 0; i < length; ++i) {
        key[i] = static_cast<char>(rng->bounded(256));
    }
    
    return QString::fromUtf8(key.toBase64());
}

QString ConfigEncryption::getDefaultKey()
{
    if (!s_defaultKey.isEmpty()) {
        return s_defaultKey;
    }
    
    // 尝试从文件读取
    QString keyFile = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/encryption.key";
    QFile file(keyFile);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        s_defaultKey = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
        return s_defaultKey;
    }
    
    // 生成新密钥并保存
    s_defaultKey = generateKey(32);
    QDir dir = QFileInfo(keyFile).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    if (file.open(QIODevice::WriteOnly)) {
        file.write(s_defaultKey.toUtf8());
        file.close();
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);  // 限制权限
        Logger::info("ConfigEncryption", "生成并保存默认加密密钥");
    }
    
    return s_defaultKey;
}

QByteArray ConfigEncryption::deriveKey(const QString& password)
{
    // 使用PBKDF2派生密钥（简化实现）
    QByteArray salt = "EagleFramework2024";  // 固定盐值（生产环境应随机生成）
    QByteArray key = QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256);
    
    // 扩展到32字节
    while (key.size() < 32) {
        key.append(QCryptographicHash::hash(key, QCryptographicHash::Sha256));
    }
    
    return key.left(32);
}

} // namespace Core
} // namespace Eagle
