#ifndef ALERTSYSTEM_P_H
#define ALERTSYSTEM_P_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <functional>
#include "eagle/core/AlertSystem.h"
#include "eagle/core/PerformanceMonitor.h"
#include "eagle/core/NotificationChannel.h"

namespace Eagle {
namespace Core {

class AlertSystem::Private {
public:
    QMap<QString, AlertRule> rules;
    QMap<QString, AlertRecord> activeAlerts;  // alertId -> AlertRecord
    QList<AlertRecord> alertHistory;  // 历史告警
    PerformanceMonitor* monitor;
    bool enabled = true;
    mutable QMutex mutex;
    
    // 告警处理器
    std::function<void(const AlertRecord&)> alertHandler;
    
    // 规则触发时间记录（用于持续时间检查）
    QMap<QString, QDateTime> ruleTriggerTimes;  // ruleId -> triggerTime
    
    // 通知渠道
    QMap<QString, NotificationChannel*> notificationChannels;  // channelName -> channel
    bool notificationEnabled = true;
    QList<AlertLevel> notificationLevels;  // 需要通知的告警级别
};

} // namespace Core
} // namespace Eagle

#endif // ALERTSYSTEM_P_H
