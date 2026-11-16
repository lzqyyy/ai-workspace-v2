# 冻结契约索引

版本：`1.0.0`。原冻结日期：2026-11-10；工程包 A 增量冻结：2026-11-16。

| 契约 | 文件 | 校验 |
|---|---|---|
| REST | `api/openapi.yaml` | `python tests/validate_contracts.py` |
| Project-first REST | `api/openapi.yaml` | Project/成员/Agent/Conversation 路径与 schema、`target_agent_id` 必填、角色枚举 |
| gRPC | `proto/workspace.proto` | `protoc --descriptor_set_out` + Go/C++ stub generation |
| SSE | `docs/contracts/sse-events.md` | 同一次 E2E 校验 `run.started`、`message.delta`、`message.completed` |
| 错误码 | `docs/contracts/error-codes.md` | 契约测试校验 OpenAPI 枚举与文档一致 |
| Project 权限/分阶段迁移 | `docs/adr/ADR-0004-project-membership.md` | 文档审查 |

工程包 A 只冻结 Project-first 契约，不实现新路由。现有 API 版本、端口、URL、Run 状态机和 SSE envelope 保持不变。契约变更遵循 `AGENTS.md` 第 8 节，不允许下游 Agent 自行修改。
