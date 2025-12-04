#ifndef EAGLE_CORE_PLUGINSIGNATURE_H
#define EAGLE_CORE_PLUGINSIGNATURE_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QCryptographicHash>

namespace Eagle {
namespace Core {

/**
 * @brief 签名算法类型
 */
enum class SignatureAlgorithm {
    SHA256,      // SHA256哈希（旧版本，向后兼容）
    RSA_SHA256,  // RSA-SHA256签名
    RSA_SHA512   // RSA-SHA512签名
};

/**
 * @brief 证书信息
 */
struct CertificateInfo {
    QString subject;          // 证书主题
    QString issuer;           // 证书颁发者
    QDateTime validFrom;      // 有效期开始
    QDateTime validTo;        // 有效期结束
    QByteArray publicKey;     // 公钥数据
    QString serialNumber;     // 序列号
    QStringList keyUsage;     // 密钥用途
    
    bool isValid() const {
        return !subject.isEmpty() && !publicKey.isEmpty();
    }
    
    bool isExpired() const {
        return QDateTime::currentDateTime() > validTo;
    }
    
    bool isNotYetValid() const {
        return QDateTime::currentDateTime() < validFrom;
    }
};

/**
 * @brief 插件签名信息
 */
struct PluginSignature {
    QString signer;              // 签名者
    QDateTime signTime;           // 签名时间
    QByteArray signature;         // 签名数据
    SignatureAlgorithm algorithm; // 签名算法
    QString certificatePath;     // 证书路径
    CertificateInfo certificate;  // 证书信息
    QStringList certificateChain; // 证书链路径
    QByteArray fileHash;          // 文件哈希值（用于验证）
    
    bool isValid() const {
        return !signature.isEmpty() && !signer.isEmpty();
    }
};

/**
 * @brief 签名撤销列表项
 */
struct RevokedSignature {
    QString signer;           // 签名者
    QString serialNumber;      // 证书序列号
    QDateTime revokedTime;    // 撤销时间
    QString reason;           // 撤销原因
    
    RevokedSignature()
    {
        revokedTime = QDateTime::currentDateTime();
    }
};

/**
 * @brief 插件签名验证器
 */
class PluginSignatureVerifier {
public:
    /**
     * @brief 验证插件签名
     * @param pluginPath 插件文件路径
     * @param signature 签名信息
     * @return 验证是否通过
     */
    static bool verify(const QString& pluginPath, const PluginSignature& signature);
    
    /**
     * @brief 从文件加载签名信息
     * @param signaturePath 签名文件路径（通常是 .sig 文件）
     * @return 签名信息，如果加载失败返回无效签名
     */
    static PluginSignature loadFromFile(const QString& signaturePath);
    
    /**
     * @brief 生成插件签名（用于开发阶段）
     * @param pluginPath 插件文件路径
     * @param privateKeyPath 私钥路径
     * @param certificatePath 证书路径
     * @param outputPath 输出签名文件路径
     * @param algorithm 签名算法（默认RSA_SHA256）
     * @return 是否成功
     */
    static bool sign(const QString& pluginPath, const QString& privateKeyPath, 
                    const QString& certificatePath, const QString& outputPath,
                    SignatureAlgorithm algorithm = SignatureAlgorithm::RSA_SHA256);
    
    /**
     * @brief 计算文件哈希值
     * @param filePath 文件路径
     * @param algorithm 哈希算法（SHA256或SHA512）
     * @return 哈希值
     */
    static QByteArray calculateHash(const QString& filePath, QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha256);
    
    /**
     * @brief 检查插件是否有签名文件
     * @param pluginPath 插件文件路径
     * @return 签名文件路径，如果不存在返回空字符串
     */
    static QString findSignatureFile(const QString& pluginPath);
    
    /**
     * @brief 验证证书链
     * @param certificate 证书信息
     * @param certificateChain 证书链路径列表
     * @param trustedRoots 受信任的根证书路径列表
     * @return 验证是否通过
     */
    static bool verifyCertificateChain(const CertificateInfo& certificate, 
                                      const QStringList& certificateChain,
                                      const QStringList& trustedRoots = QStringList());
    
    /**
     * @brief 检查签名是否在撤销列表中
     * @param signature 签名信息
     * @param crlPath 撤销列表文件路径
     * @return 是否被撤销
     */
    static bool isRevoked(const PluginSignature& signature, const QString& crlPath);
    
    /**
     * @brief 从文件加载证书信息
     * @param certificatePath 证书文件路径
     * @return 证书信息
     */
    static CertificateInfo loadCertificate(const QString& certificatePath);
    
    /**
     * @brief 从文件加载撤销列表
     * @param crlPath 撤销列表文件路径
     * @return 撤销列表
     */
    static QList<RevokedSignature> loadRevocationList(const QString& crlPath);
    
    /**
     * @brief 设置受信任的根证书路径
     * @param rootPaths 根证书路径列表
     */
    static void setTrustedRootCertificates(const QStringList& rootPaths);
    
    /**
     * @brief 获取受信任的根证书路径
     * @return 根证书路径列表
     */
    static QStringList getTrustedRootCertificates();
    
private:
    static QStringList s_trustedRoots;  // 受信任的根证书路径
    
    // RSA签名/验证（使用Qt实现或OpenSSL）
    static QByteArray signRSA(const QByteArray& data, const QString& privateKeyPath, 
                             SignatureAlgorithm algorithm);
    static bool verifyRSA(const QByteArray& data, const QByteArray& signature, 
                         const QByteArray& publicKey, SignatureAlgorithm algorithm);
    static QByteArray loadPrivateKey(const QString& privateKeyPath);
    static QByteArray loadPublicKey(const QString& certificatePath);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_PLUGINSIGNATURE_H
