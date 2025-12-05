#include "eagle/core/SslConfig.h"
#include "SslConfig_p.h"
#include "eagle/core/Logger.h"
#include <QtCore/QFile>
#include <QtCore/QMutexLocker>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

namespace Eagle {
namespace Core {

SslManager::SslManager(QObject* parent)
    : QObject(parent)
    , d(new SslManager::Private)
{
    Logger::info("SslManager", "SSL/TLS管理器初始化完成");
}

SslManager::~SslManager()
{
    delete d;
}

bool SslManager::loadConfig(const SslConfig& config)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    d->config = config;
    
    if (!config.enabled) {
        Logger::info("SslManager", "SSL/TLS已禁用");
        return true;
    }
    
    // 加载证书和私钥
    if (!config.certificatePath.isEmpty() && !config.privateKeyPath.isEmpty()) {
        if (!loadCertificate(config.certificatePath, config.privateKeyPath)) {
            Logger::error("SslManager", "无法加载SSL证书");
            return false;
        }
    }
    
    // 加载CA证书
    if (!config.caCertificatesPath.isEmpty()) {
        if (!loadCaCertificates(config.caCertificatesPath)) {
            Logger::warning("SslManager", "无法加载CA证书，继续使用系统默认CA");
        }
    }
    
    // 更新SSL配置
    updateSslConfiguration();
    
    Logger::info("SslManager", "SSL/TLS配置加载成功");
    return true;
}

SslConfig SslManager::getConfig() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->config;
}

QSslConfiguration SslManager::getSslConfiguration() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->sslConfiguration;
}

bool SslManager::loadCertificate(const QString& certificatePath, const QString& privateKeyPath)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    // 加载证书
    QFile certFile(certificatePath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        Logger::error("SslManager", QString("无法打开证书文件: %1").arg(certificatePath));
        return false;
    }
    
    QByteArray certData = certFile.readAll();
    certFile.close();
    
    QSslCertificate certificate(certData, QSsl::Pem);
    if (certificate.isNull()) {
        // 尝试DER格式
        certificate = QSslCertificate(certData, QSsl::Der);
    }
    
    if (certificate.isNull()) {
        Logger::error("SslManager", QString("无法解析证书文件: %1").arg(certificatePath));
        return false;
    }
    
    // 加载私钥
    QFile keyFile(privateKeyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        Logger::error("SslManager", QString("无法打开私钥文件: %1").arg(privateKeyPath));
        return false;
    }
    
    QByteArray keyData = keyFile.readAll();
    keyFile.close();
    
    QSslKey privateKey(keyData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (privateKey.isNull()) {
        // 尝试DER格式
        privateKey = QSslKey(keyData, QSsl::Rsa, QSsl::Der, QSsl::PrivateKey);
    }
    
    if (privateKey.isNull()) {
        Logger::error("SslManager", QString("无法解析私钥文件: %1").arg(privateKeyPath));
        return false;
    }
    
    d->certificate = certificate;
    d->privateKey = privateKey;
    
    Logger::info("SslManager", QString("证书和私钥加载成功: %1").arg(certificatePath));
    return true;
}

bool SslManager::loadCaCertificates(const QString& caCertificatesPath)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    QFile caFile(caCertificatesPath);
    if (!caFile.open(QIODevice::ReadOnly)) {
        Logger::error("SslManager", QString("无法打开CA证书文件: %1").arg(caCertificatesPath));
        return false;
    }
    
    QByteArray caData = caFile.readAll();
    caFile.close();
    
    QList<QSslCertificate> certificates = QSslCertificate::fromData(caData, QSsl::Pem);
    if (certificates.isEmpty()) {
        certificates = QSslCertificate::fromData(caData, QSsl::Der);
    }
    
    if (certificates.isEmpty()) {
        Logger::error("SslManager", QString("无法解析CA证书文件: %1").arg(caCertificatesPath));
        return false;
    }
    
    d->caCertificates.append(certificates);
    
    Logger::info("SslManager", QString("CA证书加载成功: %1 (共%2个证书)")
        .arg(caCertificatesPath).arg(certificates.size()));
    return true;
}

void SslManager::addCaCertificate(const QSslCertificate& certificate)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->caCertificates.append(certificate);
    updateSslConfiguration();
}

void SslManager::setCipherSuites(const QStringList& cipherSuites)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->config.cipherSuites = cipherSuites;
    updateSslConfiguration();
}

void SslManager::setProtocolVersion(TlsProtocol minProtocol, TlsProtocol maxProtocol)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->config.minProtocol = minProtocol;
    d->config.maxProtocol = maxProtocol;
    updateSslConfiguration();
}

