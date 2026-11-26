# 冻结契约索引

版本：`OpenAPI 1.3.0 / Proto 1.3.0`。原冻结日期：2026-11-10；工程包 A 增量冻结：2026-11-16；Run 归属与发送幂等合同收紧：2026-11-19；Conversation 实时通知：2026-11-20；Project 归档与审计读取：2026-11-21；D-13 执行归属与不可信输入边界：2026-11-25；Run Owner / retry target 接口可追溯：2026-11-26。

| 契约 | 文件 | 校验 |
|---|---|---|
| REST | `api/openapi.yaml` | `python tests/validate_contracts.py` |
| Project-first REST | `api/openapi.yaml` | Project/成员/Agent/Conversation 路径与 schema、`client_request_id` 与 `target_agent_id` 必填、角色枚举 |
| gRPC | `proto/workspace.proto` | `protoc --descriptor_set_out` + Go/C++ stub generation |
| SSE | `docs/contracts/sse-events.md` | Run SSE 回放与 Conversation `run.available` 专项 E2E |
| 错误码 | `docs/contracts/error-codes.md` | 契约测试校验 OpenAPI 枚举与文档一致 |
| Project 权限/分阶段迁移 | `docs/adr/ADR-0004-project-membership.md` | 文档审查 |
| Run 归属/幂等/并发 | docs/adr/ADR-0005-run-attribution-idempotency.md | 文档审查 |\n| Conversation 实时通知 | docs/adr/ADR-0006-conversation-event-stream.md | 文档审查与专项 E2E |

工程包 A 冻结 Project-first 契约；Package B/C 已实现 Project/成员/Agent 与共享 Conversation 路由，并将 Conversation/Message/Run/SSE 权限切换为 Project membership。`1.0.1` 额外收紧发送 `client_request_id` 为必填，并通过 `008` 增加 `runs.initiated_by_user_id`；`1.1.0` 通过 `009` 增加 Conversation `run.available` 事件与回放游标；`1.2.0` 增加 owner-only Project 软归档和脱敏审计读取，并为 `CheckPermissionRequest` 增加显式 `project_id`；`1.3.0` 在 Run 响应中返回推导的 `agent_owner_id`，并在发送/重试响应中返回真实 `target_agent_id`；Proto `1.3.0` 为 `GetExecutionContextResponse` 增加 `project_id`、`agent_owner_id`、`initiated_by_user_id`。URL 端口、Run 状态机、SSE envelope 和数据库迁移不变。契约变更遵循 `AGENTS.md` 第 8 节。
