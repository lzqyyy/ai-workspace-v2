# Handoff: PACKAGE-D-20261116 Project-first multiplayer frontend

- 状态：REVIEW
- 主责 Agent：WEB
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / `358f4a3bdee4a215becbe2cc4799883ba8bc7cf0`
- 基线：Package C `c2f74c1bbe7b637791b2df511d1f3591be70c4a6`
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

将现有原生 HTML/CSS/JavaScript 工作台切换到 Package B/C 的 Project-first API，完成 Project 切换/创建、成员与 Agent 可见性、共享 Conversation、目标 Agent 发送、Project 角色交互控制、历史恢复与共享 SSE 接入，同时保持既有单用户后端链路兼容。

## 完成内容

- 原生前端新增 Project 选择、创建、详情与角色徽标，不再把单用户 Agent 选择器当作会话权限边界。
- Project 详情弹窗展示成员和 Project Agent；owner 可添加/修改/移除成员，owner/editor 可加入自己的 Agent，owner 或 Agent 添加者可移出 Agent。viewer 只读。
- Conversation 创建/列表切换到 `POST/GET /api/v1/projects/{project_id}/conversations`。
- 发送消息始终读取 Project Agent 选择器并提交必填 `target_agent_id`；仍使用既有 Message/Run/SSE URL。
- 共享历史恢复保留；当前没有 Conversation 级 SSE，因此活动会话通过消息轮询发现未完成 Run，再订阅既有 Run SSE。双方读取相同事件序列。
- SSE 保留持久化游标；EventSource 自动重连带 `Last-Event-ID`，刷新恢复使用保存的 `after` 游标；客户端按 `(run_id, seq)` 去重。
- viewer 隐藏/禁用发送、取消、重试和会话创建；editor 不能取消/重试其他成员 Run；owner 可操作任意 Run。前端只做交互控制，服务端仍为授权依据。
- 修复浏览器验收发现的空状态旧文案问题：创建 Agent/切换共享会话后现在显示正确的“暂无消息”或 Agent 管理状态。
- 新增无第三方依赖的 headless Edge/Chrome CDP 浏览器 E2E 与截图证据。

## 修改文件

- `frontend/index.html`
- `frontend/app.js`
- `frontend/styles.css`
- `tests/frontend_state_test.js`
- `tests/frontend_e2e.js`
- `scripts/frontend-e2e.ps1`
- `docs/evidence/ui-project-first.png`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-D-20261116-project-first-frontend.md`

## 契约变更

无。仅接入已冻结的 Project-first REST 与既有 Run/SSE 契约；`target_agent_id` 为发送必填字段。

## 验证命令与结果

- `node --check frontend/app.js`：PASS。
- `node --check tests/frontend_e2e.js`：PASS。
- `node tests/frontend_state_test.js`：PASS；角色按钮矩阵、Project Conversation 路由、`target_agent_id` 发送、retry 防重、刷新恢复、共享 Run 发现、SSE 游标与去重。
- `./scripts/frontend-e2e.ps1`：PASS；真实 headless Edge 登录、创建 Project、创建并加入 Agent、创建共享 Conversation、发送消息、等待 SSE 完成并断言 Agent 回复。
- 浏览器证据：`docs/evidence/ui-project-first.png`。
- Docker-first `./scripts/setup-dev.ps1 -Up`：PASS；Core 构建、迁移、health 通过。
- `git diff --check`：PASS。

## 验收映射

- Project 列表/切换/创建：PASS（状态测试 + 浏览器 E2E）。
- Project 成员与 Agent 列表：PASS（状态测试；owner/editor/viewer 控件按角色切换）。
- 共享 Conversation 列表/创建：PASS（状态测试 + 浏览器 E2E）。
- 目标 Agent 发送：PASS（浏览器请求体断言含 `target_agent_id`，真实 E2E 完成回复）。
- 历史恢复、共享 SSE、断线重连与回放：PASS（状态测试；共享 Run 发现 + `after` 游标 + seq 去重）。
- viewer 只读及按钮权限展示：PASS（状态测试）。
- 单用户旧链路兼容：由 Package C 后端保留 legacy URL；D 未删除或改写既有 API/SSE/Run 状态机。

## 已知问题与风险

- 前端角色状态仅控制交互显示，服务端仍是唯一授权依据。
- 当前没有 Conversation 级实时事件；其他成员通过 2.5 秒消息轮询发现未完成 Run 后订阅 Run SSE，存在最多一个轮询周期的发现延迟。
- 前端未独立暴露 Project 归档；本版只做最小对话与成员/Agent 管理。
- 浏览器 E2E 依赖本机 Edge/Chrome，使用原生 CDP，不新增 npm 依赖；可通过 `BROWSER_BIN` 指定浏览器路径。

## 失败与修复记录

- 浏览器 E2E 首次运行时测试脚本把 `${unique}` 放在普通字符串中，实际等待了字面量名称；改为模板字符串后 PASS。
- 浏览器检查发现创建 Agent 后和新建 Conversation 后空状态文案仍停留在旧状态；已改为按 Project/Agent/Conversation 状态刷新，状态测试同步通过。

## 需要其他 Agent 处理的事项

- E 需要基于本前端真实执行 owner/editor/viewer、2H+2A、双 Agent 与共享 SSE 验收；若发现后端或前端缺陷，只做最小修复，不改冻结契约。

## 下一任务建议

进入 Package E，串行执行 2H+2A 权限矩阵、并发冲突、SSE 回放、取消/重试与单用户回归。
