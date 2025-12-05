#ifndef SSLCONFIG_P_H
#define SSLCONFIG_P_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include "eagle/core/SslConfig.h"

namespace Eagle {
namespace Core {

class SslManager::Private {
public:
    SslConfig config;
    QSslConfiguration sslConfiguration;
    QSslCertificate certificate;
    QSslKey privateKey;
    QList<QSslCertificate> caCertificates;
    mutable QMutex mutex;
    
    Private()
    {
        sslConfiguration = QSslConfiguration::defaultConfiguration();
    }
};

} // namespace Core
} // namespace Eagle

#endif // SSLCONFIG_P_H
