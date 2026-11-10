# 冻结契约索引

版本：`1.0.0`，冻结日期：2026-11-10。

| 契约 | 文件 | 校验 |
|---|---|---|
| REST | `api/openapi.yaml` | `python tests/validate_contracts.py` |
| gRPC | `proto/workspace.proto` | `protoc --descriptor_set_out` + Go/C++ stub generation |
| SSE | `docs/contracts/sse-events.md` | 同一次 E2E 校验 `run.started`、`message.delta`、`message.completed` |
| 错误码 | `docs/contracts/error-codes.md` | 契约测试校验 OpenAPI 枚举与文档一致 |

契约变更遵循 `AGENTS.md` 第 13 节，不允许下游 Agent 自行修改。