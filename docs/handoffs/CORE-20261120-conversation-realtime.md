# Handoff: CORE-20261120 Conversation realtime

- 状态：REVIEW
- 主责 Agent：CORE / WEB / QA
- 分支 / 提交：`agent/core/conversation-realtime` / HEAD（提交见最终报告）
- 开始时间：2026-11-20
- 完成时间：2026-11-20

## 目标

增加最小 Conversation 级实时通知，使 Project 成员在创建或重试新 Run 后立即收到 `run.available`，再用既有 `run_id` 订阅 Run SSE；保留 2.5 秒消息轮询作为兜底，并修正跨成员流式展示使用本地 `targetAgentId` 的问题。

## 完成内容

- 新增只增迁移 `009_conversation_events.sql`；C++ 在创建/重试 Run 的同一事务写入 `run.available`。
- 新增 `GET /api/v1/conversations/{conversation_id}/events` SSE 路由，支持 `Last-Event-ID` / `after` 回放。
- Conversation 事件包含 `conversation_id`、`run_id`、真实 `target_agent_id`、`initiated_by_user_id` 和 `status`。
- 前端为当前 Conversation 建立独立事件流；收到事件后调用既有 `beginRun()` / `connectRun()`，使用服务端目标 Agent。
- 前端轮询兜底发现 Run 时读取 `GET /api/v1/runs/{run_id}` 的 `agent_id` / `initiated_by_user_id`，不再使用本地 `state.targetAgentId`。
- Run GET 响应补充 `agent_id`、`initiated_by_user_id`；Run SSE envelope 未改变。
- 保留 2.5 秒消息轮询，作为刷新和实时事件丢失兜底。

## 修改文件

- `api/openapi.yaml`
- `db/migrations/009_conversation_events.sql`
- `server/src/core.hpp`
- `server/src/core.cpp`
- `server/src/app.cpp`
- `frontend/app.js`
- `tests/conversation_realtime.py`
- `tests/frontend_state_test.js`
- `tests/frontend_2h2a_e2e.js`
- `tests/validate_contracts.py`
- `scripts/e2e.ps1`
- `scripts/test-migrations.ps1`
- `docs/contracts/sse-events.md`
- `docs/contracts/README.md`
- `docs/adr/ADR-0002-versioning-state-machine.md`
- `docs/adr/ADR-0006-conversation-event-stream.md`
- `docs/schema/erd.md`
- `docs/schema/data-dictionary.md`
- `docs/runbook/A07-permissions.md`
- `docs/runbook/A12-reliability.md`
- `docs/runbook/A13-testing.md`
- `docs/runbook/A15-multiplayer-demo.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/TASK_BOARD.md`
- `README.md`
- `AGENTS.md`
- `docs/evidence/ui-project-first.png`
- `docs/evidence/ui-2h2a.png`

## 契约变更

`CONTRACT-CHANGE-REQUEST`

- OpenAPI：`1.0.1` -> `1.1.0`；新增 Conversation events path 与 `ConversationEvent` schema；Run 响应增加 `agent_id`、`initiated_by_user_id`。
- SSE：`1.1.0`；新增 Conversation `run.available` 流，Run SSE `1.0` envelope、事件名和状态机保持不变。
- 数据库：新增只增迁移 `009_conversation_events.sql`，`(conversation_id, seq)` 唯一。
- 授权：Conversation 事件与 Run SSE 共用 Project membership；viewer 可订阅，非成员 403。
- 架构：不引入 Redis/Kafka/WebSocket；Worker 不访问 PostgreSQL；C++ 仍是唯一 PostgreSQL 业务写入者。

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-health.ps1
```

- `setup-dev.ps1 -Up`：PASS。
- `test.ps1 -Docker`：PASS。
  - 契约：PASS；OpenAPI 26 operations、Run SSE 5、Conversation SSE 1。
  - Worker test/vet、前端语法/状态测试：PASS。
  - 单用户 E2E：PASS。
  - shared 2H+2A API E2E：PASS。
  - Conversation realtime E2E：PASS；`run.available` 实测延迟约 515ms；duplicate send 不增事件、retry 新 Run 写入下一条事件；回放/viewer/非成员授权通过。
  - Project API fixture、迁移重放：PASS；migrations=9、`conversation_events` 表存在、009 唯一。
- `frontend-e2e.ps1`：PASS；单浏览器 Project-first 链路完成。
- `frontend-2h2a-e2e.ps1`：PASS；双浏览器均观察到 Conversation SSE、双方 Run SSE、正确 Agent 身份和 viewer 只读。
- `test-health.ps1`：PASS；数据库停止时 503 `degraded/unavailable`，恢复后 200。
- `git diff --check`：PASS。

## 验收映射

- Project member 新 Run 实时发现：PASS。
- `run.available` 字段完整且目标 Agent 真实：PASS。
- Conversation 事件回放：PASS。
- Retry 事件写入与幂等重复不重复事件：PASS。
- viewer 可订阅、非成员 403：PASS。
- 2.5 秒轮询兜底仍存在：PASS。
- Run SSE envelope、单 active Run、幂等与 Run 状态机兼容：PASS。
- Docker、浏览器和 health 回归：PASS。

## 已知问题与风险

- Conversation SSE 与 Run SSE 当前都通过 C++ 服务端短周期查询 PostgreSQL 事件表推动；本轮保持现有实现模式，不新增数据库通知或消息总线。
- 前端只维护当前活动 Conversation 的事件流；切换 Conversation 时切换订阅。其他 Conversation 仍需靠 2.5 秒轮询或切换后回放发现。
- 首次无游标时从当前 tail 开始，避免新浏览器重放全部历史事件；显式游标仍可完整回放。
- 同一 Project 写路径仍使用既有 Project 行锁串行化，属于已知 MVP 限制。
- 本任务按要求未 push、未合并 `main`、未 force push。

## 下一任务建议

1. 由 COORD 审查并合并本分支。
2. 视真实吞吐需要推进 L2-08 锁粒度优化。
3. 再进入中心化部署与后续产品能力。