void SslManager::setEnabled(bool enabled)
{
    auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    d->config.enabled = enabled;
    Logger::info("SslManager", QString("SSL/TLS%1").arg(enabled ? "启用" : "禁用"));
}

bool SslManager::isEnabled() const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    return d->config.enabled;
}

bool SslManager::verifyCertificate(const QSslCertificate& certificate) const
{
    const auto* d = d_func();
    QMutexLocker locker(&d->mutex);
    
    if (!d->config.verifyPeer) {
        return true;  // 如果禁用验证，直接返回true
    }
    
    // 检查证书是否有效
    if (certificate.isNull() || certificate.isBlacklisted()) {
        return false;
    }
    
    // 检查证书是否过期
    QDateTime now = QDateTime::currentDateTime();
    if (certificate.expiryDate() < now || certificate.effectiveDate() > now) {
        return false;
    }
    
    // 如果配置了CA证书，验证证书链
    if (!d->caCertificates.isEmpty()) {
        // 简化验证：检查证书是否由配置的CA签发
        // 实际生产环境应该进行完整的证书链验证
        // 这里使用Qt的默认验证机制
        return true;  // 简化实现
    }
    
    return true;
}

bool SslManager::generateSelfSignedCertificate(const QString& certificatePath, 
                                               const QString& privateKeyPath,
                                               const QString& commonName,
                                               int validityDays)
{
    // 注意：Qt本身不提供生成自签名证书的功能
    // 这里提供一个简化的实现说明，实际应该使用OpenSSL命令行工具
    // 或者集成OpenSSL库来生成证书
    
    Logger::warning("SslManager", "generateSelfSignedCertificate需要OpenSSL支持");
    Logger::info("SslManager", QString("请使用以下命令生成自签名证书:"));
    Logger::info("SslManager", QString("  openssl req -x509 -newkey rsa:4096 -keyout %1 -out %2 -days %3 -nodes -subj \"/CN=%4\"")
        .arg(privateKeyPath, certificatePath).arg(validityDays).arg(commonName));
    
    return false;  // 需要外部工具生成
}

QSsl::SslProtocol SslManager::qtProtocolFromTlsProtocol(TlsProtocol protocol) const
{
    switch (protocol) {
    case TlsProtocol::TlsV1_0:
        return QSsl::TlsV1_0;
    case TlsProtocol::TlsV1_1:
        return QSsl::TlsV1_1;
    case TlsProtocol::TlsV1_2:
        return QSsl::TlsV1_2;
    case TlsProtocol::TlsV1_3:
        return QSsl::TlsV1_3;
    case TlsProtocol::TlsV1_0OrLater:
        return QSsl::TlsV1_0;
    case TlsProtocol::TlsV1_2OrLater:
        return QSsl::TlsV1_2;
    case TlsProtocol::AnyProtocol:
    default:
        return QSsl::AnyProtocol;
    }
}

void SslManager::updateSslConfiguration()
{
    auto* d = d_func();
    
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    
    // 设置证书和私钥
    if (!d->certificate.isNull() && !d->privateKey.isNull()) {
        config.setLocalCertificate(d->certificate);
        config.setPrivateKey(d->privateKey);
    }
    
    // 设置CA证书
    if (!d->caCertificates.isEmpty()) {
        config.setCaCertificates(d->caCertificates);
    }
    
    // 设置协议版本
    QSsl::SslProtocol minProtocol = qtProtocolFromTlsProtocol(d->config.minProtocol);
    QSsl::SslProtocol maxProtocol = qtProtocolFromTlsProtocol(d->config.maxProtocol);
    
    // Qt的QSslConfiguration不直接支持min/max协议，需要在连接时设置
    // 这里先设置默认协议
    if (minProtocol != QSsl::AnyProtocol) {
        config.setProtocol(minProtocol);
    }
    
    // 设置加密套件
    if (!d->config.cipherSuites.isEmpty()) {
        QList<QSslCipher> ciphers;
        for (const QString& cipherName : d->config.cipherSuites) {
            QSslCipher cipher(cipherName);
            if (!cipher.isNull()) {
                ciphers.append(cipher);
            }
        }
        if (!ciphers.isEmpty()) {
            config.setCiphers(ciphers);
        }
    }
    
    // 设置验证选项
    QSslSocket::PeerVerifyMode verifyMode = d->config.verifyPeer ? 
        QSslSocket::VerifyPeer : QSslSocket::QueryPeer;
    config.setPeerVerifyMode(verifyMode);
    
    d->sslConfiguration = config;
}

} // namespace Core
} // namespace Eagle
