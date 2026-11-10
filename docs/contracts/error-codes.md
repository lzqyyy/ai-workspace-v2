# 统一错误码 V1.0.0

- 状态：FROZEN
- 所有者：COORD
- REST 错误统一格式：`{"code","message","request_id","retryable","details?"}`

| 错误码 | HTTP | 可重试 | 用户提示 | 语义 |
|---|---:|---:|---|---|
| `AUTH_REQUIRED` | 401 | 否 | 请先登录 | 缺少或过期会话 |
| `AUTH_INVALID` | 401 | 否 | 用户名或密码错误 | 登录凭据错误 |
| `VALIDATION_FAILED` | 400 | 否 | 输入内容不完整或格式错误 | 参数校验失败 |
| `FORBIDDEN` | 403 | 否 | 无权访问该资源 | 资源不属于当前用户 |
| `NOT_FOUND` | 404 | 否 | 请求的资源不存在 | 不存在或避免枚举 |
| `CONFLICT` | 409 | 视情况 | 当前操作发生冲突 | 幂等键冲突或状态冲突 |
| `RUN_NOT_CANCELLABLE` | 409 | 否 | 当前运行已结束，无法取消 | 终态取消 |
| `JOB_LEASE_INVALID` | 409 | 是 | 任务租约已失效 | Worker 租约不匹配或过期 |
| `MODEL_TIMEOUT` | 200/SSE | 是 | 模型响应超时，请重试 | Provider 超时 |
| `MODEL_PROVIDER_ERROR` | 200/SSE | 是 | 模型服务暂时不可用，请重试 | Provider 4xx/5xx/网络错误归一化 |
| `RUN_CANCELLED` | 200/SSE | 否 | 运行已取消 | 用户或系统取消 |
| `RATE_LIMITED` | 429 | 是 | 请求过于频繁，请稍后重试 | 限流 |
| `INTERNAL` | 500 | 是 | 服务暂时不可用，请稍后重试 | 未分类内部错误 |

## 边界规则

- 禁止把 Provider 原始错误正文、请求头、密钥或完整用户消息写入响应和普通日志。
- Worker 将 Provider 错误映射为上述错误码；C++ 只接受白名单错误码。
- `request_id` 必须原样进入审计事件和错误响应。缺少请求 ID 的 API 调用按 `VALIDATION_FAILED` 处理。
- 重试必须创建新的 `run_id`；同一用户消息的重复发送通过 `client_request_id` 幂等返回原 `run_id`。
- 失败重试不得自动无限循环；Job 默认 `max_attempts=3`，仅可重试错误自动回收。