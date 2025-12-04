#ifndef EAGLE_CORE_WEBHOOKCHANNEL_H
#define EAGLE_CORE_WEBHOOKCHANNEL_H

#include "eagle/core/NotificationChannel.h"

namespace Eagle {
namespace Core {

/**
 * @brief Webhook通知渠道工厂
 */
class WebhookChannelFactory {
public:
    static NotificationChannel* create(QObject* parent = nullptr);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_WEBHOOKCHANNEL_H
