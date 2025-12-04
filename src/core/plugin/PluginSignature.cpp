#include "eagle/core/PluginSignature.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtCore/QRegExp>

namespace Eagle {
namespace Core {

QStringList PluginSignatureVerifier::s_trustedRoots;

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
    QCryptographicHash::Algorithm hashAlgo = (signature.algorithm == SignatureAlgorithm::RSA_SHA512) 
                                             ? QCryptographicHash::Sha512 
                                             : QCryptographicHash::Sha256;
    QByteArray fileHash = calculateHash(pluginPath, hashAlgo);
    if (fileHash.isEmpty()) {
        Logger::error("PluginSignature", "无法计算文件哈希值");
        return false;
    }
    
    // 根据算法类型验证
    if (signature.algorithm == SignatureAlgorithm::SHA256) {
        // SHA256哈希验证（向后兼容）
        if (fileHash != signature.fileHash) {
            Logger::error("PluginSignature", "文件哈希值不匹配");
            return false;
        }
    } else if (signature.algorithm == SignatureAlgorithm::RSA_SHA256 || 
               signature.algorithm == SignatureAlgorithm::RSA_SHA512) {
        // RSA签名验证
        if (signature.certificate.publicKey.isEmpty()) {
            Logger::error("PluginSignature", "证书公钥为空");
            return false;
        }
        
        if (!verifyRSA(fileHash, signature.signature, signature.certificate.publicKey, signature.algorithm)) {
            Logger::error("PluginSignature", "RSA签名验证失败");
            return false;
        }
        
        // 验证证书链（如果提供）
        if (!signature.certificateChain.isEmpty()) {
            if (!verifyCertificateChain(signature.certificate, signature.certificateChain, s_trustedRoots)) {
                Logger::error("PluginSignature", "证书链验证失败");
                return false;
            }
        }
        
        // 检查证书有效期
        if (signature.certificate.isExpired()) {
            Logger::error("PluginSignature", "证书已过期");
            return false;
        }
        
        if (signature.certificate.isNotYetValid()) {
            Logger::error("PluginSignature", "证书尚未生效");
            return false;
        }
    } else {
        Logger::error("PluginSignature", QString("未知的签名算法: %1").arg(static_cast<int>(signature.algorithm)));
        return false;
    }
    
    // 检查签名时间是否有效（签名不能太旧）
    QDateTime now = QDateTime::currentDateTime();
    if (signature.signTime.daysTo(now) > 365) {
        Logger::warning("PluginSignature", "签名已过期（超过1年）");
        // 不强制失败，只是警告
    }
    
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
    
    // 解析算法类型
    QString algoStr = obj["algorithm"].toString("SHA256");
    if (algoStr == "RSA-SHA256") {
        signature.algorithm = SignatureAlgorithm::RSA_SHA256;
    } else if (algoStr == "RSA-SHA512") {
        signature.algorithm = SignatureAlgorithm::RSA_SHA512;
    } else {
        signature.algorithm = SignatureAlgorithm::SHA256;  // 默认
    }
    
    signature.certificatePath = obj["certificate_path"].toString();
    signature.fileHash = QByteArray::fromHex(obj["file_hash"].toString().toUtf8());
    
    // 加载证书信息
    if (!signature.certificatePath.isEmpty()) {
        signature.certificate = loadCertificate(signature.certificatePath);
    }
    
    // 加载证书链
    if (obj.contains("certificate_chain")) {
        QJsonArray chainArray = obj["certificate_chain"].toArray();
        for (const QJsonValue& val : chainArray) {
            signature.certificateChain.append(val.toString());
        }
    }
    
