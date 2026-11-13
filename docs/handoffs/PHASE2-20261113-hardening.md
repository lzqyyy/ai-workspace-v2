# Handoff: PHASE2-20261113 阶段二边界与稳定性修补

- 状态：DONE
- 主责 Agent：CORE / WEB / QA
- 分支 / 提交：main / 5fc38c9 + 本次新的本地提交（最终报告回填 commit hash）
- 开始时间：2026-11-13
- 完成时间：2026-11-13

## 目标

修复阶段二边界与稳定性问题：sendMessage 并发幂等、取消后的 lease 过期优先级、请求未知字段与长度校验、send/retry 响应状态契约、前端 retry 防重复与恢复一致性。保持 C++ Drogon Core、Go Worker、PostgreSQL jobs/lease、REST/SSE + gRPC 架构不变。

## 根因

- S3：`sendMessage()` 在事务外先查 `client_request_id`，两个并发请求都可能通过预检；后提交者触发 `ux_messages_client_request` 唯一索引冲突，被数据库层映射为 500。
- S5：`claimNextJob()` 回收过期 running Job 时没有读取 `runs.cancel_requested`，会先按 `attempt_count/max_attempts` 重新排队或写成 `failed/MODEL_TIMEOUT`，取消信号被 lease 过期路径覆盖。
- A2/A3：REST 层只解析已知字段，没有执行 OpenAPI 的 `additionalProperties: false`；avatar 仅校验类型未校验 500000 长度；body 中的短 `client_request_id` 只检查了上限。
- A5：send/retry 的幂等重复响应返回实际 Run 状态，而 OpenAPI 只允许 `queued/running`；当原 Run 已终态时契约不一致。
- F2：retry 没有 single-flight 守卫，每次点击都会生成新的 `Idempotency-Key`，双击可能创建多个 Run。
- F3：刷新后若最终 assistant 消息已存在，只清除本地暂存而未显式恢复“已完成”状态；SSE 游标恢复逻辑本身可复用。

## 完成内容

- S3：消息插入使用现有部分唯一索引配合 `ON CONFLICT ... DO NOTHING`；冲突后在同一事务可见快照读取原 user message + Run，稳定返回同一 `message.id`、同一 `run_id` 和 `duplicate=true`。
- S5：lease 回收先检查 `cancel_requested`；已取消 Run 无论尝试次数均终结为 `cancelled`、写一次 `run.cancelled`，不重新执行、不写 `run.failed/MODEL_TIMEOUT`。
- A2/A3：登录、创建 Agent、创建会话、发送消息统一拒绝未知字段；avatar 增加 500000 长度限制；`client_request_id` 强制 8-100 长度；非法输入返回 400 `VALIDATION_FAILED`。
- A5：将 `SendMessageResponse.status` 对齐现有 Run 状态枚举，允许幂等重复请求返回终态，不新增业务状态。
- F2/F3：retry 请求期间设置 `state.retrying` 并禁用按钮；完成态刷新显式显示“已完成”；新增 Node VM 状态测试覆盖 retry 去重、刷新恢复、SSE 游标与完成态。
- T1：增加并发 send、cancel 后 lease 过期、未知字段/长度、retry 防重复、刷新/重连状态回归测试。
- 更新风险与测试 Runbook、TASK_BOARD 和本交接记录。

## 修改文件

- `server/src/common.hpp`
- `server/src/app.cpp`
- `server/src/core.cpp`
- `frontend/app.js`
- `api/openapi.yaml`
- `tests/e2e.py`
- `tests/validate_contracts.py`
- `tests/duplicate_callback/main.go`
- `tests/cancel_lease_expiry.py`
- `tests/frontend_state_test.js`
- `scripts/test-lease.ps1`
- `scripts/test.ps1`
- `docs/runbook/A12-reliability.md`
- `docs/runbook/A13-testing.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PHASE2-20261113-hardening.md`

## 契约变更

- REST：`SendMessageResponse.status` 从 `queued/running` 扩展为完整既有 Run 状态集合，用于幂等重复请求返回终态；字段名和响应结构不变。
- OpenAPI 请求边界保持原字段；服务端开始严格执行 `additionalProperties: false` 与 avatar/client_request_id 长度。
- gRPC、SSE、端口、数据库模式、C++ 单写者、Worker 仅 gRPC 访问等架构边界未改变。

## 验证命令与结果

```text
git status --short --branch
PASS：开始前 main...origin/main [ahead 2]，工作区干净。

git fetch origin --prune
FAIL：当前环境 DNS 无法解析 github.com；本地 HEAD 未改变。

git diff --check
PASS。

docker compose --env-file .env -f deploy/docker-compose.yml config -q
PASS。

docker compose --env-file .env -f deploy/docker-compose.yml build core worker
PASS：Core 与 Worker 镜像构建成功。

go test -buildvcs=false ./worker/...
PASS：worker/cmd/worker 通过。

go vet ./worker/...
PASS。

python tests/validate_contracts.py
PASS：13 OpenAPI operations / 6 RPC / 5 SSE events；新增 unknown-field、avatar length、send/retry status 契约断言。

protoc descriptor contract compile
PASS：libprotoc 3.21.12。

tests/e2e.py
PASS：完整 E2E、并发相同 client_request_id、未知字段/长度、重复回调、回放、越权、retry、取消与重试全部通过。

scripts/test-lease.ps1
PASS：wrong lease 返回 JOB_LEASE_INVALID；取消后 lease 过期终态为 run.cancelled，无 MODEL_TIMEOUT、无 assistant 消息。

node --check frontend/app.js
PASS。

node tests/frontend_state_test.js
PASS：retry_dedupe=true, refresh_restore=true, sse_reconnect=true, completed_state=true。
```

## 验收映射

- S3：并发回归断言同一 message id、同一 run_id，重复标记集合为 `{false, true}`，会话最终恰好 user + assistant 两条消息。
- S5：取消后 lease 过期专项断言 `run.cancelled` 且无 `run.failed`、无 assistant 消息。
- A2/A3：登录、Agent、会话、消息未知字段均 400；avatar 500001、content 32001、短 client_request_id 均 400 `VALIDATION_FAILED`。
- A5：契约校验断言 SendMessageResponse 状态集合与 Run 状态集合一致。
- F2/F3：前端测试断言双击 retry 仅发一次请求、按钮请求期间禁用、完成态刷新清暂存且显示已完成、SSE 从持久化游标恢复。

## 已知问题与风险

- `git fetch` 因当前环境 DNS/网络失败；按指令未 push，本地提交将在最终报告给出。
- 前端新增验证为 Node VM 级状态测试，未重复执行真实浏览器视觉验收；REST/SSE 契约链路由完整 E2E 覆盖。
- Docker 测试使用临时 Python/Go/Protobuf 容器与 `build/` 下 helper，均未提交进 Git。
- 仍保持单 Run 模型；运行中其他会话禁止发送的行为未改变。

## 需要其他 Agent 处理的事项

- 网络恢复后可正常 fetch；不要 reset、force push 或 amend `5fc38c9`。
- 后续多 Run 并行需要单独设计每会话 Run 映射，不属于本阶段。
