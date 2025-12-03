#ifndef EAGLE_CORE_CONFIGENCRYPTION_H
#define EAGLE_CORE_CONFIGENCRYPTION_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVariantMap>

namespace Eagle {
namespace Core {

/**
 * @brief 配置加密工具
 */
class ConfigEncryption {
public:
    /**
     * @brief 加密配置值
     * @param value 原始值
     * @param key 加密密钥（如果为空，使用默认密钥）
     * @return 加密后的Base64字符串
     */
    static QString encrypt(const QString& value, const QString& key = QString());
    
    /**
     * @brief 解密配置值
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
    
private:
    static QString getDefaultKey();
    static QByteArray deriveKey(const QString& password);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_CONFIGENCRYPTION_H
