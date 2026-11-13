# AGENTS.md — AI Workspace 多 Agent 维护契约

```yaml
agent_contract:
  version: 1.1.0
  project: AI Workspace V2.0 · 基础对话平台
  repository_root: git rev-parse --show-toplevel
  canonical_branch: main
  remote: origin
  current_release: v0.1.0
  current_scope: L0 + L1
  auto_sync_skill: .agents/skills/github-auto-sync/SKILL.md
  language: zh-CN
```

> 本文件是任何 AI Agent 进入本仓库后的第一读文件。
> 目标不是复述产品愿景，而是让不同 Agent 在不覆盖、不越权、不破坏契约的前提下持续维护项目。

## 0. Agent 启动检查

开始任何实现前，依次执行：

1. 阅读本文件。
2. 阅读 `README.md`、`docs/TASK_BOARD.md`。
3. 阅读当前任务涉及的冻结契约、ADR、Runbook 和最近交接记录。
4. 运行 `git status --short --branch` 和 `git fetch origin --prune`。
5. 确认当前分支、远端状态和写入范围。
6. 在 `docs/handoffs/<task-id>.md` 创建或更新任务交接，状态写为 `IN_PROGRESS`。
7. 只修改当前任务授权路径。

不得依赖聊天记忆、口头说明或未落库的上下文。长期事实必须写入仓库。

## 1. 决策优先级

发生冲突时按以下顺序处理：

1. 用户当前明确指令。
2. 本文件的架构红线、安全约束和单写者规则。
3. 已冻结的 OpenAPI、Protobuf、迁移、SSE 和错误码契约。
4. Accepted ADR。
5. `docs/runbook/`、`docs/acceptance/` 和交接记录。
6. `docs/implementation-checklist.md` 及未冻结草案。

不得为了绕过阻塞而降低权限、修改协议、扩大端口暴露或跳过测试。

## 2. 当前事实基线

### 已实现

- 原生 HTML/CSS/JavaScript 登录与对话工作台。
- C++ Drogon REST/SSE Core。
- 6 个 gRPC RPC 与 Go Worker。
- PostgreSQL 持久化、事务、Job 租约、重试、取消和幂等。
- Mock 流式 Provider 与 OpenAI-compatible Provider 适配器。
- 浏览器刷新后的历史恢复与 SSE 游标回放。
- 本地开发、Docker Compose、迁移、测试、Runbook 和交接体系。
- GitHub 自动同步 Skill。

### 尚未实现

- Project / 多 Owner / 共享 Agent。
- 多人共享会话和会话级实时事件。
- Task / Handoff 业务对象、双 Agent 协作。
- 插件系统、Profile、主题、命令面板。
- 正式用户注册、管理员用户管理、登录失败限流。
- 生产 HTTPS 反向代理的已部署实例。

### 当前权限语义

- 当前是“多用户账号、单 Owner 资源隔离”。
- Agent、Conversation、Run 只能由创建者访问。
- 多人共享 Agent 尚未实现，禁止在文档或验收中声称已支持。

## 3. 架构红线

```text
Browser
  -> HTTP/SSE :8080
C++ Core
  -> PostgreSQL :5432
  -> gRPC :50051
Go Worker
  -> HTTPS :443
Model API
```

不可违反：

1. 前端只用原生 HTML/CSS/JavaScript，不引入 React/Vue/Next.js。
2. Browser 只连接 C++ `8080`。
3. C++ 是唯一 PostgreSQL 业务写入者。
4. Worker 禁止连接 PostgreSQL。
5. C++ 与 Worker 只通过 gRPC + Protobuf。
6. 实时业务事件只用 SSE，不新增 WebSocket 业务端口。
7. `50051`、`5432` 只处于内部网络，不向公网发布。
8. API Key 只存在 Worker Secret/环境变量，不进入前端、数据库明文、Git 或普通日志。
9. 任务队列复用 PostgreSQL `jobs` 表与租约，不引入 Redis/Kafka。
10. 权限判断必须在 C++ 服务端完成，前端状态不可作为授权依据。
11. 所有跨语言对象必须由 Protobuf 或 JSON 契约明确。
12. 未授权访问默认拒绝。

