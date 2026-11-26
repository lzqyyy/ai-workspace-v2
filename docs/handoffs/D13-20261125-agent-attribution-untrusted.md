# Handoff: D13-20261125 Agent attribution and untrusted input boundary

- 状态：DONE
- 主责 Agent：CORE/WORKER/QA
- 分支 / 提交：`agent/core/d13-agent-attribution-untrusted`
- 开始时间：2026-11-25
- 完成时间：2026-11-25

## 目标

按用户确认的 `P1 + T1` 闭环 D-13：保持既有 owner/editor/viewer 权限矩阵；Run 的 Agent Owner 通过
`runs.agent_id -> agents.owner_id` 语义推导；补齐 Worker 授权上下文、消息不可信输入边界、
Permission RPC 的 Run 发起者校验，以及跨项目/提示注入专项回归。

## 完成内容

- [x] `CheckPermission` 对 `run.cancel` / `run.retry` 校验 `runs.initiated_by_user_id`。
- [x] `GetExecutionContextResponse` 增加 `project_id`、`agent_owner_id`、`initiated_by_user_id`。
- [x] Worker 明确把会话消息视为不可信数据，不据此扩权。
- [x] 增加跨项目 Agent 拒绝、Run 归属授权和提示注入/上下文隔离测试。
- [x] 同步契约、验收、任务板、Runbook 与 Excel 状态。

## 修改文件

- `proto/workspace.proto`、`worker/gen/aiworkspace/v1/workspace.pb.go`
- `server/src/core.cpp`、`server/src/grpc_server.cpp`
- `worker/cmd/worker/main.go`、`worker/cmd/worker/provider.go`、`worker/cmd/worker/provider_test.go`
- `tests/permission_rpc/main.go`、`tests/shared_conversation.py`、`tests/validate_contracts.py`
- `scripts/e2e.ps1`
- `api/openapi.yaml`、`AGENTS.md`、`docs/contracts/README.md`、`docs/runbook/A07-permissions.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`、`docs/TASK_BOARD.md`
- Excel：`D:\workspace\outputs\01a1eee3-164a-7b82-9788-8afc80a03b40\MVP技术难点与开发清单_2026-11-25_D13校准.xlsx`

## 契约变更

- Proto `1.2.0 -> 1.3.0`：`GetExecutionContextResponse` 增加 `project_id = 13`、`agent_owner_id = 14`、`initiated_by_user_id = 15`。
- 不改变 RPC、Run 状态机、SSE envelope、REST 路径或数据库迁移。

## 验证命令与结果

- `./scripts/test.ps1 -Docker`：PASS（契约、Go test/vet、前端状态、单用户 E2E、2H+2A、realtime、Project API、Permission RPC、连接池、迁移重放）。
- Permission RPC：`run_initiator_enforced=true`；editor 仅能操作自己的 Run，Project owner 可操作任意 Run。
- Shared Conversation E2E：`untrusted_message_boundary=true`、`cross_project_context_isolated=true`。
- C++ Docker build：PASS；Proto `1.3.0` 与生成 Go 代码编译：PASS。
- `git diff --check`：PASS。

## 验收映射

- 跨项目 Agent 执行被拒绝：PASS。
- Run 归属可追溯：PASS。
- 提示注入样例无法读取其他项目数据：PASS。

## 已知问题与风险

- 不新增 `runs.owner_id` 快照；Agent Owner 按 `runs.agent_id -> agents.owner_id` 推导。
- capability token、工具白名单和高危工具确认继续后置。

## 需要其他 Agent 处理的事项

- 无；本任务内由 CORE/WORKER/QA 完成。

## 下一任务建议

- 评估是否需要为历史 Run 增加不可变 Agent Owner 快照（T2），不在本任务范围。
