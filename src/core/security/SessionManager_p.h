#ifndef SESSIONMANAGER_P_H
#define SESSIONMANAGER_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include "eagle/core/SessionManager.h"

namespace Eagle {
namespace Core {

class SessionManager::Private {
public:
    QMap<QString, Session> sessions;  // sessionId -> Session
    QMap<QString, QStringList> userSessions;  // userId -> sessionIds
    QTimer* cleanupTimer;
    int defaultTimeoutMinutes = 30;
    int maxSessionsPerUser = 5;
    mutable QMutex mutex;
};

} // namespace Core
} // namespace Eagle

#endif // SESSIONMANAGER_P_H
