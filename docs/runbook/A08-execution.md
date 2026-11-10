# A08 Agent 执行流程 V1.0

## 1. 状态机

```text
queued -> running -> succeeded
   |         |     \-> failed
   |         \------> cancelled
   \----------------> cancelled
```

- Run 与 Job 同步维护业务状态。
- 终态不可回退。
- `cancel_requested=true` 后 Worker 在下一次心跳或 AppendEvent 时停止。
- Job 租约默认 30 秒，Worker 每 2 秒续租至 20 秒。
- 租约过期后：未达 `max_attempts` 则重新排队；达到上限则 Run 失败为 `MODEL_TIMEOUT`。

## 2. 创建事务

`POST /conversations/{id}/messages` 在单个事务中完成：

1. 鉴权并锁定会话。
2. 写 user message。
3. 写 Run（queued）。
4. 将 user message 关联 run_id。
5. 写 Job（queued）。
6. 写审计事件。

任何一步失败全部回滚，不产生“有消息但无任务”的半成品。

## 3. Worker 执行

1. gRPC `ClaimNextJob` 使用 `FOR UPDATE SKIP LOCKED` 领取 Job。
2. C++ 写 `run.started`。
3. Worker 调用 `GetExecutionContext`，只获得当前会话历史与 Agent 配置。
4. 模型流式片段逐条 `AppendEvent(message.delta)`。
5. Worker `UpdateJobStatus` 提交成功内容或统一错误码。
6. C++ 事务内幂等写最终 assistant message、终态事件和 Run 状态。
7. Browser 通过 SSE 先回放已持久化事件，再接收实时事件。

## 4. SSE 序号

- 每个 Run 独立从 1 递增。
- 建 Run 后首个持久化事件通常是 `run.started`。
- `Last-Event-ID` 或 `after` 之后的持久化事件按数值升序回放。
- 最终事件发送后关闭流；浏览器刷新可根据 `run_id` 与会话历史恢复。