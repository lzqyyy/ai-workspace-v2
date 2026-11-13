# A12 错误、重试、超时与幂等 V1.0

## 1. 失败分类

| 错误 | 可重试 | 处理 |
|---|---|---|
| `MODEL_TIMEOUT` | 是 | 未达 max_attempts 自动回队；达到上限终态失败 |
| `MODEL_PROVIDER_ERROR` | 是 | 统一错误码，不保存 Provider 原始响应 |
| `INTERNAL` | 是 | C++ 重启后可重试 |
| `RUN_CANCELLED` | 否 | 终态取消 |
| `JOB_LEASE_INVALID` | 是 | Worker 停止本次执行，等待 Job 回收 |

## 2. 租约回收

- Job 领取时 `attempt_count += 1`，获得随机 `lease_token`。
- Worker 每 2 秒续租 20 秒；C++ 默认租约 30 秒。
- Claim 查询同时检查 queued 和 `running + lease_until < now()`。
- 过期未达最大尝试：Job/Run 回到 queued。
- 过期达到最大尝试：Job/Run 终态 failed，写一次 `run.failed`。

## 3. 幂等点

- 发送消息：`client_request_id` 或 `Idempotency-Key` 在会话内唯一；重复请求返回原 `run_id`。
- 最终消息：`(run_id)` partial unique + `ON CONFLICT DO NOTHING`。
- 终态回调：已终态 Run 对相同 Job/Run 再次 UpdateJobStatus 返回 `accepted=true, duplicate=true`，不新增消息或事件。
- Worker 事件：`(run_id, seq)` 唯一；相同内容视为 duplicate，不同内容返回冲突。
- 事件游标：客户端用 `Last-Event-ID`/`after` 重放，不重复执行模型。

## 4. 取消

- queued：C++ 立即将 Job/Run 置 cancelled 并写 `run.cancelled`。
- running：C++ 设置 `cancel_requested`；Worker 在下一次心跳或 AppendEvent 被拒绝时停止，并提交 cancelled。
- 取消后不允许再写 message.delta。
- `cancel_requested=true` 优先于 lease 过期与重试上限：Worker 崩溃后 Job 不再重新执行，也不写 `run.failed/MODEL_TIMEOUT`，而是终结为 `cancelled` 并写一次 `run.cancelled`。

## 5. 前端恢复

- 每次 delta 更新本地 run 游标和部分内容。
- EventSource 断线自动重连，并携带 `Last-Event-ID`。
- 页面刷新从 localStorage 恢复 `run_id + lastEventId + partial content`。
- 若历史中已有该 Run 的最终消息，则清除暂存，不重复订阅，并显示已完成状态。
- Retry 请求为前端 single-flight：请求期间禁用重试按钮，避免双击创建多个 Run。