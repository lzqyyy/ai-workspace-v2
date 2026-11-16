# A04-A06 契约映射与校验入口

- A04 前后端 REST API：`api/openapi.yaml`。
- A05 C++ / Go gRPC 接口：`proto/workspace.proto`。
- A06 数据库设计：`docs/schema/erd.md` 与 `db/migrations/*.sql`。
- SSE 契约：`docs/contracts/sse-events.md`。
- 统一错误码：`docs/contracts/error-codes.md`。
- Project 多人基础契约：`docs/adr/ADR-0004-project-membership.md`、`db/migrations/006_multiplayer_foundation.sql`。

执行 `./scripts/test-contracts.ps1` 验证 OpenAPI 版本、既有与 Project-first 接口集合、Project schema/角色、`target_agent_id`、迁移关键对象、错误码枚举、SSE 事件集合、6 个 RPC 和 Protobuf 编译。
