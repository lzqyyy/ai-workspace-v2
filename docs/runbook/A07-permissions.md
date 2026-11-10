# A07 最小资源权限设计 V1.0

## 1. 身份与会话

- 登录成功后签发 32 字节随机会话令牌。
- 浏览器 Cookie：`HttpOnly`、`SameSite=Lax`、`Path=/`，生产启用 `Secure`。
- 数据库只保存令牌 SHA-256 摘要和到期时间。
- 密码：PBKDF2-HMAC-SHA256，独立盐，120000 次迭代。

## 2. 资源授权矩阵

| 操作 | 判定 |
|---|---|
| 读取 Agent 列表 | `agents.owner_id = current_user_id` |
| 创建 Agent | 当前用户成为 owner；模型 Provider 必须已启用 |
| 创建会话 | Agent 必须属于当前用户且 active |
| 读取消息 | 会话必须属于当前用户 |
| 发送消息 | 会话与 Agent 均属于当前用户且 active |
| 订阅/取消/重试 Run | Run 所属会话必须属于当前用户 |
| Worker 访问 Job/Context/Event | 必须持有未过期且匹配的 lease_token |
| 访问不存在或他人资源 | 返回最小化 `403/404`，不返回资源内容 |

## 3. 审计

记录登录、登出、Agent/会话创建、发送消息、取消、重试、权限拒绝和 Worker 终态。审计字段仅包含资源 ID、动作、结果、`request_id`、错误码；禁止记录消息正文、Authorization、API Key。

## 4. 已知边界

- 本轮仅普通用户和所有者模型，不实现 Team、Project、双 Owner。
- 前端权限状态仅用于交互，任何权限判断都不以前端缓存为依据。
- 生产必须在 HTTPS 反向代理后运行。