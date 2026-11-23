# SSE 事件契约 V1.1.0

- 状态：FROZEN
- 所有者：COORD
- 传输：浏览器 -> C++ `GET /api/v1/runs/{run_id}/events`（Run SSE，envelope 不变）\n- Conversation 发现：浏览器 -> C++ `GET /api/v1/conversations/{conversation_id}/events`
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

- Run 所属 Conversation 的 Project 成员可订阅 Run SSE；viewer 只读。非成员或无角色权限返回 `403 FORBIDDEN`，不返回事件。
- Run SSE 的 `Last-Event-ID` 缺失时从 `after` 或 0 开始。
- 服务端每 15 秒发送注释心跳，反向代理不得缓冲 SSE。
- 事件先落库后推送；不从 Worker 直达浏览器。
- 所有 Project 成员读取同一 Run 的事件序列；Run SSE URL、envelope 和状态机不变。
- payload 不记录 API Key、Authorization 头或完整敏感提示词到普通日志。

## 6. Conversation 事件流 V1.0

Conversation 事件流只负责通知“新 Run 可用”。客户端收到 `run.available` 后，必须使用其中的 `run_id` 订阅既有 Run SSE；不得把 Conversation 事件当作模型流。

### 6.1 帧与游标

```text
retry: 2000

id: 1
event: run.available
data: {"schema_version":"1.0","request_id":"req-...","conversation_id":"...","seq":1,"type":"run.available","occurred_at":"2026-11-20T03:00:00Z","payload":{"run_id":"...","target_agent_id":"...","initiated_by_user_id":"...","status":"queued"}}
```

- `id` 是同一 `conversation_id` 内单调递增的事件序号，从 1 开始。
- `event` 与 `data.type` 必须相同。
- 心跳使用 SSE 注释帧，不占用事件序号，不持久化。
- `Last-Event-ID` 优先，其次 `after`；两者都缺失时从当前 tail 开始，避免首次加载重放历史运行。
- 显式传入 `Last-Event-ID` 或 `after` 时，服务端回放 `seq > cursor` 的持久化事件。
- 单次查询最多返回 200 条事件；客户端继续按游标增量读取。

### 6.2 `run.available`

新建或重试 Run 时，C++ Core 在创建 Message/Run/Job 的同一事务内写入事件。幂等重复发送、重试重复键和 active Run 冲突不得新增事件。

Payload 必填字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `run_id` | UUID | 用于订阅既有 Run SSE |
| `target_agent_id` | UUID | 该 Run 的真实目标 Agent |
| `initiated_by_user_id` | UUID | 实际创建或重试该 Run 的用户 |
| `status` | string | 创建时为 `queued` |

### 6.3 授权与边界

- Conversation 所属 Project 的 owner/editor/viewer 均可订阅；viewer 只读。
- 非 Project 成员返回 `403 FORBIDDEN`，不建立事件流。
- Conversation 事件由 C++ Core 唯一写入；Worker 不直接写 PostgreSQL。
- 本事件流不改变单 active Run、Run 状态机、Run SSE envelope、端口或 C++/Go/PostgreSQL 架构边界。