## 4. 仓库地图

| 路径 | 作用 | 主责 |
|---|---|---|
| `api/openapi.yaml` | Browser ↔ C++ REST 契约 | COORD |
| `proto/workspace.proto` | C++ ↔ Worker gRPC 契约 | COORD |
| `docs/contracts/` | SSE、错误码、契约索引 | COORD |
| `docs/adr/` | 架构决策 | COORD |
| `docs/runbook/` | 部署、权限、执行流程、测试、运维 | QA/对应主责 |
| `docs/schema/` | ER 图与数据字典 | CORE |
| `db/migrations/` | 只增不改的 PostgreSQL 迁移 | CORE |
| `server/` | C++ REST、SSE、gRPC、数据库和权限 | CORE |
| `worker/` | Go Worker、模型适配、租约执行 | WORKER |
| `frontend/` | 原生浏览器工作台 | WEB |
| `tests/` | 契约、Worker、E2E | QA |
| `deploy/` | Docker/Compose 部署 | QA |
| `scripts/` | 构建、本地启动、测试 | QA |
| `docs/handoffs/` | 跨 Agent 交接事实 | 任务主责 |
| `docs/TASK_BOARD.md` | 任务状态与责任 | COORD |
| `.agents/skills/github-auto-sync/` | 自动提交推送流程 | COORD |

生成代码：

- Go gRPC 代码位于 `worker/gen/`，由 `proto/workspace.proto` 生成。
- C++ protobuf 代码只生成到 `build/`，不提交。
- `README.md` 是人工入口，本文件是 Agent 运行契约。

## 5. 唯一事实源

| 事实 | 唯一来源 |
|---|---|
| REST 请求/响应 | `api/openapi.yaml` |
| gRPC 字段/RPC | `proto/workspace.proto` |
| SSE 格式和状态 | `docs/contracts/sse-events.md` |
| 错误码 | `docs/contracts/error-codes.md` |
| 数据库结构 | `db/migrations/*.sql` |
| 数据关系 | `docs/schema/erd.md` |
| 架构决策 | `docs/adr/ADR-*.md` |
| 任务状态 | `docs/TASK_BOARD.md` |
| 跨 Agent 交接 | `docs/handoffs/*.md` |
| 运行配置 | `.env.example` + Runbook，不提交 `.env` |

不得把聊天消息、临时脚本或某个 Agent 的记忆当作共享契约。

## 6. 角色与单写者规则

| 角色 | 允许写入 |
|---|---|
| COORD | `AGENTS.md`、`api/`、`proto/`、`docs/contracts/`、`docs/adr/`、任务看板、合并审查 |
| CORE | `server/`、`db/migrations/`、`docs/schema/` |
| WORKER | `worker/` |
| WEB | `frontend/` |
| QA | `tests/`、`deploy/`、`scripts/`、`README.md`、`docs/runbook/`、验收记录 |

规则：

- 一个任务只有一个主责 Agent。
- 同一时间一个路径只能有一个写者。
- 跨范围修改必须先由 COORD 调整任务边界。
- 已合并迁移只能新增，不能修改。
- 共享契约只能由 COORD 变更。
- 没有交接记录的任务不能进入 `DONE`。

## 7. 多 Agent 并行协议

### 7.1 顺序单 Agent

适用于一个 Agent 独占当前 checkout：

- 可在 `main` 工作。
- 完成后使用自动同步 Skill 提交并推送。
- 必须保持工作区无其他人未提交的改动。

### 7.2 并行多 Agent

适用于两个或更多 Agent 同时维护：

1. 每个 Agent 使用独立 worktree 或独立 clone。
2. 分支必须使用 `agent/<role>/<task-id>`，例如：
   - `agent/core/G23-project-member-api`
   - `agent/web/G24-project-switcher`
   - `agent/qa/G25-multiuser-e2e`
