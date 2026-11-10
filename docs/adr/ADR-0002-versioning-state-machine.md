# ADR-0002：契约版本与运行状态机

- 状态：Accepted
- 日期：2026-11-10
- 决策者：COORD

## 决策

- OpenAPI、Protobuf、SSE、错误码当前版本均为 `1.0.0`，兼容性以主版本为准。
- Run 与 Job 共享业务状态集合：`queued -> running -> succeeded | failed | cancelled`。
- 终态不可逆；所有终态写入必须幂等。
- SSE 事件序号按 `run_id` 独立、从 1 单调递增，并先持久化后发送。
- 破坏性变化必须新增 ADR、升级主版本并同时通知 CORE、WORKER、WEB。

## 兼容性

新增可选字段为兼容；删除字段、改变字段含义、改变默认端口或事件名均为不兼容。