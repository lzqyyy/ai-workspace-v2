# Handoff: MVP-CLOSEOUT-20261126 Run Owner Traceability and Retry Target

- 状态：DONE
- 主责 Agent：CORE/WEB/QA
- 分支 / 提交：`main` / `7d18b03`
- 开始时间：2026-11-26
- 完成时间：2026-11-26

## 目标

按用户确认的 `A A A B A` 收尾当前最小 MVP，不新增数据库迁移、不改变
`owner/editor/viewer` 权限矩阵和 5A 并发方案：

- REST `Run` 返回中增加由 `runs.agent_id -> agents.owner_id` 推导的
  `agent_owner_id`；
- 发送与重试响应显式返回真实的 `target_agent_id`，避免前端在切换 Agent
  后重试旧 Run 时显示错误目标 Agent；
- 前端 retry 使用服务端返回的目标 Agent；
- 补齐 OpenAPI、契约、前端状态和共享会话回归测试。

## 写入范围

- `server/src/core.cpp`
- `frontend/app.js`
- `api/openapi.yaml`
- `tests/shared_conversation.py`
- `tests/frontend_state_test.js`
- `tests/validate_contracts.py`
- `docs/contracts/README.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/MVP-CLOSEOUT-20261126-run-owner-retry.md`

不新增迁移，不修改 Proto、SSE envelope、Run 状态机、端口或权限矩阵。

## 完成内容

- [x] Run REST 响应增加推导字段 `agent_owner_id`
- [x] 新增 `GET /api/v1/runs/{run_id}` 并复用 Project 成员授权
- [x] 发送响应增加真实 `target_agent_id`
- [x] retry 响应与重复键响应增加真实 `target_agent_id`
- [x] 前端 retry 优先使用服务端目标 Agent
- [x] 契约与回归测试
- [x] 文档与任务状态同步

## 契约变更

- REST Run schema 增加推导的 `agent_owner_id`，不新增数据库列。
- `SendMessageResponse` 增加真实 `target_agent_id`。
- 版本从 `OpenAPI 1.2.0` 升至 `1.3.0`；Proto、SSE envelope、URL、
  Run 状态机和数据库迁移不变。

## 验证命令与结果

- `node --check frontend/app.js`：PASS。
- `node tests/frontend_state_test.js`：PASS；新增 `retry_target_agent=true`。
- `py -3 tests/validate_contracts.py`：PASS；OpenAPI 29 operations。
- `./scripts/test.ps1 -Docker`：PASS；契约、Worker、前端状态、单用户 E2E、shared Conversation E2E、Conversation realtime、Permission RPC、Project API、连接池和迁移重放全部通过。
- `./scripts/e2e.ps1 -Docker`：PASS；补充的 viewer GET Run 与非成员 403 断言通过。
- `./scripts/frontend-e2e.ps1`：PASS；Project-first 浏览器链路通过。

## 验收映射

- Run 可由 API 追溯 Agent Owner 与实际发起者。
- 当前选择 Agent B 时重试 Agent A 的旧 Run，响应和前端状态仍指向 Agent A。
- 既有发送/retry 幂等、单 active Run、Project 成员权限与 SSE 行为不回归。

## 已知问题与风险

- 不新增 `runs.owner_id` 快照；Agent Owner 按当前 `agents.owner_id` 推导。
- 不增加 active Run 的 partial unique index，继续使用 5A 行锁。
- 本次不改短轮询 SSE、消息/RunEvent 查询上限或前端框架。

## 需要其他 Agent 处理的事项

无。

## 下一任务建议

1. 在真实吞吐或独立写路径出现后评估 L2-08（5B/5C）。
2. 若需要历史归属不可变，再评估 Agent Owner 快照（T2）。
