# Handoff: PACKAGE-E-20261116 2H+2A acceptance and stability

- 状态：REVIEW
- 主责 Agent：QA/CORE
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / `6abaade2f8b86d19ba3108ba19e033c54c86ad9d`
- 基线：Package D `358f4a3bdee4a215becbe2cc4799883ba8bc7cf0`
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

在 Package C 后端和 Package D Project-first 前端之上，真实执行 2 人 + 2 Agent、同 Project、同 Conversation、双目标 Agent、共享历史与共享 Run SSE；补齐权限矩阵、并发冲突、幂等、SSE 回放、迁移重放和单用户回归的可重复验收。

## 完成内容

- 新增无第三方依赖的双浏览器上下文 E2E：`demo` owner 与 `other` editor 分别登录，各自选择不同 Agent 在同一共享 Conversation 中发送，双方均看到 2 条用户消息和 2 条 Agent 回复。
- 浏览器 E2E 通过 CDP Network 事件断言双方均实际订阅 `/api/v1/runs/{run_id}/events`，不是只靠历史轮询冒充 SSE。
- 在 2H+2A 浏览器验收末尾把 editor 降级为 viewer，刷新后断言输入、发送、会话创建、取消/重试控件只读。
- 扩展 API 2H+2A 测试：非法 `target_agent_id` 格式返回 400；owner/editor 同时发送时一个成功、另一个返回 409 `CONFLICT`，保持单 active Run。
- 新增 `./scripts/test-migrations.ps1 -Docker`：检查 `conversations.project_id NOT NULL`、NULL 行数、007 迁移行，并重启 Core 后复跑迁移再检查。
- 将迁移重放检查接入 Docker-first `./scripts/test.ps1 -Docker`。
- 未修改 OpenAPI、Proto、SSE envelope、Run 状态机或权限契约。

## 修改文件

- `tests/shared_conversation.py`
- `tests/frontend_2h2a_e2e.js`
- `scripts/frontend-2h2a-e2e.ps1`
- `scripts/test-migrations.ps1`
- `scripts/test.ps1`
- `docs/evidence/ui-2h2a.png`
- `docs/runbook/A13-testing.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-E-20261116-2h2a-acceptance.md`

## 契约变更

无。测试和最小缺陷修复均在冻结的 Project/Member/Agent/Conversation/Run/SSE 契约内完成。

## 可重复运行命令

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-migrations.ps1 -Docker
```

浏览器脚本需要本机 Edge/Chrome；可用 `BROWSER_BIN` 指定路径。

## 验证命令与结果

- `./scripts/test.ps1 -Docker`：PASS。
  - 契约检查 PASS；OpenAPI 25 operations、6 RPC、5 SSE events。
  - Go Worker test/vet PASS。
  - 前端语法与状态测试 PASS。
  - 单用户 E2E PASS：登录、消息、SSE、幂等、重复终态回调、回放、取消、重试、并发 send 与跨用户拒绝。
  - `frontend_2h2a_e2e` / shared API 2H+2A PASS。
  - Project API 权限/移除保护/审计 fixture PASS。
  - 迁移重放检查 PASS。
- `./scripts/frontend-e2e.ps1`：PASS；Project 创建、Agent 加入、共享 Conversation、target Agent 发送、真实 SSE 完成与截图。
- `./scripts/frontend-2h2a-e2e.ps1`：PASS；双账户、双 Agent、双浏览器上下文、双方 SSE、共享历史和 viewer 只读。
- `./scripts/test-migrations.ps1 -Docker`：PASS；`migrations=7`、`project_id_nullable=NO`、`null_rows=0`、`migration_007=1`、Core 重启重放 PASS。
- 浏览器证据：`docs/evidence/ui-project-first.png`、`docs/evidence/ui-2h2a.png`。
- `git diff --check`：PASS。

## 验收映射

| 验收项 | 证据 | 结果 |
|---|---|---:|
| owner + editor、两个 Agent、同 Project/Conversation | `frontend_2h2a_e2e.js`、`shared_conversation.py` | PASS |
| A 选择 Agent 1；B 看到消息与 Agent 1 SSE 回复 | 双浏览器上下文 + CDP SSE URL 断言 | PASS |
| B 选择 Agent 2；A 看到消息与 Agent 2 SSE 回复 | 双浏览器上下文 + CDP SSE URL 断言 | PASS |
| owner/editor/viewer 权限矩阵 | `project_api.py`、`shared_conversation.py`、viewer 浏览器刷新断言 | PASS |
| 非成员访问 403 | `shared_conversation.py`、`project_api.py` | PASS |
| viewer 发送/取消/重试 403 | `shared_conversation.py` | PASS |
| 非 Project Agent 403；缺失/非法 target 400 | `shared_conversation.py`、单用户 validation | PASS |
| 同 Conversation 单 active Run；send/retry 409 | fixture + 双用户并发 send + retry active test | PASS |
| owner/editor 只能取消/重试自己的 Run；owner 可操作任意 Run | `shared_conversation.py` | PASS |
| 不同用户同 client_request_id 不冲突；同用户幂等 | `shared_conversation.py`、`e2e.py` | PASS |
| SSE 顺序、回放、取消、重复终态回调 | `e2e.py`、`shared_conversation.py` | PASS |
| 单用户 E2E 回归 | `./scripts/test.ps1 -Docker` | PASS |
| 迁移可重复执行、project_id NOT NULL | `test-migrations.ps1 -Docker` | PASS |

## 失败与修复记录

- 第一轮 2H+2A 浏览器脚本等待表达式误用了字面量 `${unique}`，导致超时；改为模板字符串后通过。该失败属于测试脚本缺陷，不是业务链路。
- 双浏览器脚本初期没有直接证明 SSE 订阅；增加 CDP `Network.responseReceived` 对 `/runs/{id}/events` 的断言后才记为通过。
- 未发现需要修改冻结后端契约或状态机的缺陷。

## 已知问题与风险

- 浏览器级 2H+2A 脚本依赖本机 Edge/Chrome，未放入纯 Docker 测试入口；API 2H+2A 已包含在 Docker-first 全量入口。
- 共享 Run 发现依赖活动 Conversation 的 2.5 秒轮询；SSE 本身按持久化游标回放，但发现新 Run 有一个轮询周期延迟。
- 同一 Project 的写路径按 Package C 设计由 Project 行锁串行化，MVP 可接受；高吞吐时需要收窄锁粒度。
- 本轮测试数据只用于本地演示，不包含真实模型密钥或生产凭据。

## 需要其他 Agent 处理的事项

- F 使用通过的脚本生成稳定演示数据、启动说明、验收记录和最终交接；不得重新扩大业务范围。

## 下一任务建议

进入 Package F，完成 Docker-first 演示脚本、2H+2A 演示账号/数据说明、验收截图索引、health 检查、已知限制与最终交接。
