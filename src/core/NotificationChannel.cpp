#include "eagle/core/NotificationChannel.h"

namespace Eagle {
namespace Core {

NotificationChannel::NotificationChannel(QObject* parent)
    : QObject(parent)
    , enabled(false)
{
}

NotificationChannel::~NotificationChannel()
{
}

bool NotificationChannel::isEnabled() const
{
    return enabled;
}

void NotificationChannel::setEnabled(bool enabled)
{
    this->enabled = enabled;
}

} // namespace Core
} // namespace Eagle
