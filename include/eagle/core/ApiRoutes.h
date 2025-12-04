#ifndef EAGLE_CORE_APIROUTES_H
#define EAGLE_CORE_APIROUTES_H

#include "ApiServer.h"

namespace Eagle {
namespace Core {

/**
 * @brief 注册所有API路由
 * 
 * 此函数会注册所有REST API端点，包括：
 * - 插件管理API
 * - 系统管理API
 * 
 * @param server API服务器实例
 */
void registerApiRoutes(ApiServer* server);

} // namespace Core
} // namespace Eagle

#endif // EAGLE_CORE_APIROUTES_H
