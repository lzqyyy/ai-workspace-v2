# A13 测试与验收 V1.0

## 1. 测试层次

| 层 | 命令 | 覆盖 |
|---|---|---|
| 契约 | `./scripts/test-contracts.ps1` | OpenAPI、6 RPC、SSE、错误码、Protobuf |
| Docker 全量 | `./scripts/setup-dev.ps1 -Up` + `./scripts/test.ps1 -Docker` | 契约、Go test/vet、前端检查、E2E；不依赖宿主机 Python/protoc/Go |
| C++ 构建 | `./scripts/build.ps1` | Drogon、libpq、OpenSSL、gRPC 链接 |
| Worker 单元 | `go test ./worker/...` | Mock 流式、提示词、取消 |
| 前端状态 | `node tests/frontend_state_test.js` | Retry 防重复、刷新恢复、SSE 游标与完成态 |
| E2E | `./scripts/e2e.ps1` 或 `./scripts/e2e.ps1 -Docker` | 登录、Agent、会话、发送、SSE、落库、幂等、取消、重试、越权 |
| Lease | `./scripts/test-lease.ps1` | 停止 Worker 后领取 queued Job，错误 lease 被拒绝 |
| Health | `./scripts/test-health.ps1` | PostgreSQL 停机 503、恢复 200 |

## 2. E2E 覆盖

1. 登录 `demo`，确认令牌不出现在 JSON。
2. 创建所有者 Agent 与会话。
3. 发送消息，C++ 返回 202 + `run_id`。
4. Worker 领取并调用 Mock 模型。
5. SSE 严格按 1..N 顺序收到 `run.started`、`message.delta`、`message.completed`。
6. PostgreSQL 中恰好存在 user + assistant 两条消息。
7. 相同 `client_request_id` 重发不重复写消息。
8. 已终态 Run 的 gRPC 回调重复投递两次均返回 duplicate。
9. `after=1` 重放从 seq 2 开始。
10. 第二用户访问会话消息返回 403。
11. 立即取消后 SSE 收到 `run.cancelled`，不写 assistant 消息。
12. 取消后的 Run 可创建新 Run 并成功完成。
13. 非字符串登录、Agent、会话、消息字段返回 400 `VALIDATION_FAILED`。
14. Retry 同一 Idempotency-Key 返回首次 Run，不同 Key 创建新 Run。
15. 两个并发请求使用同一 `client_request_id` 返回同一用户消息、同一 Run，且仅一个 `duplicate=false`。
16. 登录、Agent、会话、消息请求拒绝未知字段；avatar、消息内容与 `client_request_id` 长度边界返回 400 `VALIDATION_FAILED`。
17. send/retry 的 `status` 与 Run 状态枚举一致。

## 3. 工具探测与容器入口

- 宿主脚本会探测 `PYTHON`/`python`/`python3`/`py -3`、`PROTOC`、`.tools/`、Go 与 Node；缺失时给出对应的可直接执行命令。
- `test-contracts.ps1 -Docker` 在临时 Ubuntu 容器中安装 Python/PyYAML/protoc，不改宿主环境。
- `test.ps1 -Docker` 依次执行契约、Go test/vet、前端检查和 E2E；E2E 通过 Compose 内部网络访问 `core:8080`，helper 使用 `GRPC_ADDR=core:50051`。

## 4. 专项测试

- `./scripts/test-lease.ps1`：停止 Worker，使用 gRPC 测试程序领取 queued Job，先以错误 lease 调 `UpdateJobStatus` 并断言 `JOB_LEASE_INVALID`，再用正确 lease 取消清理；随后领取另一 Job、通过 REST 请求取消并等待 lease 过期，断言终态为 `cancelled`、无 `MODEL_TIMEOUT` 且无 assistant 消息；最后恢复 Worker。
- `./scripts/test-health.ps1`：在 Compose 环境中停/启 PostgreSQL，验证 `/api/v1/health` 的状态码和响应体。

## 5. 手动验收

- 浏览器登录、创建 Agent、创建会话、发送消息，观察流式回复。
- 运行中取消，页面显示已取消，不继续增长。
- SSE 网络断开后恢复，页面补齐事件且不重复最终消息。
- 刷新页面，历史消息仍在；未完成 Run 恢复订阅。
- 在窄屏宽度下可打开侧栏并完成发送。
- 浏览器级自动检查：登录、会话加载、提交消息、SSE 完成，且模型回复包含本次用户输入。
- 截图证据：`docs/evidence/ui-login.png`、`ui-workspace.png`、`ui-completed.png`。

## 6. 通过记录

```text
contract validation: PASS
C++ workspace_server: built successfully
Go worker tests: PASS
E2E: PASS
duplicate_callback: PASS
wrong_lease: PASS
sse_replay: PASS
cross_user_denied: PASS
cancel_and_retry: PASS
concurrent_send_idempotency: PASS
unknown_fields_and_lengths: PASS
cancel_then_lease_expiry: PASS
frontend_retry_refresh_reconnect: PASS
health_503_and_recovery: PASS
browser ui check: PASS
```
