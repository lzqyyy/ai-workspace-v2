# Handoff: HOTFIX-20261116 MVP merge-review blockers

- 状态：REVIEW
- 主责 Agent：CORE/QA
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / 基线 `f78df9c7373750ca669209b32c4d35070fbe8d8f`，修复提交 hash 见最终报告
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

修复 D/E/F MVP 合并审查发现的三个阻塞项，不改变已冻结角色、Project/Conversation 授权、单 active Run、SSE envelope、幂等和 C++/Go/PostgreSQL 边界。

## 完成内容

### 1. UUID 共享校验

根因：`CoreService::require_uuid` 只检查长度和 4 个连字符位置，36 字符非十六进制值会进入 PostgreSQL 并触发 500。

修复：在同一共享校验函数内检查 36 位 canonical `8-4-4-4-12` 布局及每一位十六进制字符。所有调用方统一受益，包括 `target_agent_id`、`project_id`、`conversation_id`、`run_id`、`agent_id`、`job_id` 和成员/Agent body UUID。

回归：单用户 E2E 新增 36 字符非十六进制 `target_agent_id` 与 `project_id`，均断言 400 `VALIDATION_FAILED`。

### 2. 双浏览器 2H+2A 阻塞

根因：测试在 editor 发送前只等待 `run-state-text === 已完成`。editor 页面此时仍显示 owner 上一轮 Run 的终态，条件立即为真；测试没有确认 editor 的 POST 已创建新消息/Run，因此 owner 等待的 editor 消息从未提交。这不是后端共享历史或授权缺陷。

修复：

- E2E 改为两个独立 headless 浏览器进程和独立 cookie/profile，避免单浏览器双 target 的前台/后台状态混用。
- 发送前等待 composer 可用且目标 Agent 已生效。
- 发送后先等待本页出现自己的 user message，再等待 Run 完成，消除旧终态竞态。
- 双方 SSE 断言改为按实际 owner/editor Run ID 分别验证对方订阅。
- 前端移除消息发现轮询的 `document.hidden` 门控；后台页面定时器允许运行时可继续发现共享消息，页面重新可见时仍立即刷新。
- 测试失败诊断保留页面 `hidden`、DOM 消息、消息轮询/响应状态，便于后续定位。

### 3. Project 归档契约缺口

核对结果：

- 最小 MVP PRD 第 9.1 节和五项权限矩阵定义 owner 归档、active Run 时 409、归档后只读。
- `api/openapi.yaml` 顶部和 Project schema 原描述提到归档行为，但 `paths` 中没有归档操作。
- C++ 路由和 `CoreService` 没有归档方法，运行时确实没有入口。

最小处理：不新增未经冻结的 endpoint/method。OpenAPI 描述改为明确“归档语义来自 PRD，但当前契约没有归档操作”，README 和验收记录同步标注。补齐归档需要 COORD 单独批准版本化 OpenAPI、路由、事务锁、审计与测试，不能由本次合并阻塞修复擅自扩大范围。

## 修改文件

- `server/src/core.cpp`
- `frontend/app.js`
- `tests/e2e.py`
- `tests/frontend_state_test.js`
- `tests/frontend_2h2a_e2e.js`
- `api/openapi.yaml`
- `docs/contracts/error-codes.md`
- `README.md`
- `docs/TASK_BOARD.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/handoffs/HOTFIX-20261116-mvp-review-blockers.md`

## 契约变更

- 未新增或修改 REST operation、Proto RPC、数据库迁移、SSE event envelope、错误码或 Run 状态机。
- OpenAPI 仅澄清现有文档矛盾：Project 归档没有冻结操作，不提供可调用入口。

## 验证命令与结果

```powershell
git fetch origin --prune
./scripts/setup-dev.ps1 -Up
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-health.ps1
```

- `git fetch origin --prune`：PASS。
- `./scripts/setup-dev.ps1 -Up`：PASS；Core/Worker 重建、迁移、health 通过。
- `./scripts/test.ps1 -Docker`：PASS；OpenAPI 25 operations、6 RPC、5 SSE events；Worker test/vet、前端语法/状态、单用户 E2E、共享 2H+2A API、Project 权限 fixture、迁移重启重放全部通过。
- `./scripts/frontend-e2e.ps1`：PASS；Project-first 单用户真实浏览器链路完成。
- `./scripts/frontend-2h2a-e2e.ps1`：PASS；两个独立浏览器实例、两个 Agent、共享历史、双方具体 Run SSE、viewer 只读。
- 最终断言后连续运行 3 次 `frontend_2h2a_e2e.js`：PASS。
- `./scripts/test-health.ps1`：PASS；PostgreSQL 停止时 503 `degraded/unavailable`，恢复后 200。
- 非法 UUID 手工实测：
  - `target_agent_id=zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz` → HTTP 400 `VALIDATION_FAILED`。
  - `project_id=zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz` → HTTP 400 `VALIDATION_FAILED`。

## 验收映射

| 审查阻塞项 | 结果 | 证据 |
|---|---:|---|
| 非法 36 字符非十六进制 `target_agent_id` 不再触发 500 | PASS | 单用户 E2E、手工 HTTP 400 |
| 其他 UUID 参数复用同一根因修复 | PASS | `project_id` E2E + 手工 HTTP 400 |
| owner/editor 双独立浏览器、双 Agent、同 Project/Conversation | PASS | `frontend_2h2a_e2e.js` |
| editor 新消息确实提交后才等待 owner 可见 | PASS | composer/own-message waits + API/DB 证据 |
| owner 看到 editor 消息与 Agent 2 回复 | PASS | 双浏览器 E2E |
| editor/owner 分别订阅对方具体 Run SSE | PASS | CDP Network + Run ID 断言 |
| viewer 只读 | PASS | 双浏览器 E2E |
| 单 active Run、幂等、权限、SSE replay 回归 | PASS | Docker-first 全量 |
| Project 归档契约矛盾已明确且未擅自扩范围 | PASS | OpenAPI 澄清、README/验收/本交接 |

## 已知问题与风险

- 跨成员新 Run 发现仍依赖活动 Conversation 的 2.5 秒轮询；本次只修复后台页门控和测试竞态，没有新增 Conversation SSE 或改变 Run SSE envelope。
- Project 归档仍无 API/运行时实现。补齐前，客户端不得调用或自行推断归档路由。
- 浏览器 E2E 依赖本机 Edge/Chrome；API 全量入口不依赖宿主机浏览器。
- 同一 Project 写路径仍按既有设计使用 Project 行锁串行化。

## 需要其他 Agent 处理的事项

- COORD 审查本次本地提交、测试证据和 Project 归档契约缺口说明。
- 如需本版真正支持归档，应另开契约变更任务，定义 method/path/request/response、active Run 409、归档后写拒绝、审计和回归测试。

## 下一任务建议

1. 审查并决定是否将 PACKAGE A-F 与本次 hotfix 一起合并 main。
2. 单独立项实现 Project 归档版本化契约，不与本次阻塞修复混合。
3. 下一版再评估 Conversation 级实时通知，消除短轮询发现延迟。