3. 自动同步只推送当前 Agent 分支，不直接推送 `main`。
4. 完成后创建 PR，由 COORD 审查、测试并合并。
5. 禁止两个 Agent 同时修改同一文件；共享契约冲突由 COORD 裁决。
6. 禁止 force push、重写已发布历史或删除他人提交。

### 7.3 任务领取顺序

1. COORD 在 `docs/TASK_BOARD.md` 写入任务 ID、状态、主责和路径。
2. 主责 Agent 更新状态为 `IN_PROGRESS`。
3. 创建 `docs/handoffs/<task-id>.md` 并记录范围。
4. 写完、测完、提交后改为 `REVIEW`。
5. COORD 验收后改为 `DONE`。

若任务未出现在看板且未获用户直接授权，不得自行扩大范围。
## 8. 契约变更流程

任何 REST、Proto、SSE、错误码或数据库变更必须：

1. 在 `docs/handoffs/<task-id>.md` 标记 `CONTRACT-CHANGE-REQUEST`。
2. 说明字段、原因、影响方、兼容策略和迁移方式。
3. 由 COORD 创建/更新 ADR 和版本号。
4. 同步更新契约示例、生成代码和测试。
5. CORE、WORKER、WEB 在契约合并后再继续受影响部分。

不得通过“先改一边代码”“临时 JSON 字段”或“只在聊天里约定”绕过契约。

## 9. 构建与验证命令

### 完整回归

```powershell
./scripts/test.ps1
```

### 分层验证

```powershell
./scripts/test-contracts.ps1
./scripts/build.ps1
go test -buildvcs=false ./worker/...
python tests/e2e.py
```

### 本地启动/停止

```powershell
./scripts/dev.ps1 -NoBrowser
./scripts/stop-dev.ps1
```

最低要求：

- 文档-only：契约检查或文档结构检查。
- C++：`build.ps1` + 相关 API/E2E。
- Worker：Go 测试 + E2E。
- 数据库：空库迁移 + 重复迁移 + E2E。
- 安全/权限：至少一个越权拒绝测试。
- SSE/可靠性：序号、重放、取消、重复回调测试。

## 10. 数据库与迁移

- 本地数据目录：`build/pgdata`，已被 Git 忽略。
- 本地 PostgreSQL：`127.0.0.1:55432`，数据库 `workspace`。
- Docker PostgreSQL：内部 `postgres:5432`，不发布宿主机端口。
- 迁移入口：`db/migrations/*.sql`，由 C++ 启动时执行。
- 当前迁移：`001_init` 到 `004_demo_password`。
- 所有迁移必须可重复执行并记录 `schema_migrations`。
- 不得提交数据库数据、WAL、日志、`pgdata/` 或 `.env`。
- 破坏性变更必须先写 ADR。

## 11. 安全红线

- 密码最少 8 位；密码使用 PBKDF2-HMAC-SHA256 + 独立盐。
- 会话令牌只保存 SHA-256 摘要，Cookie 使用 HttpOnly/SameSite。
- 生产必须 HTTPS、`COOKIE_SECURE=true`。
- `APP_ORIGIN` 是逗号分隔的精确允许源，不得使用通配符。
- 登录、消息、Run、取消和重试都必须在服务端鉴权。
- Worker 只持有短期 Job 租约，不持有用户会话。
- 模型 API Key 只从 Worker 环境/Secret 读取。
- 审计日志不保存消息正文、Authorization、API Key 或完整敏感提示词。
- 权限拒绝必须返回最小信息并记录审计。
- 不得把 `.env`、Token、私钥、Cookie 或生产数据写入仓库/聊天/日志。

## 12. GitHub 自动同步

用户已授予本仓库持续同步授权。只要任务修改了项目文件，结束任务前必须：

1. 运行相关测试。
2. 检查 `git diff`、`git diff --check` 和敏感文件。
3. 调用：

```powershell
./.agents/skills/github-auto-sync/scripts/github-sync.ps1 -Message "<commit message>"
```

