#ifndef EAGLE_CORE_CONFIGENCRYPTION_H
#define EAGLE_CORE_CONFIGENCRYPTION_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVariantMap>

namespace Eagle {
namespace Core {

/**
 * @brief 加密算法类型
 */
enum class EncryptionAlgorithm {
    XOR,        // XOR加密（旧版本，向后兼容）
    AES256      // AES-256加密（推荐）
};

/**
 * @brief 密钥版本信息
 */
struct KeyVersion {
    int version;                // 版本号
    EncryptionAlgorithm algorithm; // 加密算法
    QString keyId;              // 密钥ID
    QByteArray salt;            // 盐值（用于PBKDF2）
    int pbkdf2Iterations;       // PBKDF2迭代次数
    
    KeyVersion()
        : version(1)
        , algorithm(EncryptionAlgorithm::AES256)
        , pbkdf2Iterations(100000)
    {}
};

/**
 * @brief 配置加密工具
 */
class ConfigEncryption {
public:
    /**
     * @brief 加密配置值
     * @param value 原始值
     * @param key 加密密钥（如果为空，使用默认密钥）
     * @param algorithm 加密算法（默认AES256）
     * @return 加密后的Base64字符串（包含版本信息）
     */
    static QString encrypt(const QString& value, const QString& key = QString(), 
                          EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES256);
    
    /**
     * @brief 解密配置值（自动检测加密算法版本）
     * @param encryptedValue 加密的Base64字符串
     * @param key 解密密钥（如果为空，使用默认密钥）
     * @return 解密后的原始值
     */
    static QString decrypt(const QString& encryptedValue, const QString& key = QString());
    
    /**
     * @brief 加密整个配置Map中的敏感字段
     * @param config 配置Map
     * @param sensitiveKeys 需要加密的键列表（支持通配符，如 "password*", "*secret*"）
     * @param key 加密密钥
     * @return 加密后的配置Map
     */
    static QVariantMap encryptConfig(const QVariantMap& config, 
                                     const QStringList& sensitiveKeys,
                                     const QString& key = QString());
    
    /**
     * @brief 解密整个配置Map中的敏感字段
     * @param config 加密的配置Map
     * @param sensitiveKeys 需要解密的键列表
     * @param key 解密密钥
     * @return 解密后的配置Map
     */
    static QVariantMap decryptConfig(const QVariantMap& config,
                                    const QStringList& sensitiveKeys,
                                    const QString& key = QString());
    
    /**
     * @brief 设置默认加密密钥
     * @param key 密钥
     */
    static void setDefaultKey(const QString& key);
    
    /**
     * @brief 生成随机密钥
     * @param length 密钥长度（字节）
     * @return Base64编码的密钥
     */
    static QString generateKey(int length = 32);
    
    /**
     * @brief 使用PBKDF2派生密钥
     * @param password 密码
     * @param salt 盐值
     * @param iterations 迭代次数
     * @param keyLength 密钥长度（字节）
     * @return 派生后的密钥
     */
    static QByteArray deriveKeyPBKDF2(const QString& password, const QByteArray& salt, 
                                      int iterations = 100000, int keyLength = 32);
    
    /**
     * @brief 轮换密钥（将旧密钥加密的数据迁移到新密钥）
     * @param oldKey 旧密钥
     * @param newKey 新密钥
     * @param encryptedValue 加密的值
     * @return 使用新密钥加密的值
     */
    static QString rotateKey(const QString& encryptedValue, const QString& oldKey, const QString& newKey);
    
    /**
     * @brief 获取当前密钥版本
     * @return 密钥版本信息
     */
    static KeyVersion getCurrentKeyVersion();
    
    /**
     * @brief 设置密钥版本
     * @param version 密钥版本信息
     */
    static void setKeyVersion(const KeyVersion& version);
    
private:
    static QString getDefaultKey();
    static QByteArray deriveKey(const QString& password);  // 旧方法，向后兼容
    static QByteArray deriveKeyXOR(const QString& password);  // XOR加密的密钥派生
    
    // AES-256加密/解密（使用OpenSSL或Qt实现）
    static QByteArray encryptAES256(const QByteArray& data, const QByteArray& key, const QByteArray& iv);
    static QByteArray decryptAES256(const QByteArray& encryptedData, const QByteArray& key, const QByteArray& iv);
    static QByteArray generateIV();  // 生成初始化向量
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGENCRYPTION_H
