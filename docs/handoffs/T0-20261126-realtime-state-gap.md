# Handoff: T0-20261126 实时状态与 gap 恢复

- 状态：DONE
- 主责 Agent：WEB/QA
- 分支 / 提交：`main` / 待自动同步
- 开始时间：2026-11-26
- 完成时间：2026-11-26

## 目标

按用户确认的 `S1 + P0 + T0` 闭合当前 MVP 前端实时状态缺口：

- 保持当前 Project 成员 SSE 权限矩阵，不修改 P0 权限语义。
- 使用原生 JavaScript 的最小 store/reducer 管理 Conversation、消息、Project Agent、Run 和事件游标。
- Run SSE 与 Conversation SSE 检测 seq gap；不得直接跳过。
- gap 时关闭当前流、重新拉取 REST 快照，再从最后成功应用的游标恢复。
- 保持重复事件去重、刷新恢复和现有 SSE envelope。

## 写入范围

- `frontend/app.js`
- `tests/frontend_state_test.js`
- `docs/TASK_BOARD.md`
- `AGENTS.md`
- `docs/handoffs/T0-20261126-realtime-state-gap.md`

未修改 OpenAPI、Proto、数据库迁移、C++/Go 权限逻辑、端口或 SSE envelope。

## 完成内容

- [x] 最小 reducer/store：Conversation、Message、Project Agent、target Agent、Run、Conversation cursor。
- [x] Run seq gap：关闭旧流、读取 Run/Message 快照、保留最后成功游标并重连回放。
- [x] Conversation seq gap：读取 Message 快照、成功后再推进并持久化 cursor、重连通知流。
- [x] 重复事件去重和刷新恢复回归保持通过。
- [x] 后台 Run gap 恢复不会覆盖当前活动 Conversation 的消息快照。
- [x] Conversation resync 按 Conversation 作用域，切换会话不会阻塞新 SSE。
- [x] EventSource/timer 保持在 reducer 外，store 只管理领域状态。

## 契约变更

无。保持 `OpenAPI 1.2.0`、`Proto 1.3.0`、Run SSE envelope、Conversation SSE 和权限矩阵不变。

## 验证命令与结果

- `node --check frontend/app.js`：PASS。
- `node tests/frontend_state_test.js`：PASS。
  - `store_reducer=true`
  - `run_gap_snapshot=true`
  - `conversation_gap_snapshot=true`
  - `background_run_isolation=true`
  - `sse_reconnect=true`
  - `sse_dedupe=true`
  - `refresh_restore=true`
- `./scripts/test.ps1 -Docker`：PASS；契约、Worker、前端状态、单用户 E2E、2H+2A API、权限 RPC、连接池、迁移重放全部通过。
- `./scripts/frontend-2h2a-e2e.ps1 -Screenshot build/t0-ui-2h2a.png`：PASS；owner/editor 双浏览器共享历史、双方 Conversation/Run SSE、viewer 只读、owner 审计/归档 UI 均通过。
- `git diff --check`：PASS。

## 验收映射

- 两个成员可同时订阅同一 Run：PASS（双浏览器 E2E）。
- 断线重连不丢不重：PASS（游标回放、重复去重、Run/Conversation gap 状态测试）。
- 刷新后消息、目标 Agent 和事件游标可恢复：PASS（刷新恢复与状态测试）。
- 非成员/越权权限边界：保持既有 P0 矩阵，Docker 权限测试 PASS。

## 已知问题与风险

- S1 不实现 Project 级成员/角色/Agent 变更实时事件流；该能力留到 S2。
- 未改后端事件总线；当前仍是 C++ 查询 PostgreSQL 事件表推动 SSE。
- T1 全量前端状态迁移仍后置，当前只覆盖实时协作相关领域状态。
