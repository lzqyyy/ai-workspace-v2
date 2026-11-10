# SSE 事件契约 V1.0.0

- 状态：FROZEN
- 所有者：COORD
- 传输：浏览器 -> C++ `GET /api/v1/runs/{run_id}/events`
- 重放：`Last-Event-ID` 优先，其次 `after` 查询参数
- 编码：UTF-8，`text/event-stream`

## 1. 帧格式

```text
retry: 2000

id: 1
event: run.started
data: {"schema_version":"1.0","request_id":"req-...","run_id":"...","seq":1,"type":"run.started","occurred_at":"2026-11-10T03:00:00Z","payload":{}}

: heartbeat 2026-11-10T03:00:15Z
```

- `id` 是同一 `run_id` 内单调递增的事件序号，从 1 开始。
- `event` 与 `data.type` 必须相同。
- 心跳使用 SSE 注释帧，不占用事件序号，不持久化。
- 服务端每次连接先回放 `seq > cursor` 的事件，再持续轮询并推送新事件。
- 最终事件为 `message.completed`、`run.failed` 或 `run.cancelled` 之一，发送后关闭连接。
- 客户端重连带回最后已处理的事件 ID。重复事件按 `(run_id, seq)` 去重。

## 2. Envelope

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| schema_version | string | 是 | 固定 `1.0` |
| request_id | string | 是 | 触发该事件的请求或 Worker 请求 ID |
| run_id | UUID | 是 | 运行 ID |
| seq | int64 | 是 | 单调递增事件序号 |
| type | string | 是 | 事件类型 |
| occurred_at | RFC3339 | 是 | 服务端写入时间 |
| payload | object | 是 | 类型相关数据 |

## 3. 事件类型

### `run.started`

```json
{
  "schema_version": "1.0",
  "request_id": "req-demo-0001",
  "run_id": "68952f68-b9a3-4d8f-8718-ff0d912be300",
  "seq": 1,
  "type": "run.started",
  "occurred_at": "2026-11-10T03:00:00Z",
  "payload": {"job_id": "d17a2aa3-70b2-4a39-a65e-8d2af28b2c98", "attempt": 1}
}
```

### `message.delta`

```json
{
  "schema_version": "1.0",
  "request_id": "req-demo-0001",
  "run_id": "68952f68-b9a3-4d8f-8718-ff0d912be300",
  "seq": 2,
  "type": "message.delta",
  "occurred_at": "2026-11-10T03:00:01Z",
  "payload": {"delta": "你好，"}
}
```

### `message.completed`

payload 必须含完整最终内容，确保断线后即使不重建中间 delta 也能恢复：

```json
{
  "schema_version": "1.0",
  "request_id": "req-demo-0001",
  "run_id": "68952f68-b9a3-4d8f-8718-ff0d912be300",
  "seq": 5,
  "type": "message.completed",
  "occurred_at": "2026-11-10T03:00:03Z",
  "payload": {
    "message_id": "61eff80d-89e9-45a4-b21c-f7a8ea2236db",
    "content": "你好，我是 Mock Agent。",
    "usage": {"input_tokens": 9, "output_tokens": 8}
  }
}
```

### `run.failed`

```json
{
  "schema_version": "1.0",
  "request_id": "req-demo-0001",
  "run_id": "68952f68-b9a3-4d8f-8718-ff0d912be300",
  "seq": 3,
  "type": "run.failed",
  "occurred_at": "2026-11-10T03:00:02Z",
  "payload": {"error_code": "MODEL_TIMEOUT", "message": "模型响应超时", "retryable": true}
}
```

### `run.cancelled`

```json
{
  "schema_version": "1.0",
  "request_id": "req-demo-0002",
  "run_id": "68952f68-b9a3-4d8f-8718-ff0d912be300",
  "seq": 3,
  "type": "run.cancelled",
  "occurred_at": "2026-11-10T03:00:02Z",
  "payload": {"reason": "user_requested"}
}
```

## 4. 状态机

```text
queued -> running -> succeeded
   |         |     \-> failed
   |         \------> cancelled
   \----------------> cancelled
```

- `queued -> running`：C++ 成功给 Worker 发放租约。
- `running -> succeeded`：Worker 提交最终内容；事务内写 assistant message 和 `message.completed`。
- `running -> failed`：Worker 提交统一错误码，或租约耗尽且达到最大尝试次数。
- `running -> cancelled`：用户取消后 Worker 在下一次心跳发现，并停止产生输出。
- 终态不可回退。重复终态回调返回幂等成功，不新增消息或最终事件。

## 5. 安全与可靠性

- 仅运行所有者可订阅；越权返回 `403 FORBIDDEN`，不返回事件。
- `Last-Event-ID` 缺失时从 `after` 或 0 开始。
- 服务端每 15 秒发送注释心跳，反向代理不得缓冲 SSE。
- 事件先落库后推送；不从 Worker 直达浏览器。
- payload 不记录 API Key、Authorization 头或完整敏感提示词到普通日志。