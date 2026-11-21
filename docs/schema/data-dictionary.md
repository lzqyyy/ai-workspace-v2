# 核心数据字典 V1.1.0

| 表 | 关键字段 | 说明 |
|---|---|---|
| users | id, username, password_hash, status | 用户；密码仅存 PBKDF2 哈希 |
| sessions | token_hash, expires_at, revoked_at | 会话令牌仅存 SHA-256 摘要 |
| model_providers | provider_type, base_url, model_name, secret_ref | API Key 仅存外部 secret_ref 名称 |
| agents | owner_id, system_prompt, model_provider_id | Agent 身份、所有权与模型配置；加入 Project 不改变 owner_id |
| projects | id, name, description, status, created_by_user_id | 最小 Project；状态 `active / archived`，归档为单向软关闭，本版不做硬删除或取消归档 |
| project_members | project_id, user_id, role, added_by_user_id | Project 成员；角色 `owner / editor / viewer`，主键 `(project_id, user_id)` |
| project_agents | project_id, agent_id, added_by_user_id | Project 可用 Agent 映射；不转让 Agent 所有权，历史 Message/Run 在移除后保留 |
| conversations | owner_id, agent_id, project_id | `owner_id` 为创建者；`agent_id` 为历史默认 Agent；权限归属为 Project membership |
| messages | conversation_id, sender_id, seq, content, run_id, client_request_id | 用户消息与最终 Agent 消息；只追加，不编辑、不删除 |
| runs | conversation_id, agent_id, trigger_message_id, initiated_by_user_id, status | `agent_id` 等于发送请求的 `target_agent_id`；`initiated_by_user_id` 为实际发起/重试者；一个 Conversation 最多一个 active Run |
| retry_idempotency | user_id, operation, key, run_id, source_run_id | Retry Idempotency-Key 与首次创建 Run 的映射 |
| jobs | run_id, lease_token, lease_until, attempt_count | PostgreSQL 租约队列 |
| run_events | run_id, seq, event_type, payload_jsonb | Run SSE 回放事实源 |
| conversation_events | conversation_id, seq, event_type, payload_jsonb | Conversation 级 `run.available` 回放事实源；由 C++ 唯一写入 |
| audit_events | action, resource, result, request_id | 不含消息正文和密钥；记录 Project 成员、Agent、归档、权限和运行操作；owner 读取接口仅返回白名单字段 |

## 关键约束

- `messages.client_request_id` 唯一索引为 `(conversation_id, sender_id, client_request_id)`，仅约束用户消息且三者均非空。
- `conversations.project_id` 由 `006` 回填并建索引，由 `007` 收紧为 `NOT NULL`；兼容 C++ 创建路径会先确保 legacy Project、owner 成员和 Project Agent 映射存在。Package C 已完成 Project-first Conversation、消息/SSE 权限和单 active Run 事务。
- `client_request_id` 和 `target_agent_id` 都是 REST 发送消息请求的必填字段；前者按 sender 范围做幂等，后者映射到 `runs.agent_id`。
- `runs.initiated_by_user_id` 由 `008_run_initiated_by.sql` 新增并回填；发送时等于当前发送者，retry 时等于当前重试者。
- `conversation_events` 由 `009_conversation_events.sql` 新增，`(conversation_id, seq)` 唯一；事件在 Run 创建事务内写入，不在 Worker 或数据库触发器中生成。
- Run 的“自己的 Run”权限按 `runs.initiated_by_user_id` 判断，而不是按 `trigger_message_id` 的消息作者推断。
- 不建立 `conversation_members`；Project 成员默认可见全部 Conversation。
- Agent owner 必须是 Project member；只有 owner 自己能把自己的 Agent 加入 Project。
- 一个 Conversation 同时最多一个 `queued/running` Run；Package C 通过 Project + Conversation 行锁、active Run 检查和 409 返回实现该不变量，不依赖裸唯一约束。
