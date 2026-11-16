# Handoff: PACKAGE-A-20261116 multiplayer foundation

- 状态：REVIEW
- 主责 Agent：CORE/COORD
- 分支 / 提交：`agent/core/mvp-foundation` / 单个本地提交（hash 见最终报告）
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

冻结最小多人多 Agent 对话平台的数据模型与 REST API 契约，不实现 Project 路由或后续业务包。

## CONTRACT-CHANGE-REQUEST

- 新增 `projects`、`project_members`、`project_agents` 及 Project-first REST 契约。
- `conversations.project_id` 采用分阶段迁移：本包只回填和建索引，暂不强制 `NOT NULL`，写入切换由工程包 B/C 完成。
- `messages.client_request_id` 唯一范围调整为 `(conversation_id, sender_id, client_request_id)`。
- 发送消息新增必填 `target_agent_id`。
- 兼容策略：保留现有 URL、API 版本和 SSE envelope；本包不实现新路由，现有单用户链路继续运行。

## 完成内容

- 新增只增迁移 `006_multiplayer_foundation.sql`：创建 Project/成员/Agent 映射，确定性生成 legacy Project，回填 owner 成员、Agent 映射和 Conversation `project_id`。
- 新增 Contract、ProjectRole、ProjectMember、ProjectAgent 与 Project-first request/response schema。
- 冻结 `target_agent_id` 必填、非成员 403、active Run/成员移除/Agent 移除/Project 归档 409 语义。
- 现有 Run/Cancel/Retry/SSE 权限描述改为 Project member 语义，URL、端口和事件格式不变。
- 更新 C++ 现有 send 写路径的 `ON CONFLICT` 目标以匹配新的 sender-scoped 幂等索引，保证单用户链路兼容。
- 更新 ER、数据字典、错误码、SSE、权限 Runbook、ADR、契约索引和任务看板。
- 扩展契约测试，静态检查 Project 路径、schema、角色、`target_agent_id`、迁移表与幂等索引范围。

## 修改文件

- `api/openapi.yaml`
- `db/migrations/006_multiplayer_foundation.sql`
- `server/src/core.cpp`
- `tests/validate_contracts.py`
- `docs/schema/erd.md`
- `docs/schema/data-dictionary.md`
- `docs/contracts/README.md`
- `docs/contracts/error-codes.md`
- `docs/contracts/sse-events.md`
- `docs/runbook/A04-A06-contract-map.md`
- `docs/runbook/A07-permissions.md`
- `docs/adr/ADR-0004-project-membership.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-A-20261116-multiplayer-foundation.md`

## 契约变更

- Project 成员可见 Project 下全部 Conversation；不建立 `conversation_members`。
- `conversations.owner_id` 仅为创建者；`conversations.agent_id` 仅为历史默认 Agent。
- 消息发送必须提供 `target_agent_id`，Run 使用该 Agent。
- `client_request_id` 按 Conversation + sender 判重。
- 一个 Conversation 同时最多一个 queued/running Run；由工程包 B/C 的事务发送锁实现并返回 409。
- `project_id` 本包保持可空；B/C 切换写入后再单独收紧为 `NOT NULL`。
- API 主版本、路径、端口、SSE envelope 和 Run 状态机不变。

## 验证命令与结果

- `git diff --check`：PASS。
- `docker compose --env-file .env -f deploy/docker-compose.yml config -q`：PASS。
- `python tests/validate_contracts.py` / Docker contract check：PASS；OpenAPI 25 operations、6 RPC、5 SSE events。
- Protobuf descriptor compile（Docker contract-tools）：PASS。
- Go Worker 单元测试：PASS。
- 隔离 PostgreSQL 16 容器：`001`—`006` 连续执行两遍 PASS；第二遍后 `schema_migrations=6`、Projects=2、Members=2、ProjectAgents=2、回填 Conversations=2；不同 sender 同 `client_request_id` 两条消息可共存，同 sender 重复被拒绝；`conversations.project_id` 仍可空。
- 现有数据库 Core 重启重复执行迁移：PASS；migration rows/distinct versions 均为 6，health 为 `ok`。
- `./scripts/test.ps1 -Docker`：PASS；现有单用户 E2E 的 validation、并发 send 幂等、duplicate callback、SSE replay、跨用户拒绝、retry 幂等、cancel/retry 全部 PASS。

## 验收映射

- 数据模型冻结：PASS（迁移与 schema 文档）。
- API 契约冻结：PASS（OpenAPI 与契约测试）。
- 不实现后续业务：PASS（无 Project 路由 handler）。
- 现有单用户回归：PASS（完整 Docker E2E）。
- 非成员 403 / 409 场景：OpenAPI 与错误码文档已冻结，运行时验收留给 B/C。

## 已知问题与风险

- Project-first 路由尚未实现，当前运行时仍是单 Owner 资源隔离。
- `conversations.project_id` 仍可空，旧 C++ 创建会话路径尚未切换。
- 一个 Conversation 单 active Run 仍由 B/C 发送锁实现；本包未增加会令旧路径返回 500 的数据库唯一约束。
- 成员/Agent 业务校验、Project 写权限、SSE Project 授权和审计动作仍待 B/C。
- 现有 `POST /api/v1/conversations` 保留为兼容入口；Project-first 客户端应使用新路径。

## 需要其他 Agent 处理的事项

- B/C 实现 Project 成员/Agent/Conversation 路由与 owner/editor/viewer 授权，非成员统一 403。
- B/C 切换 Conversation 写入 Project，并在后续迁移中将 `project_id` 收紧为 `NOT NULL`。
- B/C 在发送/重试事务中锁定 Conversation，检查 active Run，并返回 409。
- B/C 写 `target_agent_id` 校验、Project Agent active 校验和成员/Agent 移除保护。
- WEB 使用 Project-first 接口并把 `target_agent_id` 作为发送参数；此前不得宣称多人功能可用。

## 下一任务建议

按工程包 B 实现 Project/ProjectMember/ProjectAgent 的 C++ 路由和授权，再进入工程包 C 的共享 Conversation + 单 active Run 发送切换。
