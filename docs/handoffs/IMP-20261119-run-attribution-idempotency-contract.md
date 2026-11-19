# Handoff: IMP-20261119 run attribution and idempotency contract

- 状态：DONE
- 主责 Agent：CORE/COORD
- 分支 / 提交：`main` / `2cd622d`（实现在 `7caee67`，merge 后基线为 `2cd622d`）
- 开始时间：2026-11-19
- 完成时间：2026-11-19

## 目标

- 将发送消息的 `client_request_id` 收紧为必填合同。
- 为 Run 增加显式 `initiated_by_user_id`，区分触发消息作者与实际发起/重试者。
- 保持 Project-first 数据模型、4A 幂等、5A Project+Conversation 行锁和 6A 同事务审计。
- 将 5B 锁粒度优化登记为后续计划，不在本任务实施。

## 完成内容

- OpenAPI 升级到 `1.0.1`，发送消息要求 `content`、`client_request_id`、`target_agent_id`。
- C++ 发送入口拒绝缺失 `client_request_id`，并拒绝与 `Idempotency-Key` 不一致的请求。
- 新增 `008_run_initiated_by.sql`，回填历史 Run 并将 `runs.initiated_by_user_id` 收紧为 NOT NULL。
- 发送新 Run 和 retry Run 都写入实际操作用户；retry 新 Run 归执行 retry 的用户。
- 取消、重试和成员移除保护改为按 `initiated_by_user_id` 判断。
- 更新契约、ERD、数据字典、ADR、任务看板和验收记录。
- 新增 5B 后续任务 `L2-08`：Conversation 级锁 + membership/Agent 授权行共享锁；当前继续使用 5A。
- 修复 CMake Protobuf 查找：优先 CONFIG，缺失时回退 MODULE，兼容 Docker 与 MSYS2。

## 契约变更

`CONTRACT-CHANGE-REQUEST`

- REST：`POST /api/v1/conversations/{conversation_id}/messages` 的 `client_request_id` 改为必填，长度仍为 8-100。
- 数据库：新增只增迁移 `008_run_initiated_by.sql`。
- Run 权限：owner/editor 的“自己的 Run”按 `runs.initiated_by_user_id` 判断；`trigger_message_id` 保留原始触发消息。
- 兼容策略：历史 Run 回填为触发消息作者；新发送和 retry 均写实际操作用户。
- SSE envelope、错误码、端口、Run 状态机不变。

## 修改文件

- `api/openapi.yaml`
- `server/src/app.cpp`
- `server/src/core.cpp`
- `db/migrations/008_run_initiated_by.sql`
- `CMakeLists.txt`
- `tests/e2e.py`
- `tests/shared_conversation.py`
- `tests/validate_contracts.py`
- `scripts/test-migrations.ps1`
- `scripts/e2e.ps1`
- `docs/schema/erd.md`
- `docs/schema/data-dictionary.md`
- `docs/contracts/README.md`
- `docs/runbook/A07-permissions.md`
- `docs/runbook/A12-reliability.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/TASK_BOARD.md`
- `docs/adr/ADR-0002-versioning-state-machine.md`
- `docs/adr/ADR-0005-run-attribution-idempotency.md`
- `AGENTS.md`
- 本交接文件

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/test.ps1 -Docker
./scripts/frontend-2h2a-e2e.ps1
```

- `setup-dev.ps1 -Up`：PASS；Core/Worker 重建并健康启动。
- `test.ps1 -Docker`：PASS。
  - 契约：PASS；OpenAPI 25 operations、6 RPC、5 SSE events。
  - Worker test/vet、前端语法/状态测试：PASS。
  - 单用户 E2E：PASS；`client_request_id` 必填、header/body 不一致、幂等、SSE、cancel/retry。
  - shared Conversation E2E：PASS；2H+2A、不同用户同 key、单 active Run、Run attribution。
  - Project API fixture：PASS；active Run 成员/Agent 移除保护。
  - 迁移重放：PASS；migrations=8、`initiated_by_nullable=NO`、NULL rows=0、008 记录唯一。
- `frontend-2h2a-e2e.ps1`：PASS；双浏览器、双 Agent、双方 SSE、viewer 只读。

## 已知问题与风险

- 5A 仍会串行化同一 Project 的写路径；5B 已登记为 `L2-08`，需在真实吞吐或并发需求出现后再实施。
- 同 key 不同正文仍返回原请求结果；需要参数指纹校验时升级为通用幂等表。
- OpenAPI 1.0.1 与 Proto/SSE envelope/错误码 1.0.0 兼容，主版本不变。

## 下一任务建议

1. 由 COORD 审查并合并本分支。
2. 合并后再评估 Conversation 级 Run 发现 SSE 与 5B 锁粒度。
