#ifndef EAGLE_CORE_EMAILCHANNEL_H
#define EAGLE_CORE_EMAILCHANNEL_H

#include "eagle/core/NotificationChannel.h"

namespace Eagle {
namespace Core {

/**
 * @brief 邮件通知渠道工厂
 */
class EmailChannelFactory {
public:
    static NotificationChannel* create(QObject* parent = nullptr);
};

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_EMAILCHANNEL_H
