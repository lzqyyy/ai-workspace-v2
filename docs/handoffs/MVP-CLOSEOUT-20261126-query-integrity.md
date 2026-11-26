# Handoff: MVP-CLOSEOUT-20261126 Query Integrity

- 状态：DONE
- 主责 Agent：CORE/WEB/QA
- 分支 / 提交：`agent/core/mvp-closeout-query-integrity` / 本任务同步提交
- 开始时间：2026-11-26
- 完成时间：2026-11-26

## 目标

按用户确认的 `DB1 + PG1 + Q1 + R1` 收口当前 P0：

- `MVP-06`：新增单 active Run partial unique index；保留应用层目标 Agent 校验。
- `MVP-07`/`MVP-15`：消息查询默认 100、最大 200，增加增量分页字段；RunEvent 查询固定批量上限；保留消息只追加、无编辑/删除路由。
- `MVP-44`：用可重复的 EXPLAIN/性能 smoke 验证消息、RunEvent、审计和项目查询使用索引。
- `R1`：不新增角色，不改变 owner/editor/viewer 权限矩阵。
- `MVP-45`：本次不纳入 P0；继续作为 P1 条件项。

## 完成内容

- [x] `010_single_active_run.sql` 增加 `ux_runs_one_active_per_conversation` partial unique index，并在迁移前拒绝已有重复 active Run。
- [x] C++ 数据库层把该唯一索引冲突映射为 409 `CONFLICT`。
- [x] Message API 增加 `limit`（默认 100/最大 200）、`has_more`、`next_after_seq`。
- [x] 前端首次加载按页拉完整历史，轮询只拉 `after_seq` 增量并合并。
- [x] RunEvent SSE 单次查询限制 200 条。
- [x] 增加 `scripts/test-query-integrity.ps1`：检查唯一索引、消息/RunEvent EXPLAIN、关键索引。
- [x] 更新 OpenAPI 1.4.0、契约、ERD、AGENTS、README、测试 Runbook 和任务看板。
- [x] E2E 增加消息分页边界与 `after_seq`/`limit` 参数校验。
- [x] 前端状态测试增加分页循环和增量轮询合并。
- [x] 修复 lease/cancel/event-seq 专项测试中遗留的旧 `send()` 参数签名。

## 修改文件

- `db/migrations/010_single_active_run.sql`
- `server/src/app.cpp`
- `server/src/core.cpp`
- `server/src/core.hpp`
- `server/src/database.cpp`
- `frontend/app.js`
- `api/openapi.yaml`
- `tests/e2e.py`
- `tests/frontend_state_test.js`
- `tests/validate_contracts.py`
- `tests/lease_guard.py`
- `tests/cancel_lease_expiry.py`
- `tests/event_seq_guard.py`
- `scripts/test-query-integrity.ps1`
- `scripts/test-migrations.ps1`
- `scripts/test.ps1`
- `README.md`
- `AGENTS.md`
- `docs/TASK_BOARD.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/schema/erd.md`
- `docs/contracts/README.md`
- `docs/contracts/sse-events.md`
- `docs/runbook/A13-testing.md`

## 验证命令与结果

- `node --check frontend/app.js`：PASS
- `node tests/frontend_state_test.js`：PASS（含 message_pagination、incremental_polling）
- `py -3 -m py_compile tests/e2e.py tests/shared_conversation.py tests/validate_contracts.py tests/lease_guard.py tests/cancel_lease_expiry.py tests/event_seq_guard.py`：PASS
- `py -3 tests/validate_contracts.py`：PASS（OpenAPI 1.4.0；29 operations）
- PowerShell 脚本语法解析：PASS
- `git diff --check`：PASS（仅有 Git 行尾提示）
- `./scripts/setup-dev.ps1 -Up`：PASS；C++ Core 与 Worker 重新构建并健康启动
- `./scripts/test.ps1 -Docker`：PASS；契约、Worker、前端状态、连接池、单用户 E2E、2H+2A、权限、迁移重放、查询完整性全部通过
- `./scripts/test-migrations.ps1 -Docker`：PASS；`migrations=10`、`migration_010=1`、`active_run_index=1`、`restart_replay=PASS`
- `./scripts/test-query-integrity.ps1 -Docker`：PASS；message/RunEvent EXPLAIN、项目索引、active Run unique constraint 全部通过
- `./scripts/frontend-e2e.ps1`：PASS；浏览器单用户链路与截图更新
- `./scripts/frontend-2h2a-e2e.ps1`：PASS；双浏览器、双人类、双 Agent、双方 SSE、viewer 只读
- `./scripts/test-health.ps1`：PASS；停库 503、恢复 200
- `./scripts/test-lease.ps1`：PASS；wrong lease、cancel lease expiry、event seq gap/duplicate/out-of-order 全部通过

## 验收映射

- `MVP-06`：唯一索引存在；query integrity 断言第二个 active Run 插入被 `unique_violation` 拒绝；Run 归属原有回归继续 PASS。
- `MVP-07`/`MVP-15`：OpenAPI 默认/最大 limit、E2E 分页边界、前端分页/增量测试、无编辑/删除路由保持。
- `MVP-44`：消息与 RunEvent 增量查询 EXPLAIN 无 Seq Scan；关键索引存在；SSE 单批上限 200。
- `R1`：owner/editor/viewer 权限测试继续 PASS，无角色变更。

## 已知问题与风险

- `MVP-45` 未纳入本次 P0：真实 HTTPS 反向代理、备份恢复演练和正式部署仍后置。
- 本次采用应用层 5A 行锁 + `010` partial unique index 双保险；未引入目标 Agent 数据库触发器，目标 Agent 仍由 C++ 事务内 `requireProjectAgent` 校验。
