# A13 测试与验收 V1.0

## 1. 测试层次

| 层 | 命令 | 覆盖 |
|---|---|---|
| 契约 | `./scripts/test-contracts.ps1` | OpenAPI、6 RPC、SSE、错误码、Protobuf |
| 连接池 | `./scripts/test-pool.ps1 -Docker` | 256 请求/32 并发、连接上限、idle/active 归还、PostgreSQL 重启恢复、connect_timeout |
| Docker 全量 | `./scripts/setup-dev.ps1 -Up` + `./scripts/test.ps1 -Docker` | 契约、Go test/vet、前端检查、连接池、单用户 E2E、2H+2A E2E、Project fixture、迁移重放；不依赖宿主机 Python/protoc/Go |
| C++ 构建 | `./scripts/build.ps1` | Drogon、libpq、OpenSSL、gRPC 链接 |
| Worker 单元 | `go test ./worker/...` | Mock 流式、提示词、取消 |
| 前端状态 | `node tests/frontend_state_test.js` | 角色控件、Project 路由、target Agent、Retry 防重、刷新恢复、Conversation `run.available`、共享 Run 发现、SSE 游标/去重 |
| E2E | `./scripts/e2e.ps1` 或 `./scripts/e2e.ps1 -Docker` | 登录、Agent、会话、发送、Run/Conversation SSE、落库、幂等、取消、重试、越权、2H+2A |
| Conversation 实时专项 | `python tests/conversation_realtime.py` | `run.available` 字段/低延迟、retry 事件、幂等不重复事件、显式回放、viewer 订阅、非成员 403 |
| 浏览器 E2E | `./scripts/frontend-e2e.ps1` | 真实 Edge/Chrome 登录、Project-first 单用户发送与流式完成 |
| 2H+2A 浏览器 E2E | `./scripts/frontend-2h2a-e2e.ps1` | 双浏览器上下文、双人类、双 Agent、共享历史、双方 Conversation/Run SSE、真实目标 Agent、viewer 只读 |
| 迁移重放 | `./scripts/test-migrations.ps1 -Docker` | project_id NOT NULL、007/008/009/010 唯一迁移行、Core 重启重新执行迁移 |
| 查询完整性 | `./scripts/test-query-integrity.ps1 -Docker` | 单 active Run 唯一索引、消息/RunEvent 增量查询 EXPLAIN、关键索引存在 |
| Lease | `./scripts/test-lease.ps1` | 停止 Worker 后领取 queued Job，错误 lease 被拒绝 |
| 事件序号 | `./scripts/test-lease.ps1` | 跳号/乱序被拒绝，重复事件仍幂等 |
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
18. Project-first Conversation 创建/列表和 Project member Message 读取。
19. 2H+2A：owner/editor、两个 Agent、同一共享 Conversation、双方读取完整历史并回放 SSE。
20. 不同用户使用相同 `client_request_id` 不冲突；同用户重复发送返回原 Run。
21. viewer 可读/可订阅但不能发送、取消或重试；非成员访问 Conversation/Run/SSE 返回 403。
22. active Run fixture 下发送返回 409；不同 key 的 retry 在 active Run 期间返回 409。
23. Agent owner 被移出 Project 后，即使 `project_agents` 历史映射仍存在，也不能再执行该 Agent。
24. 非法 `target_agent_id` 格式返回 400 `VALIDATION_FAILED`。
25. owner/editor 并发向同一 Conversation 发送时，一个请求成功、另一个返回 409 `CONFLICT`。
26. 消息历史 `limit` 默认/最大边界、`after_seq` 分页、`has_more` 与 `next_after_seq` 通过；非法 limit/after_seq 返回 400。

## 3. 工具探测与容器入口

- 宿主脚本会探测 `PYTHON`/`python`/`python3`/`py -3`、`PROTOC`、`.tools/`、Go 与 Node；缺失时给出对应的可直接执行命令。
- `test-contracts.ps1 -Docker` 使用 `deploy/contract-tools.Dockerfile` 构建并复用本地 `ai-workspace-v2-contract-tools:local` 镜像，内含 Python/PyYAML/protoc，不改宿主环境且无需每次 `apt-get`。
- `test.ps1 -Docker` 依次执行契约、Go test/vet、前端检查和 E2E；Go 测试复用 `ai-workspace-v2-go-mod` 命名卷，E2E 通过 Compose 内部网络访问 `core:8080`，helper 使用 `GRPC_ADDR=core:50051`。

## 4. 专项测试

- `./scripts/test-lease.ps1`：停止 Worker，使用 gRPC 测试程序领取 queued Job，先以错误 lease 调 `UpdateJobStatus` 并断言 `JOB_LEASE_INVALID`，再用正确 lease 取消清理；随后领取另一 Job、通过 REST 请求取消并等待 lease 过期，断言终态为 `cancelled`、无 `MODEL_TIMEOUT` 且无 assistant 消息；最后恢复 Worker。
- `./scripts/test-health.ps1`：在 Compose 环境中停/启 PostgreSQL，验证 `/api/v1/health` 的状态码和响应体。
- `tests/event_seq_guard.py`：停止 Worker 后领取 queued Job，验证 `event_seq` 跳号/乱序返回 `CONFLICT`，同一事件重复投递仍返回 `duplicate=true`，最后取消 Run。

## 5. 手动验收

- 浏览器登录、创建 Agent、创建会话、发送消息，观察流式回复。
- 运行中取消，页面显示已取消，不继续增长。
- SSE 网络断开后恢复，页面补齐事件且不重复最终消息。
- 刷新页面，历史消息仍在；未完成 Run 恢复订阅。
- 在窄屏宽度下可打开侧栏；选择会话后侧栏自动关闭，并可完成发送。
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
shared_conversation_2h2a: PASS
viewer_read_only: PASS
non_member_denied: PASS
active_run_conflict_409: PASS
removed_agent_owner_denied: PASS
cancel_and_retry: PASS
concurrent_send_idempotency: PASS
message_pagination: PASS
query_integrity: PASS
active_run_constraint: PASS
unknown_fields_and_lengths: PASS
cancel_then_lease_expiry: PASS
frontend_retry_refresh_reconnect: PASS
health_503_and_recovery: PASS
browser ui check: PASS
frontend_browser_project_first: PASS
frontend_browser_2h2a: PASS
viewer_browser_readonly: PASS
invalid_target_validation: PASS
concurrent_multiuser_single_active: PASS
migration_restart_replay: PASS
```
