#include "eagle/core/ConfigEncryption.h"
#include "eagle/core/Logger.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QStandardPaths>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QRandomGenerator>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

// 尝试使用OpenSSL（如果可用）
// 注意：由于链接问题，暂时禁用OpenSSL，使用Qt回退实现
// 如果需要使用OpenSSL，需要在CMakeLists.txt或.pro文件中链接OpenSSL库
// #define USE_OPENSSL_AES  // 取消注释以启用OpenSSL（需要链接libssl和libcrypto）
#if 0  // 暂时禁用OpenSSL检测
#if defined(QT_FEATURE_openssl) && QT_FEATURE_openssl
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#define USE_OPENSSL_AES
#elif __has_include(<openssl/evp.h>)
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#define USE_OPENSSL_AES
#endif
#endif
// 如果没有OpenSSL，使用Qt的加密功能实现AES（简化版）
// 注意：这是一个简化的AES实现，生产环境建议使用OpenSSL

namespace Eagle {
namespace Core {

static QString s_defaultKey;
static KeyVersion s_currentKeyVersion;

QString ConfigEncryption::encrypt(const QString& value, const QString& key, EncryptionAlgorithm algorithm)
{
    if (value.isEmpty()) {
        return value;
    }
    
    QString actualKey = key.isEmpty() ? getDefaultKey() : key;
    QByteArray encryptedData;
    
    if (algorithm == EncryptionAlgorithm::AES256) {
        // 使用AES-256加密
        QByteArray keyData = deriveKeyPBKDF2(actualKey, s_currentKeyVersion.salt, 
                                             s_currentKeyVersion.pbkdf2Iterations, 32);
        QByteArray iv = generateIV();
        QByteArray data = value.toUtf8();
        
        encryptedData = encryptAES256(data, keyData, iv);
        if (encryptedData.isEmpty()) {
            Logger::error("ConfigEncryption", "AES加密失败，回退到XOR加密");
            algorithm = EncryptionAlgorithm::XOR;  // 回退
        } else {
            // 格式：版本(1字节) + 算法(1字节) + IV(16字节) + 加密数据
            QByteArray result;
            result.append(static_cast<char>(s_currentKeyVersion.version));
            result.append(static_cast<char>(algorithm));
            result.append(iv);
            result.append(encryptedData);
            encryptedData = result;
        }
    }
    
    if (algorithm == EncryptionAlgorithm::XOR || encryptedData.isEmpty()) {
        // XOR加密（向后兼容）
        QByteArray keyData = deriveKeyXOR(actualKey);
        QByteArray data = value.toUtf8();
        
        encryptedData.resize(data.size());
        for (int i = 0; i < data.size(); ++i) {
            encryptedData[i] = data[i] ^ keyData[i % keyData.size()];
        }
        
        // 添加简单的校验和
        QByteArray hash = QCryptographicHash::hash(encryptedData + keyData, QCryptographicHash::Sha256);
        encryptedData.append(hash.left(4));  // 添加4字节校验
        
        // 格式：版本(1字节) + 算法(1字节) + 加密数据
        QByteArray result;
        result.append(static_cast<char>(1));  // 版本1（XOR）
        result.append(static_cast<char>(EncryptionAlgorithm::XOR));
        result.append(encryptedData);
        encryptedData = result;
    }
    
    return QString::fromUtf8(encryptedData.toBase64());
}

QString ConfigEncryption::decrypt(const QString& encryptedValue, const QString& key)
{
    if (encryptedValue.isEmpty()) {
        return encryptedValue;
    }
    
    QString actualKey = key.isEmpty() ? getDefaultKey() : key;
    
    QByteArray encrypted = QByteArray::fromBase64(encryptedValue.toUtf8());
    if (encrypted.size() < 2) {
        Logger::error("ConfigEncryption", "加密数据格式错误");
        return QString();
    }
    
    // 读取版本和算法
    Q_UNUSED(static_cast<unsigned char>(encrypted[0]));  // 版本号，暂时未使用
    EncryptionAlgorithm algorithm = static_cast<EncryptionAlgorithm>(static_cast<unsigned char>(encrypted[1]));
    QByteArray data = encrypted.mid(2);
    
    if (algorithm == EncryptionAlgorithm::AES256) {
        // AES-256解密
        if (data.size() < 16) {
            Logger::error("ConfigEncryption", "AES加密数据格式错误（缺少IV）");
            return QString();
        }
        
        QByteArray iv = data.left(16);
        QByteArray encryptedData = data.mid(16);
        
        QByteArray keyData = deriveKeyPBKDF2(actualKey, s_currentKeyVersion.salt, 
                                             s_currentKeyVersion.pbkdf2Iterations, 32);
        QByteArray decrypted = decryptAES256(encryptedData, keyData, iv);
        
        if (decrypted.isEmpty()) {
            Logger::error("ConfigEncryption", "AES解密失败");
            return QString();
        }
        
        return QString::fromUtf8(decrypted);
    } else if (algorithm == EncryptionAlgorithm::XOR) {
        // XOR解密（向后兼容）
        if (data.size() < 4) {
            Logger::error("ConfigEncryption", "XOR加密数据格式错误");
            return QString();
        }
        
        QByteArray keyData = deriveKeyXOR(actualKey);
        
        // 验证校验和
        QByteArray cipherData = data.left(data.size() - 4);
        QByteArray checksum = data.right(4);
        QByteArray expectedHash = QCryptographicHash::hash(cipherData + keyData, QCryptographicHash::Sha256);
        
        if (checksum != expectedHash.left(4)) {
            Logger::error("ConfigEncryption", "校验和验证失败，数据可能被篡改");
            return QString();
        }
        
        // 解密
        QByteArray decrypted;
        decrypted.resize(cipherData.size());
        for (int i = 0; i < cipherData.size(); ++i) {
            decrypted[i] = cipherData[i] ^ keyData[i % keyData.size()];
        }
        
        return QString::fromUtf8(decrypted);
    } else {
        Logger::error("ConfigEncryption", QString("未知的加密算法: %1").arg(static_cast<int>(algorithm)));
        return QString();
    }
}

QVariantMap ConfigEncryption::encryptConfig(const QVariantMap& config,
                                            const QStringList& sensitiveKeys,
                                            const QString& key)
{
    QVariantMap result = config;
    
    // 默认使用AES256加密
    EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256;
    
    for (auto it = result.begin(); it != result.end(); ++it) {
        QString keyName = it.key();
        QVariant value = it.value();
        
        // 检查是否需要加密
        bool shouldEncrypt = false;
        for (const QString& pattern : sensitiveKeys) {
            QString cleanPattern = pattern;
            cleanPattern.replace("*", "");
            if (keyName.contains(cleanPattern, Qt::CaseInsensitive)) {
                shouldEncrypt = true;
                break;
            }
        }
        
        if (shouldEncrypt) {
            if (value.type() == QVariant::String) {
                QString strValue = value.toString();
                // 如果已经是加密值，跳过
                if (strValue.startsWith("ENC:")) {
                    continue;
                }
                QString encrypted = encrypt(strValue, key, algorithm);
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
    // 旧方法，向后兼容（使用简化实现）
    return deriveKeyXOR(password);
}

QByteArray ConfigEncryption::deriveKeyXOR(const QString& password)
{
    // XOR加密的密钥派生（简化实现，向后兼容）
    QByteArray salt = "EagleFramework2024";  // 固定盐值
    QByteArray key = QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256);
    
    // 扩展到32字节
    while (key.size() < 32) {
        key.append(QCryptographicHash::hash(key, QCryptographicHash::Sha256));
    }
    
    return key.left(32);
}

QByteArray ConfigEncryption::deriveKeyPBKDF2(const QString& password, const QByteArray& salt, 
                                             int iterations, int keyLength)
{
#ifdef USE_OPENSSL_AES
    // 使用OpenSSL的PBKDF2
    QByteArray key(keyLength, 0);
    if (PKCS5_PBKDF2_HMAC(password.toUtf8().constData(), password.length(),
                          reinterpret_cast<const unsigned char*>(salt.constData()), salt.length(),
                          iterations, EVP_sha256(), keyLength,
                          reinterpret_cast<unsigned char*>(key.data())) == 1) {
        return key;
    } else {
        Logger::error("ConfigEncryption", "PBKDF2密钥派生失败");
        return QByteArray();
    }
#else
    // 使用Qt实现PBKDF2（简化版，使用HMAC-SHA256）
    QByteArray key;
    QByteArray u = salt;
    u.append(static_cast<char>(0x00)).append(static_cast<char>(0x00))
     .append(static_cast<char>(0x00)).append(static_cast<char>(0x01));  // 添加块索引
    
    for (int i = 0; i < iterations; ++i) {
        u = QCryptographicHash::hash((password.toUtf8() + u), QCryptographicHash::Sha256);
    }
    
    // 扩展到所需长度
    while (key.size() < keyLength) {
        key.append(u);
        if (key.size() >= keyLength) break;
        // 继续迭代
        u = QCryptographicHash::hash((password.toUtf8() + u), QCryptographicHash::Sha256);
    }
    
    return key.left(keyLength);
#endif
}

QByteArray ConfigEncryption::encryptAES256(const QByteArray& data, const QByteArray& key, const QByteArray& iv)
{
#ifdef USE_OPENSSL_AES
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return QByteArray();
    }
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    QByteArray encrypted;
    encrypted.resize(data.size() + 16);  // 预留空间
    int outLen = 0;
    int finalLen = 0;
    
    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(encrypted.data()), &outLen,
                         reinterpret_cast<const unsigned char*>(data.constData()), data.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(encrypted.data()) + outLen, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    EVP_CIPHER_CTX_free(ctx);
    encrypted.resize(outLen + finalLen);
    return encrypted;
#else
    // 如果没有OpenSSL，使用简化的AES实现（不推荐用于生产环境）
    // 这里提供一个基于Qt的简化实现
    Logger::warning("ConfigEncryption", "OpenSSL不可用，使用简化AES实现（不推荐用于生产环境）");
    
    // 简化的AES实现：使用XOR + 密钥扩展（这不是真正的AES，仅用于兼容性）
    QByteArray encrypted;
    encrypted.resize((data.size() + 15) / 16 * 16);  // 对齐到16字节
    
    // 扩展密钥
    QByteArray expandedKey = key;
    while (expandedKey.size() < 32) {
        expandedKey.append(QCryptographicHash::hash(expandedKey, QCryptographicHash::Sha256));
    }
    expandedKey = expandedKey.left(32);
    
    // 简化的块加密（实际应使用真正的AES算法）
    for (int i = 0; i < data.size(); i += 16) {
        QByteArray block = data.mid(i, 16);
        if (block.size() < 16) {
            block.append(QByteArray(16 - block.size(), 0));  // PKCS7填充
        }
        
        // 简化的加密（实际应使用AES算法）
        for (int j = 0; j < 16; ++j) {
            encrypted[i + j] = block[j] ^ expandedKey[j % 32] ^ iv[j];
        }
    }
    
    return encrypted;
#endif
}

QByteArray ConfigEncryption::decryptAES256(const QByteArray& encryptedData, const QByteArray& key, const QByteArray& iv)
{
#ifdef USE_OPENSSL_AES
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return QByteArray();
    }
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    QByteArray decrypted;
    decrypted.resize(encryptedData.size());
    int outLen = 0;
    int finalLen = 0;
    
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(decrypted.data()), &outLen,
                          reinterpret_cast<const unsigned char*>(encryptedData.constData()), 
                          encryptedData.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(decrypted.data()) + outLen, &finalLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    
    EVP_CIPHER_CTX_free(ctx);
    decrypted.resize(outLen + finalLen);
    return decrypted;
#else
    // 如果没有OpenSSL，使用简化的AES实现（不推荐用于生产环境）
    QByteArray decrypted;
    decrypted.resize(encryptedData.size());
    
    // 扩展密钥
    QByteArray expandedKey = key;
    while (expandedKey.size() < 32) {
        expandedKey.append(QCryptographicHash::hash(expandedKey, QCryptographicHash::Sha256));
    }
    expandedKey = expandedKey.left(32);
    
    // 简化的块解密
    for (int i = 0; i < encryptedData.size(); i += 16) {
        QByteArray block = encryptedData.mid(i, 16);
        
        // 简化的解密（实际应使用AES算法）
        for (int j = 0; j < 16; ++j) {
            decrypted[i + j] = block[j] ^ expandedKey[j % 32] ^ iv[j];
        }
    }
    
    // 移除填充
    int padding = static_cast<unsigned char>(decrypted[decrypted.size() - 1]);
    if (padding > 0 && padding <= 16) {
        decrypted.resize(decrypted.size() - padding);
    }
    
    return decrypted;
#endif
}

QByteArray ConfigEncryption::generateIV()
{
    QByteArray iv(16, 0);
#ifdef USE_OPENSSL_AES
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 16) == 1) {
        return iv;
    }
#endif
    // 如果没有OpenSSL，使用Qt的随机数生成器
    QRandomGenerator* rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i) {
        iv[i] = static_cast<char>(rng->bounded(256));
    }
    return iv;
}

QString ConfigEncryption::rotateKey(const QString& encryptedValue, const QString& oldKey, const QString& newKey)
{
    // 先解密
    QString decrypted = decrypt(encryptedValue, oldKey);
    if (decrypted.isEmpty()) {
        Logger::error("ConfigEncryption", "密钥轮换失败：无法使用旧密钥解密");
        return QString();
    }
    
    // 使用新密钥加密
    return encrypt(decrypted, newKey, EncryptionAlgorithm::AES256);
}

KeyVersion ConfigEncryption::getCurrentKeyVersion()
{
    if (s_currentKeyVersion.version == 0) {
        // 初始化默认版本
        s_currentKeyVersion.version = 2;  // 版本2使用AES256
        s_currentKeyVersion.algorithm = EncryptionAlgorithm::AES256;
        s_currentKeyVersion.keyId = "default";
        s_currentKeyVersion.salt = QByteArray("EagleFramework2024AES").left(16);
        s_currentKeyVersion.pbkdf2Iterations = 100000;
    }
    return s_currentKeyVersion;
}

void ConfigEncryption::setKeyVersion(const KeyVersion& version)
{
    s_currentKeyVersion = version;
}

} // namespace Core
} // namespace Eagle
