# 核心数据字典 V1.1.0

| 表 | 关键字段 | 说明 |
|---|---|---|
| users | id, username, password_hash, status | 用户；密码仅存 PBKDF2 哈希 |
| sessions | token_hash, expires_at, revoked_at | 会话令牌仅存 SHA-256 摘要 |
| model_providers | provider_type, base_url, model_name, secret_ref | API Key 仅存外部 secret_ref 名称 |
| agents | owner_id, system_prompt, model_provider_id | Agent 身份、所有权与模型配置；加入 Project 不改变 owner_id |
| projects | id, name, description, status, created_by_user_id | 最小 Project；状态 `active / archived`，本版不做硬删除 |
| project_members | project_id, user_id, role, added_by_user_id | Project 成员；角色 `owner / editor / viewer`，主键 `(project_id, user_id)` |
| project_agents | project_id, agent_id, added_by_user_id | Project 可用 Agent 映射；不转让 Agent 所有权，历史 Message/Run 在移除后保留 |
| conversations | owner_id, agent_id, project_id | `owner_id` 为创建者；`agent_id` 为历史默认 Agent；权限归属为 Project membership |
| messages | conversation_id, sender_id, seq, content, run_id, client_request_id | 用户消息与最终 Agent 消息；只追加，不编辑、不删除 |
| runs | conversation_id, agent_id, trigger_message_id, status | `agent_id` 等于发送请求的 `target_agent_id`；一个 Conversation 最多一个 active Run |
| retry_idempotency | user_id, operation, key, run_id, source_run_id | Retry Idempotency-Key 与首次创建 Run 的映射 |
| jobs | run_id, lease_token, lease_until, attempt_count | PostgreSQL 租约队列 |
| run_events | run_id, seq, event_type, payload_jsonb | SSE 回放事实源 |
| audit_events | action, resource, result, request_id | 不含消息正文和密钥；记录 Project 成员、Agent、权限和运行操作 |

## 关键约束

- `messages.client_request_id` 唯一索引为 `(conversation_id, sender_id, client_request_id)`，仅约束用户消息且三者均非空。
- `conversations.project_id` 在迁移 `006` 中回填并建索引，但暂时保持可空。工程包 C 切换 C++ 写入后，再在独立迁移中收紧为 `NOT NULL`。
- `target_agent_id` 是 REST 发送消息请求的必填字段，不新增为 `messages` 列；执行时映射到 `runs.agent_id`。
- 不建立 `conversation_members`；Project 成员默认可见全部 Conversation。
- Agent owner 必须是 Project member；只有 owner 自己能把自己的 Agent 加入 Project。
- 一个 Conversation 同时最多一个 `queued/running` Run；工程包 C 通过事务发送锁和 409 返回实现该不变量。
