# ADR-0006：Conversation 级实时 Run 通知

- 状态：Accepted
- 日期：2026-11-20
- 决策者：产品负责人、CORE/COORD

## 背景

跨成员发现新 Run 不能继续只依赖活动会话的 2.5 秒消息轮询；后台标签页的浏览器定时器还可能被节流。现有 Run SSE 负责单个 Run 的事件流，但不能通知客户端出现了一个新的 `run_id`。

## 决策

- 新增 `GET /api/v1/conversations/{conversation_id}/events`，传输仍为 SSE。
- 新增只增表 `conversation_events`，由 C++ Core 在创建或重试 Run 的同一事务写入 `run.available`。
- 事件携带 `conversation_id`、`run_id`、真实 `target_agent_id`、`initiated_by_user_id`、`status`。
- Conversation 事件使用独立游标和 `Last-Event-ID`/`after` 回放；缺失游标时从 tail 开始。
- 客户端收到事件后调用既有 `beginRun()`/`connectRun()`，继续使用 Run SSE envelope。
- 保留 2.5 秒消息轮询，仅作为发现和刷新兜底。

## 不改变

- 单 active Run、Run 状态机、Run SSE envelope、错误码、端口和 C++/Go/PostgreSQL 架构边界。
- Project membership 仍是 Conversation、Run 和 SSE 的统一授权边界。
- Worker 不直接访问 PostgreSQL，也不新增 Redis/Kafka/WebSocket。

## 影响

- OpenAPI 升至 `1.1.0`；SSE 契约升至 `1.1.0`，Run SSE `1.0.0` envelope 不变。
- 数据库迁移新增 `009_conversation_events.sql`。
- 测试必须覆盖事件回放、viewer/非成员授权、新 Run 低延迟发现和真实目标 Agent 归属。
