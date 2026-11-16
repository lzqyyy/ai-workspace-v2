# ADR-0004：Project 成员授权与 Conversation 分阶段归属

- 状态：Accepted
- 日期：2026-11-16
- 决策者：COORD/CORE
- 依据：`多人平台的最小mvp_PRD.md`（v1.0-frozen）

## 决策

- 引入 `projects`、`project_members`、`project_agents`；角色固定为 `owner / editor / viewer`。
- Conversation 访问权改为 Project membership，采用方案 A：Project 成员可见全部 Conversation，不建立 `conversation_members`。
- `conversations.owner_id` 仅表示创建者；`conversations.agent_id` 仅表示历史默认 Agent。
- 发送消息必须提供 `target_agent_id`，Run 使用该 Agent；非 Project Agent 拒绝执行。
- 一个 Conversation 同时最多一个 `queued/running` Run；该运行时不变量由工程包 B/C 的发送锁实现。
- Agent 所有权仍为 `agents.owner_id`；只有 Agent owner 可将其加入 Project，且其本人必须是 Project member。
- `client_request_id` 幂等范围为 `(conversation_id, sender_id, client_request_id)`。
- 现有 REST URL、SSE envelope、Run 状态机、API 版本和端口保持不变；新增 Project-first 路径属于兼容增量。

## 迁移策略

`006_multiplayer_foundation.sql` 是只增、可重复执行的迁移：

- 为已有 Agent/Conversation owner 创建确定性 legacy Project；
- 写入 owner 成员关系、Agent 映射和 Conversation 回填；
- 暂不强制 `conversations.project_id NOT NULL`，因为当前 C++ 创建路径尚未切换；
- `NOT NULL` 与写入切换由工程包 B/C 使用后续迁移完成。

工程包 C 独立部分已新增 `007_conversation_project_required.sql`：兼容 C++ 创建路径先确保 legacy Project/membership/Project Agent 映射并写入 `project_id`，随后将列收紧为 `NOT NULL`。Project-first 路由与 Project member 运行时权限仍等待工程包 B。

这是分阶段迁移，不是数据模型变更。

## 影响

- 工程包 B/C 必须按 Project membership 实现路由、写路径、审计与 409 保护。
- 工程包 A 不实现新路由；当前运行时仍为单 Owner 隔离。
- Worker 不新增 PostgreSQL 访问，也不获得用户权限判断职责。
