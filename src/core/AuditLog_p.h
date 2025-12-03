#ifndef AUDITLOG_P_H
#define AUDITLOG_P_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include "eagle/core/AuditLog.h"

namespace Eagle {
namespace Core {

class AuditLogManager::Private {
public:
    QList<AuditLogEntry> entries;
    QString logFilePath;
    int maxEntries = 10000;
    bool autoFlush = true;
    mutable QMutex mutex;
    
    void writeToFile(const AuditLogEntry& entry);
};

} // namespace Core
} // namespace Eagle

#endif // AUDITLOG_P_H
