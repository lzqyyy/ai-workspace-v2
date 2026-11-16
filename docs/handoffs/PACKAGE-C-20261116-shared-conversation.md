# Handoff: PACKAGE-C-20261116 shared conversation and active Run switch

- 状态：REVIEW
- 主责 Agent：CORE
- 分支 / 提交：`agent/core/package-c-shared-conversation` / 本分支最终本地提交（hash 见最终报告）
- 基线：Package B `38f8afac3d10aa08dd76b868aae8210803225cc1`
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

在 Package B 的 Project membership/Agent 授权之上，完成共享 Conversation、target Agent 发送、单 active Run、Project member Run/SSE 权限、2H+2A 验证和 `conversations.project_id NOT NULL` 收尾。

## 完成内容

- 将 C 从 Package A rebase 到 Package B 最新本地提交，并解决 `core.cpp`、任务看板、ER 和数据字典冲突。
- 实现 `POST/GET /api/v1/projects/{project_id}/conversations`。
- 兼容 `POST /api/v1/conversations` 继续创建 legacy Project 并写入非空 `project_id`。
- legacy `GET /api/v1/conversations` 改为列出当前用户所有 Project membership 下的 Conversation。
- Conversation 创建、Conversation 列表、Message 读取统一改为 Project membership；viewer 只读。
- 发送消息强制 `target_agent_id`；校验 Agent 属于当前 Project、`agents.status='active'`，且 Agent owner 当前仍是 Project member。
- Run 的 `agent_id` 使用本次请求的 `target_agent_id`；Agent 回复消息保留目标 Agent 的 `sender_id`。
- 发送与 retry 在 Project 行锁后锁定 Conversation，再检查 `status IN ('queued','running')`；冲突返回 409，不依赖裸唯一约束。
- `client_request_id` 保持 `(conversation_id, sender_id, client_request_id)` sender-scoped 幂等；重复发送在 active Run 检查前返回原 Run。
- Retry Idempotency-Key 按用户判重；重复 Key 在 active Run 检查前返回原 retry Run，不同 Key 在已有 active Run 时返回 409。
- Run 取消/重试改为 Project member 语义：owner/editor 操作自己的 Run，owner 可操作任意 Run，viewer 拒绝。
- `getRun`、`listRunEvents` 和 SSE 改为 Project member 可读；非成员 403。
- 成员/Agent 移除使用 Project 行锁；发送、retry、Conversation 创建也先取得 Project 行锁，统一锁顺序，避免移除检查与新 Run 创建竞态。
- Worker `CheckPermission` 的 CONVERSATION/RUN/AGENT 授权改为 Project membership。
- 新增 `tests/shared_conversation.py`，覆盖 2H+2A、共享历史、双方 SSE、不同用户同 `client_request_id`、viewer、非成员、Run 权限和被移除 Agent owner。
- 扩展 fixture 测试，覆盖 active Run 时发送返回 409。
- OpenAPI 将 `Conversation.project_id` 收紧为 required/non-null，与 `007` 保持一致。

## 修改文件

- `api/openapi.yaml`
- `server/src/app.cpp`
- `server/src/core.cpp`
- `server/src/core.hpp`
- `db/migrations/007_conversation_project_required.sql`
- `tests/e2e.py`
- `tests/project_api.py`
- `tests/shared_conversation.py`
- `tests/validate_contracts.py`
- `scripts/e2e.ps1`
- `docs/TASK_BOARD.md`
- `docs/adr/ADR-0004-project-membership.md`
- `docs/contracts/README.md`
- `docs/runbook/A07-permissions.md`
- `docs/runbook/A13-testing.md`
- `docs/schema/data-dictionary.md`
- `docs/schema/erd.md`
- `docs/handoffs/PACKAGE-C-20261116-shared-conversation.md`

## 契约变更