    return signature;
}

bool PluginSignatureVerifier::sign(const QString& pluginPath, const QString& privateKeyPath, 
                                   const QString& certificatePath, const QString& outputPath,
                                   SignatureAlgorithm algorithm)
{
    if (!QFile::exists(pluginPath)) {
        Logger::error("PluginSignature", QString("插件文件不存在: %1").arg(pluginPath));
        return false;
    }
    
    // 计算文件哈希
    QCryptographicHash::Algorithm hashAlgo = (algorithm == SignatureAlgorithm::RSA_SHA512) 
                                           ? QCryptographicHash::Sha512 
                                           : QCryptographicHash::Sha256;
    QByteArray hash = calculateHash(pluginPath, hashAlgo);
    if (hash.isEmpty()) {
        return false;
    }
    
    QByteArray signatureData;
    QString algorithmStr = "SHA256";
    
    if (algorithm == SignatureAlgorithm::SHA256) {
        // SHA256哈希（向后兼容）
        signatureData = hash;
        algorithmStr = "SHA256";
    } else if (algorithm == SignatureAlgorithm::RSA_SHA256 || 
               algorithm == SignatureAlgorithm::RSA_SHA512) {
        // RSA签名
        if (privateKeyPath.isEmpty() || !QFile::exists(privateKeyPath)) {
            Logger::error("PluginSignature", "私钥文件不存在");
            return false;
        }
        
        signatureData = signRSA(hash, privateKeyPath, algorithm);
        if (signatureData.isEmpty()) {
            Logger::error("PluginSignature", "RSA签名生成失败");
            return false;
        }
        
        algorithmStr = (algorithm == SignatureAlgorithm::RSA_SHA512) ? "RSA-SHA512" : "RSA-SHA256";
    } else {
        Logger::error("PluginSignature", "不支持的签名算法");
        return false;
    }
    
    // 加载证书信息（如果提供）
    CertificateInfo certInfo;
    if (!certificatePath.isEmpty() && QFile::exists(certificatePath)) {
        certInfo = loadCertificate(certificatePath);
    }
    
    // 创建签名信息
    QJsonObject signatureObj;
    signatureObj["signer"] = certInfo.isValid() ? certInfo.subject : "Eagle Framework";
    signatureObj["sign_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    signatureObj["signature"] = QString::fromUtf8(signatureData.toBase64());
    signatureObj["algorithm"] = algorithmStr;
    signatureObj["file_hash"] = QString::fromUtf8(hash.toHex());
    signatureObj["certificate_path"] = certificatePath;
    
    // 保存签名文件
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error("PluginSignature", QString("无法创建签名文件: %1").arg(outputPath));
        return false;
    }
    
    QJsonDocument doc(signatureObj);
    file.write(doc.toJson());
    file.close();
    
    Logger::info("PluginSignature", QString("插件签名成功: %1 (算法: %2)").arg(outputPath, algorithmStr));
    return true;
}

QByteArray PluginSignatureVerifier::calculateHash(const QString& filePath, QCryptographicHash::Algorithm algorithm)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开文件计算哈希: %1").arg(filePath));
        return QByteArray();
    }
    
    QCryptographicHash hash(algorithm);
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

QByteArray PluginSignatureVerifier::signRSA(const QByteArray& data, const QString& privateKeyPath, 
                                            SignatureAlgorithm algorithm)
{
    // 加载私钥
    QByteArray privateKey = loadPrivateKey(privateKeyPath);
    if (privateKey.isEmpty()) {
        Logger::error("PluginSignature", "无法加载私钥");
        return QByteArray();
    }
    
    // 简化实现：使用Qt的加密功能
    // 注意：这不是真正的RSA签名，而是基于哈希的简化实现
    // 生产环境应使用OpenSSL或其他加密库
    
    QCryptographicHash::Algorithm hashAlgo = (algorithm == SignatureAlgorithm::RSA_SHA512) 
                                             ? QCryptographicHash::Sha512 
                                             : QCryptographicHash::Sha256;
    QCryptographicHash hash(hashAlgo);
    hash.addData(data);
    QByteArray hashValue = hash.result();
    
    // 简化实现：使用私钥对哈希值进行"签名"（实际应使用真正的RSA算法）
    // 这里使用XOR作为占位符（生产环境必须使用真正的RSA）
    QByteArray signature;
    signature.resize(hashValue.size());
    for (int i = 0; i < hashValue.size() && i < privateKey.size(); ++i) {
        signature[i] = hashValue[i] ^ privateKey[i % privateKey.size()];
    }
    
    Logger::warning("PluginSignature", "使用简化RSA签名实现（生产环境应使用真正的RSA算法）");
    return signature;
}

bool PluginSignatureVerifier::verifyRSA(const QByteArray& data, const QByteArray& signature, 
                                        const QByteArray& publicKey, SignatureAlgorithm algorithm)
{
    if (signature.isEmpty() || publicKey.isEmpty()) {
        return false;
    }
    
    // 简化实现：使用Qt的加密功能
    // 注意：这不是真正的RSA验证，而是基于哈希的简化实现
    
    QCryptographicHash::Algorithm hashAlgo = (algorithm == SignatureAlgorithm::RSA_SHA512) 
                                             ? QCryptographicHash::Sha512 
                                             : QCryptographicHash::Sha256;
    QCryptographicHash hash(hashAlgo);
    hash.addData(data);
    QByteArray hashValue = hash.result();
    
    // 简化实现：验证签名（实际应使用真正的RSA算法）
    QByteArray expectedSignature;
    expectedSignature.resize(hashValue.size());
    for (int i = 0; i < hashValue.size() && i < publicKey.size(); ++i) {
        expectedSignature[i] = hashValue[i] ^ publicKey[i % publicKey.size()];
    }
    
    return signature == expectedSignature;
}

QByteArray PluginSignatureVerifier::loadPrivateKey(const QString& privateKeyPath)
{
    QFile file(privateKeyPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开私钥文件: %1").arg(privateKeyPath));
        return QByteArray();
    }
    
    QByteArray keyData = file.readAll();
    file.close();
    
    // 简化实现：直接读取文件内容
    // 实际应解析PEM格式的私钥
    return keyData;
}

QByteArray PluginSignatureVerifier::loadPublicKey(const QString& certificatePath)
{
    CertificateInfo cert = loadCertificate(certificatePath);
    return cert.publicKey;
}

