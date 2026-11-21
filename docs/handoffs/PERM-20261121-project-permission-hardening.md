# Handoff: PERM-20261121 Project permission hardening, audit read and archive

- 状态：REVIEW
- 主责 Agent：CORE（含最小 WEB/QA/文档同步）
- 分支 / 提交：`agent/core/conversation-realtime` / `79d1bd9`（功能、测试与文档提交；后续仅补充交接同步事实）
- 开始时间：2026-11-21
- 完成时间：2026-11-21

## 目标

修复多人 MVP Project 权限缺口：集中式 role × action 矩阵、CheckPermission 显式 Project 上下文、移除成员时清理其 Project Agent 映射、owner-only 脱敏审计读取 API 与 owner-only Project 软归档。保持 owner/editor/viewer 和 viewer 只读可见范围不变。

## 完成内容

- 所有 REST 授权统一使用同一 `project_role_allows` 矩阵，默认拒绝未知 action。
- `CheckPermissionRequest` 增加 `project_id = 6`；C++ 按 Project 校验 Agent/Conversation/Run，并同时执行 action 权限判断。
- 移除 Project 成员与清理该成员拥有的 `project_agents` 映射在同一事务完成；历史 Message/Run/审计保留，active Run 仍阻止移除。
- 新增 `GET /api/v1/projects/{project_id}/audit-events`：owner-only，按 Project 过滤，metadata 仅返回白名单字段。
- 新增 `POST /api/v1/projects/{project_id}/archive`：owner-only 单向软关闭；active Run 409；归档后读取保留、写操作 409；重复归档返回当前 Project；无取消归档。
- 前端 Project 设置增加 owner-only 审计列表和归档入口；归档后进入只读 UI，viewer/editor 不显示敏感入口。
- 未新增数据库迁移；复用 `projects.status` 与 `audit_events`。

## 修改文件

- 契约/文档：`MASTER-PRD.md`、`多人平台的最小mvp_PRD.md`（workspace 级）；`api/openapi.yaml`、`proto/workspace.proto`；`AGENTS.md`、`README.md`、`docs/contracts/*`、`docs/runbook/A04-A06-contract-map.md`、`docs/runbook/A07-permissions.md`、`docs/schema/*`、`docs/acceptance/V0.2-multiplayer-mvp.md`、`docs/TASK_BOARD.md`。
- Core：`server/src/core.hpp`、`server/src/core.cpp`、`server/src/app.cpp`、`server/src/grpc_server.cpp`。
- Web：`frontend/app.js`、`frontend/index.html`、`frontend/styles.css`。
- 生成代码：`worker/gen/aiworkspace/v1/workspace.pb.go`、`worker/gen/aiworkspace/v1/workspace_grpc.pb.go`。
- 测试/脚本：`tests/validate_contracts.py`、`tests/project_api.py`、`tests/shared_conversation.py`、`tests/frontend_state_test.js`、`tests/frontend_2h2a_e2e.js`、`tests/permission_rpc/main.go`、`scripts/e2e.ps1`。
- 证据：`docs/evidence/ui-project-first.png`、`docs/evidence/ui-2h2a.png`。
- Excel 状态校准：原始 `MVP技术难点与开发清单_2026-11-19.xlsx` 被 Excel 锁定，已生成 `MVP技术难点与开发清单_2026-11-21_权限归档校准.xlsx`，仅更新相关 15 行状态/证据，不改产品范围。

## 契约变更

`CONTRACT-CHANGE-REQUEST`

- OpenAPI `1.1.0 -> 1.2.0`：新增 owner-only Project archive/audit-events paths、`AuditEvent` schema 与归档 403/409 语义。
- Proto `1.2.0`：`CheckPermissionRequest.project_id = 6`；服务端要求非空 UUID。旧调用方若不传该字段会收到校验错误，但仓库当前没有 Worker 调用该 RPC。
- 无数据库迁移；无 SSE envelope 或 Run 状态机变化。
- Q4-B 已由用户确认：归档为单向软关闭，不提供取消归档。

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Build -Up
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-health.ps1
git diff --check
```

- Docker Core/Worker build：PASS。
- `test.ps1 -Docker`：PASS。契约 28 operations / 6 RPC；Go test/vet；frontend syntax/state；单用户 E2E；shared Conversation；Conversation realtime；Project API；permission RPC；pool；migration replay；全部 PASS。
- Worker `go test`：PASS；generated protobuf compiles with `project_id`。
- `frontend-e2e.ps1`：PASS（owner Project-first 对话）。
- `frontend-2h2a-e2e.ps1`：PASS（双浏览器共享历史/SSE、owner 审计/归档入口、editor/viewer 隐藏敏感入口）。
- `test-health.ps1`：PASS（503 degraded/unavailable -> 200 recovery）。
- `git diff --check`：PASS。

## 验收映射

- viewer/editor/非成员读取审计 403，owner 成功：PASS。
- 同一 Agent 按 action 区分读取/写入（`agent.read` vs `agent.use`）：PASS。
- Agent 必须在请求 Project 中：PASS（permission RPC fixture）。
- 成员移除后 Agent 映射清理、历史消息保留：PASS。
- owner 归档、editor/viewer/非成员 403、active Run 409：PASS。
- 归档后读取仍可用、Conversation/Message/成员/Agent 写入 409：PASS。
- `project.archive` 审计存在且 metadata 不含敏感字段：PASS。
- Q2 viewer 可见范围和三角色名称未变化：PASS。

## 已知问题与风险

- `CheckPermission` 的 `project_id` 在 proto3 中是可选 wire field，但服务端语义上必填；仓库内无旧调用方。
- 归档当前为幂等软关闭，不做取消归档；若未来需要恢复，必须新增版本化契约与产品决策。
- 同一 Project 写路径仍使用 Project 行锁串行化，MVP 规模可接受。
- Excel 原始清单保存时被占用；已保留校准副本，待原文件关闭后可替换或合并。

## 需要其他 Agent 处理的事项

- COORD 审查 OpenAPI/Proto 版本与分支合并。
- WEB/QA 复核归档后所有只读边界和审计 metadata 白名单。

## 下一任务建议

- 按 L2-08 评估 Conversation 级锁粒度优化；随后推进中心化部署与正式审计查看权限策略。