- 数据库：`conversations.project_id` 由 `006` 回填并建索引，由 `007` 收紧为 `NOT NULL`。
- REST：Project-first Conversation 路由成为推荐入口；legacy Conversation URL 保留并返回 `project_id`/`owner_id`。
- 发送请求仍使用既有 URL；`target_agent_id` 成为运行时必填字段。
- OpenAPI 的 `Conversation.project_id` 从 nullable 收紧为非空 required。
- API 版本、端口、Run 状态机和 SSE envelope 不变。
- 不新增多 Run 并行、Conversation member、Redis/Kafka、新端口或 Worker PostgreSQL 访问。

## 锁顺序与并发

发送、retry、Project-first Conversation 创建、成员/Agent 移除统一采用：

1. `BEGIN`。
2. 锁定 Project 行与该调用者的 membership 行。
3. 锁定 Conversation 行。
4. 校验角色、Project/Conversation 状态、active Project Agent 和 Agent owner membership。
5. 检查 sender-scoped 幂等或 retry 幂等。
6. 检查同 Conversation active Run。
7. 写入 Message/Run/Job 或 retry Run/Job。
8. `COMMIT`。

`ponytail:` 已知简化：Project 行锁会串行化同一 Project 的发送/retry。MVP 规模可接受；吞吐不足时再收窄为 conversation/member 级锁。

## 验证命令与结果

- `git diff --check`：PASS。
- Docker Compose Core/Worker 构建与启动：PASS。
- `./scripts/test.ps1 -Docker`：PASS；契约、Go Worker test/vet、前端语法/状态检查、单用户 E2E、shared Conversation E2E、Project API fixture 全部通过。
- 契约检查：PASS；OpenAPI 25 operations、6 RPC、5 SSE events。
- 单用户 E2E：PASS；send/retry 幂等、并发 send、SSE replay、cancel/retry、重复 callback、跨用户拒绝。
- `tests/shared_conversation.py`：PASS；2H+2A、共享历史、双方 SSE、不同用户同 `client_request_id`、viewer 只读、非成员 403、Run 权限、Agent owner 移除后 403。
- active Run fixture：PASS；有 queued Run 时发送返回 409 `CONFLICT`。
- PostgreSQL 重复执行 007：PASS；执行两次后 `project_id` 仍为 `NOT NULL`、NULL 行数 0、`schema_migrations('007_project_id_required')` 计数 1。
- 现有 Docker E2E 回归：PASS。

## 验收映射

- Project-first Conversation POST/GET：PASS。
- 2H+2A：PASS。
- 双方读取完整历史和订阅 SSE：PASS。
- 不同用户选择不同 Agent：PASS。
- viewer 只读：PASS。
- 非成员 403：PASS。
- active Run 冲突 409：PASS。
- sender-scoped `client_request_id`：PASS。
- owner/editor 自己的 Run、owner 任意 Run：PASS。
- 单用户 E2E：PASS。
- 007 可重复执行与 `NOT NULL`：PASS。

## 已知问题与风险

- 前端未修改，仍使用 legacy Conversation 创建/发送参数；发送链路因 `target_agent_id` 必填会返回 400，WEB 必须接入 Project-first Conversation 和目标 Agent 选择后才能宣称浏览器多人 UI 可用。
- AGENTS.md 的“尚未实现”事实基线仍是 Package A 之前文本，需 COORD 在合并审查时更新。
- Project 行锁串行化同 Project 写路径，是可接受的 MVP 简化；高吞吐时再细化锁粒度。
- 没有新增数据库唯一约束来保证单 active Run；并发正确性依赖统一事务锁，符合用户要求避免裸约束 500。
- 本版仍不支持多 Run 并行、Conversation 级成员、归档 UI/API、消息编辑删除、文件或共享记忆。

## 需要其他 Agent 处理的事项

- WEB：切换到 `POST/GET /api/v1/projects/{project_id}/conversations`，发送消息传 `target_agent_id`，并展示 Project Agent 选择器。
- COORD：审查本分支并更新 `AGENTS.md` 当前事实基线、任务看板和最终 OpenAPI 版本说明。
- QA：合并前在完整部署中复跑 2H+2A、SSE 断线回放和迁移重复执行。

## 下一任务建议

合并 Package B/C 后创建 Project 多人前端切片：Project 列表、成员/Agent 管理、共享 Conversation、目标 Agent 选择与 Run SSE。