CertificateInfo PluginSignatureVerifier::loadCertificate(const QString& certificatePath)
{
    CertificateInfo cert;
    
    if (!QFile::exists(certificatePath)) {
        Logger::warning("PluginSignature", QString("证书文件不存在: %1").arg(certificatePath));
        return cert;
    }
    
    QFile file(certificatePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开证书文件: %1").arg(certificatePath));
        return cert;
    }
    
    // 简化实现：从JSON格式的证书文件加载
    // 实际应支持PEM/DER格式的X.509证书
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        cert.subject = obj["subject"].toString();
        cert.issuer = obj["issuer"].toString();
        cert.validFrom = QDateTime::fromString(obj["valid_from"].toString(), Qt::ISODate);
        cert.validTo = QDateTime::fromString(obj["valid_to"].toString(), Qt::ISODate);
        cert.publicKey = QByteArray::fromBase64(obj["public_key"].toString().toUtf8());
        cert.serialNumber = obj["serial_number"].toString();
        
        if (obj.contains("key_usage")) {
            QJsonArray usageArray = obj["key_usage"].toArray();
            for (const QJsonValue& val : usageArray) {
                cert.keyUsage.append(val.toString());
            }
        }
    } else {
        // 如果不是JSON，尝试作为PEM格式处理（简化实现）
        QString pemData = QString::fromUtf8(data);
        // 提取基本信息（简化实现）
        QRegExp subjectRegex("Subject: (.+)");
        if (subjectRegex.indexIn(pemData) >= 0) {
            cert.subject = subjectRegex.cap(1);
        }
        cert.publicKey = data;  // 简化：使用整个文件作为公钥
    }
    
    return cert;
}

bool PluginSignatureVerifier::verifyCertificateChain(const CertificateInfo& certificate, 
                                                    const QStringList& certificateChain,
                                                    const QStringList& trustedRoots)
{
    if (certificateChain.isEmpty() && trustedRoots.isEmpty()) {
        // 如果没有证书链和受信任根，只验证证书本身
        return certificate.isValid() && !certificate.isExpired() && !certificate.isNotYetValid();
    }
    
    // 简化实现：验证证书链
    // 实际应使用X.509证书链验证
    
    // 检查证书是否有效
    if (!certificate.isValid() || certificate.isExpired() || certificate.isNotYetValid()) {
        return false;
    }
    
    // 验证证书链（简化实现）
    for (const QString& chainPath : certificateChain) {
        CertificateInfo chainCert = loadCertificate(chainPath);
        if (!chainCert.isValid()) {
            Logger::warning("PluginSignature", QString("证书链中的证书无效: %1").arg(chainPath));
            return false;
        }
    }
    
    // 检查是否信任根证书
    if (!trustedRoots.isEmpty()) {
        bool trusted = false;
        for (const QString& rootPath : trustedRoots) {
            CertificateInfo rootCert = loadCertificate(rootPath);
            if (rootCert.isValid() && rootCert.subject == certificate.issuer) {
                trusted = true;
                break;
            }
        }
        if (!trusted) {
            Logger::warning("PluginSignature", "证书链未找到受信任的根证书");
            return false;
        }
    }
    
    return true;
}

bool PluginSignatureVerifier::isRevoked(const PluginSignature& signature, const QString& crlPath)
{
    if (crlPath.isEmpty() || !QFile::exists(crlPath)) {
        return false;  // 没有撤销列表，认为未撤销
    }
    
    QList<RevokedSignature> revokedList = loadRevocationList(crlPath);
    for (const RevokedSignature& revoked : revokedList) {
        if (revoked.signer == signature.signer || 
            revoked.serialNumber == signature.certificate.serialNumber) {
            Logger::warning("PluginSignature", QString("签名已被撤销: %1").arg(revoked.reason));
            return true;
        }
    }
    
    return false;
}

QList<RevokedSignature> PluginSignatureVerifier::loadRevocationList(const QString& crlPath)
{
    QList<RevokedSignature> revokedList;
    
    if (!QFile::exists(crlPath)) {
        return revokedList;
    }
    
    QFile file(crlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("PluginSignature", QString("无法打开撤销列表文件: %1").arg(crlPath));
        return revokedList;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("revoked")) {
            QJsonArray revokedArray = obj["revoked"].toArray();
            for (const QJsonValue& val : revokedArray) {
                QJsonObject revokedObj = val.toObject();
                RevokedSignature revoked;
                revoked.signer = revokedObj["signer"].toString();
                revoked.serialNumber = revokedObj["serial_number"].toString();
                revoked.revokedTime = QDateTime::fromString(revokedObj["revoked_time"].toString(), Qt::ISODate);
                revoked.reason = revokedObj["reason"].toString();
                revokedList.append(revoked);
            }
        }
    }
    
    return revokedList;
}

void PluginSignatureVerifier::setTrustedRootCertificates(const QStringList& rootPaths)
{
    s_trustedRoots = rootPaths;
    Logger::info("PluginSignature", QString("设置受信任的根证书: %1 个").arg(rootPaths.size()));
}

QStringList PluginSignatureVerifier::getTrustedRootCertificates()
{
    return s_trustedRoots;
}

} // namespace Core
} // namespace Eagle