4. 确认输出为 `SYNCED ...`。
5. 在最终回复中报告 commit hash 和推送结果。

分支行为：

- `main`：顺序单 Agent 自动推送 `origin/main`。
- `agent/<role>/<task-id>`：并行 Agent 自动推送同名远端分支。
- 不自动推送其他分支名。
- 不 force push。
- 测试失败、发现密钥、范围不明、远端冲突时停止并保留现场。

纯读取任务或无 tracked file 变化时，不创建空提交。

## 13. 交接文件模板

```markdown
# Handoff: <task-id> <task-name>

- 状态：IN_PROGRESS / REVIEW / BLOCKED / DONE
- 主责 Agent：COORD / CORE / WORKER / WEB / QA
- 分支 / 提交：
- 开始时间：
- 完成时间：

## 目标
## 完成内容
## 修改文件
## 契约变更
## 验证命令与结果
## 验收映射
## 已知问题与风险
## 需要其他 Agent 处理的事项
## 下一任务建议
```

交接必须写入仓库，不得只留在聊天里。未完成项必须明确写失败或未覆盖。

## 14. Definition of Ready

任务只能在以下条件满足后开始：

- 任务 ID、目标、主责和写入范围明确。
- 依赖已完成，或明确允许并行。
- 所需契约已存在且有版本。
- 验证命令明确。
- 安全边界和资源归属明确。
- 不依赖未确认的端口、字段或框架。

## 15. Definition of Done

任务完成必须同时满足：

- 代码/文档已完成。
- 只修改授权范围。
- 契约、迁移、生成代码和测试同步。
- 至少一条可重复验证命令通过。
- 权限、状态机、幂等和敏感信息边界有检查。
- 没有密钥、Token、数据库数据或无关构建产物进入 Git。
- 交接记录完整。
- 自动同步成功，或已明确记录失败原因。

前端额外要求：刷新恢复、SSE 重连/回放、错误提示、窄屏可用。
Worker 额外要求：不连数据库、租约/取消/超时可终止、重复回调幂等。
C++ 额外要求：写前鉴权、事务一致、SSE 序号单调、审计脱敏。

## 16. 当前已知陷阱

### Origin

本地允许：

```text
http://localhost:8080
http://127.0.0.1:8080
```

生产改 `APP_ORIGIN`，不要关闭校验。

### SSE 排序

事件必须按数值 `run_events.seq` 排序，不能按 `seq::text` 排序。

### 当前触发消息

Worker 上下文必须包含本次 trigger message；否则模型会引用上一条用户消息。

### Worker 并发

当前单 Worker 主循环串行处理 Job。需要并行时优先增加 Worker 副本，C++ 的 `SKIP LOCKED` 支持安全并发领取。

### 终态回调

已终态 Run 的重复 UpdateJobStatus 必须返回 accepted+duplicate，不得新增消息或事件。

### Demo 凭据

演示账号只用于本地 Acceptance，不得作为生产账号。当前说明见 `README.md`。

## 17. 建议的下一阶段工作

按顺序推进：

1. 中心化多用户部署：HTTPS、账号管理、Worker 扩展、备份。
2. Project + ProjectMember：共享 Agent、owner/editor/viewer。
3. 私有 Conversation：共享 Agent，但每个用户独立会话上下文。
4. 会话级 SSE：多人共享会话时同步消息和运行状态。
5. Task/Handoff、双 Owner、双 Agent、第二 Provider。

每次只推进一个可验收切片，不在基础权限未稳定时提前做插件平台。

## 18. Agent 结束前自检

```text
[ ] 我是否只修改了授权路径？
[ ] 是否读取了最新契约和交接？
[ ] 是否运行了相关测试？
[ ] 是否检查了敏感信息和无关变更？
[ ] 是否更新了交接与任务状态？
[ ] 是否通过自动同步 Skill 推送？
[ ] 最终回复是否包含测试结果、风险、commit 和下一步？
```
