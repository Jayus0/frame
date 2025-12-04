#ifndef APIKEYMANAGER_P_H
#define APIKEYMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include "eagle/core/ApiKeyManager.h"

namespace Eagle {
namespace Core {

class ApiKeyManager::Private {
public:
    QMap<QString, ApiKey> keys;  // keyId -> ApiKey
    QMap<QString, QString> keyValueToId;  // keyValue -> keyId (用于快速查找)
    int defaultExpirationDays = 365;  // 默认1年过期
    mutable QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // APIKEYMANAGER_P_H
