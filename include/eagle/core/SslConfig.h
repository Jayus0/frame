#ifndef EAGLE_CORE_SSLCONFIG_H
#define EAGLE_CORE_SSLCONFIG_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QObject>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

namespace Eagle {
namespace Core {

/**
 * @brief TLS协议版本
 */
enum class TlsProtocol {
    TlsV1_0,    // TLS 1.0（不推荐，仅兼容）
    TlsV1_1,    // TLS 1.1（不推荐，仅兼容）
    TlsV1_2,    // TLS 1.2（推荐）
    TlsV1_3,    // TLS 1.3（推荐，如果支持）
    TlsV1_0OrLater,  // TLS 1.0或更高
    TlsV1_2OrLater,  // TLS 1.2或更高（推荐）
    AnyProtocol      // 任何支持的协议
};

/**
 * @brief SSL/TLS配置
 */
struct SslConfig {
    bool enabled;                   // 是否启用SSL/TLS
    QString certificatePath;       // 服务器证书路径
    QString privateKeyPath;         // 私钥路径
    QString caCertificatesPath;     // CA证书路径（可选）
    QStringList cipherSuites;       // 加密套件列表（可选，空表示使用默认）
    TlsProtocol minProtocol;        // 最小TLS协议版本
    TlsProtocol maxProtocol;        // 最大TLS协议版本
    bool requireClientCert;         // 是否需要客户端证书
    bool verifyPeer;                 // 是否验证对等方证书
    
    SslConfig()
        : enabled(false)
        , minProtocol(TlsProtocol::TlsV1_2OrLater)
        , maxProtocol(TlsProtocol::AnyProtocol)
        , requireClientCert(false)
        , verifyPeer(true)
    {}
};

/**
 * @brief SSL/TLS管理器
 * 
 * 负责SSL/TLS配置、证书管理和TLS连接支持
 */
class SslManager : public QObject {
    Q_OBJECT
    
public:
    explicit SslManager(QObject* parent = nullptr);
    ~SslManager();
    
    /**
     * @brief 加载SSL配置
     */
    bool loadConfig(const SslConfig& config);
    
    /**
     * @brief 获取当前SSL配置
     */
    SslConfig getConfig() const;
    
    /**
     * @brief 配置QSslConfiguration
     */
    QSslConfiguration getSslConfiguration() const;
    
    /**
     * @brief 加载证书
     */
    bool loadCertificate(const QString& certificatePath, const QString& privateKeyPath);
    
    /**
     * @brief 加载CA证书
     */
    bool loadCaCertificates(const QString& caCertificatesPath);
    
    /**
     * @brief 添加CA证书
     */
    void addCaCertificate(const QSslCertificate& certificate);
    
    /**
     * @brief 设置加密套件
     */
    void setCipherSuites(const QStringList& cipherSuites);
    
    /**
     * @brief 设置TLS协议版本
     */
    void setProtocolVersion(TlsProtocol minProtocol, TlsProtocol maxProtocol);
    
    /**
     * @brief 启用/禁用SSL/TLS
     */
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    /**
     * @brief 验证证书
     */
    bool verifyCertificate(const QSslCertificate& certificate) const;
    
    /**
     * @brief 生成自签名证书（用于开发测试）
     */
    static bool generateSelfSignedCertificate(const QString& certificatePath, 
                                             const QString& privateKeyPath,
                                             const QString& commonName = "localhost",
                                             int validityDays = 365);
    
signals:
    void sslError(const QString& error);
    void certificateVerified(const QSslCertificate& certificate);
    
private:
    Q_DISABLE_COPY(SslManager)
    
    class Private;
    Private* d;
    
    inline Private* d_func() { return d; }
    inline const Private* d_func() const { return d; }
    
    // 辅助方法
    QSsl::SslProtocol qtProtocolFromTlsProtocol(TlsProtocol protocol) const;
    void updateSslConfiguration();
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_SSLCONFIG_H
