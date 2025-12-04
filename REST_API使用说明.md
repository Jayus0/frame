# REST API 使用说明

## 概述

Eagle框架提供了完整的REST API接口，支持远程管理插件、查询系统状态、查看日志等功能。所有API都集成了认证、权限检查、限流和审计日志功能。

## 配置

在框架配置文件中启用API服务器：

```json
{
  "framework": {
    "api": {
      "enabled": true,
      "port": 8080
    }
  }
}
```

## 认证方式

API支持两种认证方式：

### 1. API密钥认证

在请求头中添加：
```
Authorization: ApiKey <your-api-key>
```

或在查询参数中添加：
```
?api_key=<your-api-key>
```

### 2. 会话认证

在请求头中添加：
```
Authorization: Bearer <session-id>
```

## API端点

### 插件管理

#### 获取插件列表
```http
GET /api/v1/plugins
Authorization: ApiKey <your-api-key>
```

**响应示例：**
```json
{
  "success": true,
  "data": {
    "plugins": [
      {
        "id": "com.eagle.sample",
        "name": "示例插件",
        "version": "1.0.0",
        "author": "开发团队",
        "description": "示例插件描述",
        "loaded": true
      }
    ],
    "total": 1
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `plugin.read`

#### 获取插件详情
```http
GET /api/v1/plugins/{id}
Authorization: ApiKey <your-api-key>
```

**路径参数：**
- `id`: 插件ID

**响应示例：**
```json
{
  "success": true,
  "data": {
    "id": "com.eagle.sample",
    "name": "示例插件",
    "version": "1.0.0",
    "author": "开发团队",
    "description": "示例插件描述",
    "loaded": true,
    "dependencies": ["com.eagle.core"]
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `plugin.read`

#### 加载插件
```http
POST /api/v1/plugins/{id}/load
Authorization: ApiKey <your-api-key>
```

**路径参数：**
- `id`: 插件ID

**响应示例：**
```json
{
  "success": true,
  "data": {
    "pluginId": "com.eagle.sample",
    "status": "loaded"
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `plugin.load`

#### 卸载插件
```http
DELETE /api/v1/plugins/{id}
Authorization: ApiKey <your-api-key>
```

**路径参数：**
- `id`: 插件ID

**响应示例：**
```json
{
  "success": true,
  "data": {
    "pluginId": "com.eagle.sample",
    "status": "unloaded"
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `plugin.unload`

### 系统管理

#### 健康检查
```http
GET /api/v1/health
```

**说明：** 此端点不需要认证

**响应示例：**
```json
{
  "success": true,
  "data": {
    "status": "healthy",
    "timestamp": "2024-01-15T10:30:00Z",
    "system": {
      "cpuUsage": 25.5,
      "memoryUsageMB": 512
    },
    "plugins": {
      "total": 10,
      "loaded": 5
    }
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

#### 获取监控指标
```http
GET /api/v1/metrics
Authorization: ApiKey <your-api-key>
```

**响应示例：**
```json
{
  "success": true,
  "data": {
    "system": {
      "cpuUsage": 25.5,
      "memoryUsageMB": 512
    },
    "plugins": [
      {
        "id": "com.eagle.sample",
        "loadTime": 150
      }
    ]
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

#### 查询日志
```http
GET /api/v1/logs?userId=user1&operation=plugin.load&limit=100
Authorization: ApiKey <your-api-key>
```

**查询参数：**
- `userId` (可选): 用户ID
- `operation` (可选): 操作名称
- `limit` (可选): 返回数量限制，默认100

**响应示例：**
```json
{
  "success": true,
  "data": {
    "logs": [
      {
        "userId": "user1",
        "operation": "POST /api/v1/plugins/{id}/load",
        "resource": "com.eagle.sample",
        "level": 1,
        "success": true,
        "timestamp": "2024-01-15T10:30:00Z"
      }
    ],
    "total": 1
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `log.read`

#### 更新配置
```http
POST /api/v1/config
Authorization: ApiKey <your-api-key>
Content-Type: application/json

{
  "framework.plugins.scan_paths": ["/path/to/plugins"],
  "framework.logging.level": "debug"
}
```

**响应示例：**
```json
{
  "success": true,
  "data": {
    "message": "Configuration updated successfully"
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

**所需权限：** `config.write`

## 错误响应

所有错误响应都遵循以下格式：

```json
{
  "error": true,
  "code": 401,
  "message": "Unauthorized",
  "details": "Missing authentication token",
  "timestamp": "2024-01-15T10:30:00Z"
}
```

### 常见错误码

- `400 Bad Request`: 请求参数错误
- `401 Unauthorized`: 认证失败
- `403 Forbidden`: 权限不足
- `404 Not Found`: 资源不存在
- `429 Too Many Requests`: 请求频率超限
- `500 Internal Server Error`: 服务器内部错误

## 限流

默认限流规则：
- 每个IP地址或用户每分钟最多100次请求
- 超过限制返回429错误

可以通过配置修改限流规则。

## 审计日志

所有API调用都会自动记录到审计日志中，包括：
- 用户ID
- 操作类型
- 资源
- 操作结果
- 时间戳

## 使用示例

### cURL示例

```bash
# 获取插件列表
curl -H "Authorization: ApiKey your-api-key" \
     http://localhost:8080/api/v1/plugins

# 加载插件
curl -X POST \
     -H "Authorization: ApiKey your-api-key" \
     http://localhost:8080/api/v1/plugins/com.eagle.sample/load

# 健康检查（无需认证）
curl http://localhost:8080/api/v1/health
```

### Python示例

```python
import requests

# 配置
API_BASE = "http://localhost:8080"
API_KEY = "your-api-key"
headers = {"Authorization": f"ApiKey {API_KEY}"}

# 获取插件列表
response = requests.get(f"{API_BASE}/api/v1/plugins", headers=headers)
plugins = response.json()["data"]["plugins"]

# 加载插件
plugin_id = "com.eagle.sample"
response = requests.post(
    f"{API_BASE}/api/v1/plugins/{plugin_id}/load",
    headers=headers
)
print(response.json())
```

### JavaScript示例

```javascript
const API_BASE = 'http://localhost:8080';
const API_KEY = 'your-api-key';

// 获取插件列表
fetch(`${API_BASE}/api/v1/plugins`, {
  headers: {
    'Authorization': `ApiKey ${API_KEY}`
  }
})
.then(response => response.json())
.then(data => console.log(data));
```

## 安全建议

1. **使用HTTPS**: 生产环境应启用HTTPS（需要额外配置SSL/TLS）
2. **保护API密钥**: 不要将API密钥提交到版本控制系统
3. **定期轮换密钥**: 定期更换API密钥
4. **最小权限原则**: 只授予必要的权限
5. **监控异常请求**: 定期检查审计日志，发现异常行为

## 注意事项

1. API服务器默认未启用，需要在配置文件中设置 `framework.api.enabled = true`
2. 默认端口为8080，可通过配置修改
3. 所有需要认证的端点都需要有效的API密钥或会话
4. 权限检查基于RBAC系统，确保用户具有相应权限
5. 限流基于IP地址或用户ID，防止滥用
