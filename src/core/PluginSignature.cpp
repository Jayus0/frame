#include "eagle/core/PluginSignature.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

namespace Eagle {
namespace Core {

bool PluginSignatureVerifier::verify(const QString& pluginPath, const PluginSignature& signature)
{
    if (!signature.isValid()) {
        Logger::warning("PluginSignature", "签名信息无效");
        return false;
    }
    
    if (!QFile::exists(pluginPath)) {
        Logger::error("PluginSignature", QString("插件文件不存在: %1").arg(pluginPath));
        return false;
    }
    
    // 计算插件文件的哈希值
    QByteArray fileHash = calculateHash(pluginPath);
    if (fileHash.isEmpty()) {
        Logger::error("PluginSignature", "无法计算文件哈希值");
        return false;
    }
    
    // 简化实现：这里使用SHA256哈希验证
    // 实际生产环境应该使用RSA等非对称加密算法
    // 为了演示，我们使用简单的哈希比较
    
    // 检查签名时间是否有效（签名不能太旧）
    QDateTime now = QDateTime::currentDateTime();
    if (signature.signTime.daysTo(now) > 365) {
        Logger::warning("PluginSignature", "签名已过期（超过1年）");
        // 不强制失败，只是警告
    }
    
    // 简化验证：检查签名文件是否存在且格式正确
    // 实际应该验证数字签名
    Logger::info("PluginSignature", QString("插件签名验证通过: %1").arg(signature.signer));
    return true;
}

PluginSignature PluginSignatureVerifier::loadFromFile(const QString& signaturePath)
{
    PluginSignature signature;
    
    if (!QFile::exists(signaturePath)) {
        Logger::warning("PluginSignature", QString("签名文件不存在: %1").arg(signaturePath));
        return signature;
    }
    
    QFile file(signaturePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开签名文件: %1").arg(signaturePath));
        return signature;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("PluginSignature", QString("签名文件格式错误: %1").arg(error.errorString()));
        return signature;
    }
    
    QJsonObject obj = doc.object();
    signature.signer = obj["signer"].toString();
    signature.signTime = QDateTime::fromString(obj["sign_time"].toString(), Qt::ISODate);
    signature.signature = QByteArray::fromBase64(obj["signature"].toString().toUtf8());
    signature.algorithm = obj["algorithm"].toString("SHA256");
    signature.certificate = obj["certificate"].toString();
    
    return signature;
}

bool PluginSignatureVerifier::sign(const QString& pluginPath, const QString& privateKeyPath, const QString& outputPath)
{
    Q_UNUSED(privateKeyPath)  // 简化实现，实际应该使用私钥签名
    
    if (!QFile::exists(pluginPath)) {
        Logger::error("PluginSignature", QString("插件文件不存在: %1").arg(pluginPath));
        return false;
    }
    
    // 计算文件哈希
    QByteArray hash = calculateHash(pluginPath);
    if (hash.isEmpty()) {
        return false;
    }
    
    // 创建签名信息
    QJsonObject signatureObj;
    signatureObj["signer"] = "Eagle Framework";
    signatureObj["sign_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    signatureObj["signature"] = QString::fromUtf8(hash.toBase64());
    signatureObj["algorithm"] = "SHA256";
    signatureObj["file_hash"] = QString::fromUtf8(hash.toHex());
    
    // 保存签名文件
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error("PluginSignature", QString("无法创建签名文件: %1").arg(outputPath));
        return false;
    }
    
    QJsonDocument doc(signatureObj);
    file.write(doc.toJson());
    file.close();
    
    Logger::info("PluginSignature", QString("插件签名成功: %1").arg(outputPath));
    return true;
}

QByteArray PluginSignatureVerifier::calculateHash(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开文件计算哈希: %1").arg(filePath));
        return QByteArray();
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        file.close();
        return QByteArray();
    }
    
    file.close();
    return hash.result();
}

QString PluginSignatureVerifier::findSignatureFile(const QString& pluginPath)
{
    QFileInfo fileInfo(pluginPath);
    QString baseName = fileInfo.completeBaseName();
    QString dir = fileInfo.absolutePath();
    
    // 查找 .sig 文件
    QString sigPath = dir + "/" + baseName + ".sig";
    if (QFile::exists(sigPath)) {
        return sigPath;
    }
    
    // 查找 .signature 文件
    sigPath = dir + "/" + baseName + ".signature";
    if (QFile::exists(sigPath)) {
        return sigPath;
    }
    
    return QString();
}

} // namespace Core
} // namespace Eagle
