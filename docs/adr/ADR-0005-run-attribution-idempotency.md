# ADR-0005：Run 归属、发送幂等与并发控制

- 状态：Accepted
- 日期：2026-11-19；增量确认：2026-11-26
- 决策者：产品负责人、CORE/COORD
- 依据：`多人平台的最小mvp_PRD.md` v1.1-frozen、`ADR-0004-project-membership.md`

## 决策

### 1. 基线

采用 `1A`：以已验收的多人 MVP 分支为基线，同步最新 `main` 后继续演进，不从单用户基线重做。

### 2. 数据模型

采用 `2A`：继续使用 `projects`、`project_members`、`project_agents`。本阶段不引入 `resource_acl`；等出现真实的资源级动作授权需求后再设计。

### 3. Run 归属

采用 `3B`：新增 `runs.initiated_by_user_id`。

- `trigger_message_id` 保留原始触发消息。
- 新发送消息时，`initiated_by_user_id = 发送者`。
- retry 时，新 Run 的 `initiated_by_user_id = 当前重试者`。
- owner/editor 的“自己的 Run”权限按 `initiated_by_user_id` 判断。
- `008_run_initiated_by.sql` 回填历史 Run。

### 4. 发送幂等

采用 `4A`：消息表唯一索引 `(conversation_id, sender_id, client_request_id)` 配合 `ON CONFLICT DO NOTHING` 和返回原结果。

- `client_request_id` 收紧为 REST 必填字段。
- 同用户同 key 重复请求返回原消息和原 Run。
- 不同用户同 key 互不影响。
- 同 key 不同正文仍按同一逻辑请求处理；需要参数指纹校验时再升级为通用幂等表。

### 5. 并发控制

采用 `5A`：继续使用 Project + Conversation 悲观行锁，检查 active Run 并返回 409。当前接受同一 Project 写路径串行化的吞吐上限。

- `5B` 登记为后续任务 `L2-08`：Conversation 级锁 + membership/Agent 授权行共享锁。
- 2026-11-26 增量确认：`5A` 继续作为主方案；`010_single_active_run.sql` 增加 `5C` partial unique index 作为数据库兜底，并把唯一冲突映射为 409。

### 6. 审计

采用 `6A`：业务写入与 `audit_events` 在同一 PostgreSQL 事务提交，审计不记录消息正文或密钥。

### 7. 开源复用

采用 `7A`：参考官方规范与成熟开源项目的设计模式，不 fork 整个外部聊天平台。只在许可证兼容、记录来源和提交、并通过本项目测试的前提下复用局部代码。

## 影响

- REST 契约版本升级为 `1.0.1`，URL、端口、SSE envelope 和 Run 状态机不变。
- 数据库从迁移 `007` 增至 `008`；2026-11-26 再增至 `010_single_active_run`。
- 新增/修改的测试必须覆盖：`client_request_id` 缺失、header/body 不一致、不同用户同 key、owner retry editor Run 后的归属、2H+2A 与迁移重放。
