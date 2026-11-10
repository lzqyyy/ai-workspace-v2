# 核心数据字典 V1.0.0

| 表 | 关键字段 | 说明 |
|---|---|---|
| users | id, username, password_hash, status | 用户；密码仅存 PBKDF2 哈希 |
| sessions | token_hash, expires_at, revoked_at | 会话令牌仅存 SHA-256 摘要 |
| model_providers | provider_type, base_url, model_name, secret_ref | API Key 仅存外部 secret_ref 名称 |
| agents | owner_id, system_prompt, model_provider_id | Agent 身份与模型配置解耦 |
| conversations | owner_id, agent_id, project_id | project_id 为 L2 可空预留 |
| messages | conversation_id, seq, sender_type, content, run_id | 用户消息与最终 Agent 消息 |
| runs | trigger_message_id, status, cancel_requested | 一次模型执行 |
| jobs | run_id, lease_token, lease_until, attempt_count | PostgreSQL 租约队列 |
| run_events | run_id, seq, event_type, payload_jsonb | SSE 回放事实源 |
| audit_events | action, resource, result, request_id | 不含消息正文和密钥 |