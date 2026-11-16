# A15 多人多 Agent 最小演示与验收入口

- 状态：ACTIVE
- 适用版本：V0.2.0 当前最小 MVP
- 演示规模：2 个真实账号 + 2 个 Agent + 1 个 Project + 1 个共享 Conversation
- 数据来源：本地 Mock Provider，不需要真实模型密钥

## 1. 启动

```powershell
./scripts/setup-dev.ps1 -Up
```

确认 health：

```powershell
curl.exe -fsS http://127.0.0.1:8080/api/v1/health
```

期望：

```json
{"database":"ok","status":"ok","version":"1.0.0"}
```

## 2. 账号

| 人类 | 用户名 | 密码 | Project 角色 |
|---|---|---|---|
| Human A | `demo` | `demo1234` | owner |
| Human B | `other` | `demo1234` | editor |

仅用于本地演示。不要复用为生产账号，也不要把任何真实 API Key 写入仓库。

## 3. 生成演示数据

```powershell
./scripts/demo-2h2a.ps1 -Docker
```

脚本会幂等创建/复用：

- Project：`2H+2A 本地演示`
- Agent 1：`演示 Agent 1`，属于 `demo`
- Agent 2：`演示 Agent 2`，属于 `other`
- Conversation：`共享协作演示`
- 两条用户消息及两条 Agent 回复

输出示例字段：

```json
{
  "status": "PASS",
  "project_id": "<uuid>",
  "conversation_id": "<uuid>",
  "accounts": {"owner": "demo / demo1234", "editor": "other / demo1234"},
  "agents": {"owner": "演示 Agent 1", "editor": "演示 Agent 2"},
  "messages": 4
}
```

重复运行不会重复创建同一 Project/Conversation/消息；脚本只补齐缺失对象。报告写入 `build/demo-2h2a-report.json`。

## 4. 手工验收步骤

1. 使用 `demo` 登录 `http://localhost:8080`，选择 `2H+2A 本地演示`，确认角色徽标为 `owner`。
2. 打开另一个 InPrivate/隐身窗口，使用 `other` 登录，选择同一个 Project，确认角色徽标为 `editor`。
3. 两个窗口都选择 `共享协作演示`，确认看到同一份四条历史消息和两个 Agent 的名称。
4. Human A 在“发送给 Project Agent”中选择 `演示 Agent 1`，发送一条消息；Human B 应看到该用户消息与 Agent 1 回复。
5. Human B 选择 `演示 Agent 2`，发送一条消息；Human A 应看到该用户消息与 Agent 2 回复。
6. 打开 Project 详情，确认两个成员和两个 Project Agent 均存在。
7. owner 将 editor 改为 `viewer`；editor 刷新后应看到只读提示，发送、创建会话、取消和重试均不可用。
8. 将 editor 恢复为 `editor`，避免影响后续演示。

## 5. 自动证据

```powershell
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test.ps1 -Docker
./scripts/test-migrations.ps1 -Docker
./scripts/test-health.ps1
```

证据文件：

- `docs/evidence/ui-project-first.png`
- `docs/evidence/ui-2h2a.png`
- `docs/evidence/demo-2h2a-report.json`
- `docs/evidence/health-response.json`
- `docs/acceptance/V0.2-multiplayer-mvp.md`

## 6. 故障排查

- `health` 非 200：先检查 `docker compose --env-file .env -f deploy/docker-compose.yml ps` 与 PostgreSQL 日志。
- 登录失败：确认使用 8 位演示密码，并检查 `APP_ORIGIN` 是否包含当前浏览器源。
- 发送返回 400：确认前端已选择 Project Agent，请求体包含 `target_agent_id`。
- 发送返回 409：同一 Conversation 已有 queued/running Run；等待完成或由有权限成员取消。
- 无法加入 Agent：确认 Agent owner 是 Project 成员且角色为 owner/editor。
- 看不到新 Run：先在当前 Conversation 等待一个短轮询周期；前端发现 Run 后会订阅既有 SSE URL。

## 7. 已知限制

- 不提供 Conversation 级事件流；跨成员新 Run 发现存在最多约 2.5 秒延迟。
- 同一 Project 的写路径按 Project 行锁串行化，适配当前 MVP 规模。
- 本版不支持多 Run 并行、消息编辑删除、文件、共享记忆、插件、诊断/匹配/Agreement/Trial。
- 生产部署仍需 HTTPS、正式账号管理、备份和 Worker 扩容。
