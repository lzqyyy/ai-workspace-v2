# Handoff: PACKAGE-F-20261116 delivery and closeout

- 状态：REVIEW
- 主责 Agent：QA/COORD
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / 本包本地提交（hash 见最终交付报告）
- 基线：Package E `6abaade2f8b86d19ba3108ba19e033c54c86ad9d`
- Worktree：`D:\workspace\tmp\ai-workspace-v2-package-def`
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

在不新增业务功能的前提下完成 D/E/F 最小 MVP 交付收口：Docker-first 启动与演示、2H+2A 演示数据/账号、验收入口、截图与 health 证据、Runbook、任务看板、AGENTS 当前事实和最终交接。所有提交保持本地，不 push、不合并 main。

## 包级交付

| 包 | Commit | 内容 |
|---|---|---|
| D | `358f4a3bdee4a215becbe2cc4799883ba8bc7cf0` | Project-first 原生前端、成员/Agent 管理、共享 Conversation、target Agent、共享 SSE、角色交互 |
| E | `6abaade2f8b86d19ba3108ba19e033c54c86ad9d` | 双浏览器 2H+2A、权限/并发/幂等/迁移测试、viewer 浏览器只读 |
| F | 本包本地提交 | 演示 seed、A15 Runbook、验收记录、README/AGENTS/TASK_BOARD、证据与最终交接 |

## 完成内容

- 新增幂等 `tests/demo_2h2a.py` 与 `scripts/demo-2h2a.ps1`：创建/复用同一 demo Project、两个 Agent、共享 Conversation 和四条演示消息。
- 新增 `docs/runbook/A15-multiplayer-demo.md`，覆盖启动、账号、自动数据、手工 2H+2A 步骤、自动证据和故障排查。
- 新增 `docs/acceptance/V0.2-multiplayer-mvp.md`，记录验收范围、覆盖矩阵、命令、证据和下一阶段建议。
- 复制/保存演示报告与 health 响应到 `docs/evidence/`。
- 更新 README 为 Project-first 多人平台入口；更新 AGENTS 当前事实/权限/下一阶段；更新任务看板为 V0.2.0。
- D/E handoff 补入精确本地 commit。
- 未新增诊断、匹配、Agreement、Trial、插件、文件、共享记忆、多 Run 并行或新业务端口。

## 修改文件

- `README.md`
- `AGENTS.md`
- `tests/demo_2h2a.py`
- `scripts/demo-2h2a.ps1`
- `docs/runbook/A15-multiplayer-demo.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/evidence/demo-2h2a-report.json`
- `docs/evidence/health-response.json`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-D-20261116-project-first-frontend.md`
- `docs/handoffs/PACKAGE-E-20261116-2h2a-acceptance.md`
- `docs/handoffs/PACKAGE-F-20261116-delivery-closeout.md`

## 契约变更

无。交付只使用已冻结 Project-first REST、Run 状态机、SSE envelope、错误码和数据库模型。

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/demo-2h2a.ps1 -Docker
./scripts/demo-2h2a.ps1 -Docker
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-migrations.ps1 -Docker
./scripts/test-health.ps1
```

- Demo seed 首次运行：PASS；创建 `2H+2A 本地演示`、两个 Agent、共享 Conversation、4 条消息和 2 个 Run。
- Demo seed 第二次运行：PASS；复用相同 Project/Conversation，消息仍为 4，`created_runs=[]`。
- Docker-first 全量：PASS；契约、Worker、前端状态、单用户 E2E、2H+2A API、Project fixture、审计与迁移重放全部通过。
- 单用户浏览器 E2E：PASS；截图 `docs/evidence/ui-project-first.png`。
- 2H+2A 双浏览器 E2E：PASS；双方共享历史与双方 SSE 均已验证，viewer 刷新后只读；截图 `docs/evidence/ui-2h2a.png`。
- Migration replay：PASS；`migrations=7`、`project_id_nullable=NO`、`null_rows=0`、`migration_007=1`。
- Health：PASS；数据库停止时 503 `degraded/unavailable`，恢复后 200 `ok`。
- `git diff --check`：PASS。

## 验收映射

- Docker-first 启动：PASS（setup-dev + A15 Runbook）。
- 2H+2A 演示数据与账号：PASS（demo seed 两次）。
- 权限矩阵、非成员/Agent/SSE 拒绝：PASS（E 证据）。
- 单 active Run、409、幂等、取消/重试：PASS（E 证据）。
- SSE 顺序、回放、双方订阅：PASS（E 证据）。
- 单用户旧链路：PASS（Docker-full E2E）。
- DB 迁移可重复、单用户回归、工作区干净：验证后保持 clean 并本地提交。
- 截图/health/已知限制/下一阶段：PASS（验收记录与 A15）。

## 已知问题与风险

- 新 Run 跨成员发现仍是 2.5 秒消息轮询；Run 事件本身使用冻结的持久化 SSE 游标，下一阶段建议补 Conversation 级通知/订阅。
- 同一 Project 写路径由 Project 行锁串行化，MVP 规模可接受；多 Run 并行前需重新评审锁粒度。
- 浏览器 E2E 依赖本机 Edge/Chrome；Docker-first API 2H+2A 不依赖宿主机浏览器。
- 正式生产仍需 HTTPS、正式账号、管理员/限流、备份、监控和 Worker 扩容。
- 本任务按用户要求没有 push、没有合并 main，共享 checkout 和 main 未修改。

## 下一阶段建议

1. Conversation 级实时通知或 Run subscription 发现，消除跨成员新 Run 轮询延迟。
2. 多 Run 并行前评审锁粒度、前端状态与冲突策略。
3. 中心化多用户部署：HTTPS、正式账号、限流、审计查询、备份和监控。
4. 再按 Master PRD 推进 Task/Handoff、诊断、能力缺口、匹配、Agreement、Trial 和插件。
