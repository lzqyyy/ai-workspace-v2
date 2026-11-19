# A07 资源权限设计 V1.1（Project 目标契约）

## 1. 身份与会话

- 登录成功后签发 32 字节随机会话令牌。
- 浏览器 Cookie：`HttpOnly`、`SameSite=Lax`、`Path=/`，生产启用 `Secure`。
- 数据库只保存令牌 SHA-256 摘要和到期时间。
- 密码：PBKDF2-HMAC-SHA256，独立盐，120000 次迭代。

## 2. 目标 Project 授权矩阵

| 操作 | 目标判定 |
|---|---|
| 读取 Project/成员/Agent | 当前用户是 Project member |
| 读取 Conversation/Message/SSE | 当前用户是 Conversation 所属 Project 的 member |
| 创建 Conversation、发送消息 | member 角色为 `owner` 或 `editor` |
| 选择目标 Agent | `target_agent_id` 是当前 Project 的 active Project Agent |
| 添加自己的 Agent | member 角色为 `owner/editor`，且 `agents.owner_id = current_user_id` |
| 取消自己的 Run | member 角色为 `owner/editor`，且 `runs.initiated_by_user_id = current_user_id` |
| 取消其他成员的 Run | 仅 Project `owner` |
| 重试自己的 Run | member 角色为 `owner/editor`，且 `runs.initiated_by_user_id = current_user_id` |
| 重试其他成员的 Run | 仅 Project `owner` |
| 添加/移除成员、修改角色、归档 Project | 仅 Project `owner` |
| Worker 访问 Job/Context/Event | 必须持有未过期且匹配的 lease_token |
| 非 Project 成员访问 Project 资源 | 返回最小化 `403`，不泄露资源是否存在 |

## 3. 审计

记录登录、登出、Project/成员/Agent、Conversation、发送消息、取消、重试、权限拒绝和 Worker 终态。审计字段仅包含资源 ID、动作、结果、`request_id`、错误码；禁止记录消息正文、Authorization、API Key。

## 4. 实现状态与边界

- 工程包 A 冻结数据模型与 OpenAPI 契约；工程包 B/C 已实现 Project、ProjectMember、ProjectAgent、共享 Conversation、必填 `client_request_id`/`target_agent_id` 发送、显式 Run 发起者及 Conversation/Message/Run/SSE 的 Project 授权。
- Conversation/Message/Run/SSE 权限统一由 `conversations.project_id -> project_members` 判定；`conversations.owner_id` 仅为创建者字段。viewer 只读，owner/editor 按矩阵写操作。
- 发送与 retry 在 Project 行锁后锁定 Conversation；成员/Agent 移除同样先锁 Project，避免“移除检查完成后并发创建 Run”的竞态。
- Project 成员/Agent 关键写操作和权限拒绝写入脱敏审计，只包含资源 ID、角色、动作、结果和 `request_id`。
- Agent 所有权继续由 `agents.owner_id` 表示，不因加入 Project 转让。
- 前端权限状态仅用于交互，任何权限判断都不以前端缓存为依据。
- Project 归档语义来自最小 MVP PRD，但当前冻结 OpenAPI 没有归档 operation，运行时也没有入口；不得由客户端自行推断路由或方法。
- 生产必须在 HTTPS 反向代理后运行。
