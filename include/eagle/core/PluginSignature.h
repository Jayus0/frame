#ifndef EAGLE_CORE_PLUGINSIGNATURE_H
#define EAGLE_CORE_PLUGINSIGNATURE_H

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

/**
 * @brief 插件签名信息
 */
struct PluginSignature {
    QString signer;           // 签名者
    QDateTime signTime;       // 签名时间
    QByteArray signature;      // 签名数据
    QString algorithm;        // 签名算法（如：RSA-SHA256）
    QString certificate;      // 证书信息
    
    bool isValid() const {
        return !signature.isEmpty() && !signer.isEmpty();
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
     * @param outputPath 输出签名文件路径
     * @return 是否成功
     */
    static bool sign(const QString& pluginPath, const QString& privateKeyPath, const QString& outputPath);
    
    /**
     * @brief 计算文件哈希值
     * @param filePath 文件路径
     * @return 哈希值
     */
    static QByteArray calculateHash(const QString& filePath);
    
    /**
     * @brief 检查插件是否有签名文件
     * @param pluginPath 插件文件路径
     * @return 签名文件路径，如果不存在返回空字符串
     */
    static QString findSignatureFile(const QString& pluginPath);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_PLUGINSIGNATURE_H
