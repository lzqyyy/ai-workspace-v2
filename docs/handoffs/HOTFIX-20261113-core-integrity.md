# Handoff: HOTFIX-20261113 Core 一致性与健康检查

- 状态：DONE
- 主责 Agent：CORE / WEB / QA
- 分支 / 提交：main / b093cbe + 本次新的本地提交（提交内容见最终报告）
- 开始时间：2026-11-13
- 完成时间：2026-11-13

## 目标

修复 S1 lease 校验、S2 retry Idempotency-Key、A1 JSON 字段类型校验、F1 会话切换 Run/SSE 状态、O1 数据库不可用健康检查状态码；保持现有 C++/Go/PostgreSQL/gRPC/SSE 架构。

## 根因

- S1：`updateJobStatus()` 只按 job/run 查行，未校验 job 必须为 `running`、lease token 必须匹配、lease 未过期；错误回调可提前写入终态。
- S2：REST retry 路由读取了 `Idempotency-Key` 但没有传入服务层，数据库也没有保存 retry key 与首次 Run 的映射。
- A1：登录、Agent、会话和消息直接调用 nlohmann `value()`，非字符串 JSON 抛类型异常后被通用 `catch (...)` 映射为 500。
- F1：切换会话时只关闭 EventSource，却保留 `state.run`；随后代码手动重新启用输入框，而发送函数仍因 `state.run` 直接返回，造成“可点但无效”，切回时也不会重连 SSE。
- O1：health 路由无论数据库状态如何都固定返回 HTTP 200。

## 完成内容

- S1：在 `updateJobStatus()` 的终态幂等分支之后增加 lease 校验：job `running`、token 完全匹配、`lease_until > now()`；终态 Run 的重复回调仍返回 accepted+duplicate。
- S2：新增 `retry_idempotency` 增量迁移；同一用户、同一 `Idempotency-Key` 返回首次 retry Run，不同 key 创建新 Run；并发冲突通过唯一键 + 回滚重查处理。
- A1：新增统一 `json_string()` 字段解析；登录、创建 Agent、创建会话、发送消息的非字符串字段返回 400 `VALIDATION_FAILED`，不改变真实内部异常的 500 语义。
- F1：会话切换时关闭旧 SSE 但保留当前 Run；运行中会话被切回时重新订阅并回放；运行期间其他会话明确禁用输入和发送并显示“另一会话运行中”，避免输入可点但发送无效。
- O1：health 在数据库不可用时返回 HTTP 503，仍保持原 URL、响应字段和 Compose healthcheck 命令不变。
- 增加错误 lease、retry 幂等、JSON 类型、health 停库专项测试和浏览器级会话切换验证。

## 修改文件

- `server/src/common.hpp`
- `server/src/core.hpp`
- `server/src/core.cpp`
- `server/src/app.cpp`
- `frontend/app.js`
- `db/migrations/005_retry_idempotency.sql`
- `api/openapi.yaml`
- `tests/e2e.py`
- `tests/lease_guard.py`
- `tests/duplicate_callback/main.go`
- `tests/validate_contracts.py`
- `scripts/test-health.ps1`
- `scripts/test-lease.ps1`
- `docs/contracts/error-codes.md`
- `docs/schema/data-dictionary.md`
- `docs/schema/erd.md`
- `docs/runbook/A13-testing.md`
- `docs/runbook/A14-operations.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/HOTFIX-20261113-core-integrity.md`

## 契约变更

- REST：retry `Idempotency-Key` 行为明确；health 新增 HTTP 503 响应。
- DB：新增只增迁移 `005_retry_idempotency`，新增 `retry_idempotency` 映射表。
- gRPC、SSE、端口、C++ 单写者、Worker 仅 gRPC 访问等架构边界未改变。

## 验证命令与结果

```text
git fetch origin --prune
FAIL：当前环境无法解析 github.com；本地 HEAD 未改变。

docker compose --env-file .env -f deploy/docker-compose.yml config -q
PASS

docker compose --env-file .env -f deploy/docker-compose.yml build core worker
PASS：Core 和 Worker 镜像构建成功。

go test -buildvcs=false ./worker/...
PASS（Go 1.25 临时容器）
ok ai-workspace-v2/worker/cmd/worker

go vet ./worker/...
PASS（同一 Go 1.25 临时容器）

tests/validate_contracts.py
PASS：13 OpenAPI operations / 6 RPC / 5 SSE events。

protoc descriptor contract compile
PASS。

tests/e2e.py
PASS：登录、类型校验、Agent、会话、发送、SSE、落库、重复回调、回放、越权、retry 同 key/不同 key、取消与重试。

scripts/test-lease.ps1
PASS：错误 lease 返回 JOB_LEASE_INVALID，测试 Run 被正确取消，Worker 恢复运行。

scripts/test-health.ps1
PASS：PostgreSQL 停机时 health 返回 503 degraded/unavailable；恢复后返回 200。

frontend/app.js node --check
PASS。

浏览器级 F1 验证
PASS：运行中切换到另一会话显示“另一会话运行中”且输入/发送禁用；切回原会话后 SSE 恢复并显示“已完成”，模型回复包含本次输入。
```

## 验收映射

- S1：错误 lease 被拒绝；终态重复回调继续 accepted+duplicate。
- S2：同 key 返回原 retry Run；完成后再重复同 key 仍返回原 Run；不同 key 创建新 Run。
- A1：登录 username/password、Agent 字段、会话 agent_id/title、消息 content/client_request_id 非字符串均返回 400。
- F1：切换期间输入状态与发送守卫一致；切回运行中会话后 SSE 自动恢复。
- O1：数据库不可用时 health 为 503，恢复后为 200。

## 已知问题与风险

- `git fetch` 因当前环境 DNS/网络失败；按指令未 push，远端 main 仍停在 `03ecffd`。
- 本地 Docker daemon 需 elevated 权限访问；E2E 使用临时 Python 容器和 Linux gRPC helper，未提交临时文件。
- `scripts/test-lease.ps1` 假定 `build/duplicate-callback-linux` 已构建；构建步骤见 `scripts/build.ps1` 的等价 Go helper 构建流程或本交接验证记录。
- 并发 retry 由数据库唯一键兜底；重复请求会回滚本次新 Run 后重查首次 Run，不新增第二个 Run。

## 需要其他 Agent 处理的事项

- 网络恢复后可执行正常 `git push`；不要 amend `b093cbe`。
- Docker Desktop 权限应配置为普通开发用户可用，避免每次测试请求 elevated daemon 访问。

## 下一任务建议

- 在 CI 中把 lease/health 专项脚本纳入 Compose 验收。
- 后续若支持多会话并行 Run，需要把前端单 Run 状态改为每会话 Run 映射；本次按要求保持单 Run 最小修复。
