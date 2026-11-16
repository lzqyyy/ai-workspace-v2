# Handoff: PACKAGE-C-20261116 shared conversation and active Run switch

- 状态：BLOCKED
- 主责 Agent：CORE
- 分支 / 提交：`agent/core/package-c-shared-conversation` / 本提交（本地）
- 开始时间：2026-11-16
- 完成时间：2026-11-16（独立部分）

## 目标

实现 Project-first 共享 Conversation、多 Agent 目标选择、单 active Run、Project member 权限，并在兼容 Conversation 写入切到 `project_id` 后用只增迁移收紧 `NOT NULL`。

## 依赖检查

- Package A：本地 `agent/core/mvp-foundation` / `1f0ead3fbe1a9c7f5d818372667f73000f9aba18`，已完成。
- Package B：本地、reflog、stash 与 `origin` 均无分支、提交或交接；确认缺失。
- 因此本包不实现或重写 Project、ProjectMember、ProjectAgent 路由，也不猜测 B 的授权 helper。
- 本 worktree 从 Package A 建立；B 合入后需 rebase 到 B 最新本地提交再继续依赖部分。

## 完成内容

- `POST /api/v1/conversations` 兼容路径现在先确保确定性 legacy Project、owner membership 和 Project Agent 映射存在，再写入 `conversations.project_id`。
- Conversation 响应和 legacy `GET /api/v1/conversations` 返回 `project_id`。
- 新增只增、可重复执行迁移 `007_conversation_project_required.sql`，在 006 回填后把 `conversations.project_id` 收紧为 `NOT NULL`。
- 契约检查验证 007 存在并包含 `SET NOT NULL`，同时继续验证 006 不提前收紧。
- 单用户 E2E 增加 Conversation 创建与列表的 `project_id` 非空断言。

## Project-first 接口切换设计（等待 B）

### 路由

- 新增 `POST /api/v1/projects/{project_id}/conversations`：owner/editor 创建共享 Conversation。
- 新增 `GET /api/v1/projects/{project_id}/conversations`：任意 Project member 读取 Project 下全部 Conversation。
- 保留 legacy `/api/v1/conversations` 与 `/api/v1/conversations/{id}/messages` URL。

### 授权查询

- Conversation 访问统一由 `conversations.project_id -> project_members` 决定。
- Message 读取要求当前用户是 Project member，不再使用 `conversations.owner_id`。
- 发送要求角色为 owner/editor；viewer 只读。
- `target_agent_id` 必须同时满足：`project_id` 匹配、`project_agents` 存在、`agents.status='active'`。
- 非成员或 Agent 不属于 Project 统一返回 403，不泄露资源细节。

### 发送事务与单 active Run

固定锁顺序，避免死锁：

1. `BEGIN`。
2. `SELECT project_id,status FROM conversations WHERE id=$1 FOR UPDATE`。
3. 校验 Project membership、角色、Project 状态和 active Project Agent。
4. 在同事务中按 `(conversation_id, sender_id, client_request_id)` 查询重复消息；命中则返回原 Run，不要求当前无 active Run。
5. `SELECT 1 FROM runs WHERE conversation_id=$1 AND status IN ('queued','running') LIMIT 1`；存在则返回 409 `CONFLICT`，不依赖裸唯一约束。
6. 创建 user Message、Run（`runs.agent_id=target_agent_id`）、Job，并更新 Conversation。
7. `COMMIT`。

`POST /api/v1/runs/{run_id}/retry` 使用相同的 Conversation 行锁和 active Run 检查；重复 retry idempotency key 在 active Run 检查前返回原 retry Run，其他并发 retry 返回 409。

### Run/SSE 权限

- `getRun`、`listRunEvents`、SSE：Project member 均可读；viewer 可订阅但不可写。
- cancel/retry：owner/editor 可操作自己的 Run；owner 可操作任意 Run；viewer 拒绝。
- Run 发起者通过 `runs.trigger_message_id -> messages.sender_id` 判定，不新增 Run 字段。
- 保留现有 URL、SSE envelope、事件顺序和 Run 状态机。

### 必须补齐的测试（B 可用后）

1. 2H+2A：demo/owner 与 other/editor 加入同一 Project；两个 Agent 均映射到该 Project。
2. 共享 Conversation：两个用户都能 GET 同一 Conversation 和完整 Message；非成员 403。
3. 不同用户分别以不同 `target_agent_id` 发送，Run 的 Agent 与回复消息 sender 正确。
4. viewer 读取/订阅成功，发送、取消、重试 403。
5. owner/editor 只能取消/重试自己的 Run，owner 可操作任意 Run。
6. 发送与 retry 并发时最多一个 queued/running Run，冲突返回 409 而非数据库 500。
7. 不同用户使用相同 `client_request_id` 不冲突；同用户重复请求返回原 Run。

## 修改文件

- `server/src/core.cpp`
- `db/migrations/007_conversation_project_required.sql`
- `tests/validate_contracts.py`
- `tests/e2e.py`
- `docs/schema/erd.md`
- `docs/schema/data-dictionary.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-C-20261116-shared-conversation.md`

## 契约变更

- 数据库：`conversations.project_id` 由可空收紧为 `NOT NULL`。
- REST：兼容 Conversation 创建/列表返回新增 `project_id` 字段；URL 与既有字段不变。
- Project-first REST、`target_agent_id` 运行时强制和 Project member 权限切换仍等待 Package B，当前不宣称已运行可用。

## 验证命令与结果

- 基线 `./scripts/test.ps1 -Docker`：PASS（修改前）。
- `git diff --check`：PASS。
- Core Docker build/start：PASS。
- PostgreSQL：`schema_migrations` 含 `007_project_id_required` 且仅 1 行；`conversations.project_id is_nullable=NO`；NULL 行数 0。
- Core 重启重复执行 007：PASS；迁移行仍为 1，schema 仍为 `NO`，health=healthy。
- 修改后 `./scripts/test.ps1 -Docker`：PASS；契约、Worker、前端状态、单用户 E2E 全部通过，E2E 新增 `project_id` 断言通过。

## 验收映射

- Conversation 写入 `project_id`：本包独立部分完成。
- 007 `NOT NULL`：本包独立部分完成。
- Project-first Conversation、Project member 权限、多 Agent 发送、单 active Run：BLOCKED，等待 Package B。
- 2H+2A 验收：BLOCKED；未以静态测试冒充通过。

## 已知问题与风险

- Package B 缺失；现在合入 C 的依赖部分会重复或覆盖 B 的成员/Agent 授权实现。
- OpenAPI 的 `Conversation.project_id` 仍是可空声明；运行迁移已非空，最终契约收紧需与 B/C 一并由 COORD 审核。
- 前端仍不发送 `target_agent_id`；运行切换完成后前端会 400，需 WEB 后续接入。

## 需要其他 Agent 处理的事项

- CORE：完成 Package B 并交接 Project service membership helper 与路由。
- COORD：批准最终 OpenAPI 中 `project_id` required/non-null 及 Project-first Conversation 运行时说明。
- WEB：切换 Project-first Conversation 与 `target_agent_id` 发送参数。

## 下一任务建议

Package B 提交后，将本分支 rebase 到 B 最新本地提交，补齐 Project-first Conversation 路由、发送/retry 行锁、Run/SSE Project member 权限和 2H+2A 测试。
