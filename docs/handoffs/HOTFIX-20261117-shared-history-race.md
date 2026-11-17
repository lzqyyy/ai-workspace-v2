# Handoff: HOTFIX-20261117 shared-history polling race

- 状态：REVIEW
- 主责 Agent：WEB/QA
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / 基线 `79e5196`，修复提交 hash 见最终报告
- 开始时间：2026-11-17
- 完成时间：2026-11-17

## 目标

修复共享 Conversation 历史轮询的重叠响应竞态：较早发出但较晚完成的 GET 不得覆盖较新的消息快照。同时修复 Windows 下浏览器 E2E 临时 profile 清理导致的误报。

## 完成内容

- `loadMessages` 增加递增请求序号；只有最新请求可以更新 `state.messages`、触发共享 Run 发现或显示错误。
- 新增状态测试：先完成第二个请求，再返回第一个旧请求，断言旧响应不能覆盖新历史。
- 单浏览器与双浏览器浏览器 CDP 脚本改为等待浏览器进程退出后删除临时 profile；删除增加重试，清理失败只记录警告，不再把已通过的业务断言判为失败。

## 修改文件

- `frontend/app.js`
- `tests/frontend_state_test.js`
- `tests/frontend_e2e.js`
- `tests/frontend_2h2a_e2e.js`
- `docs/TASK_BOARD.md`
- `docs/handoffs/HOTFIX-20261117-shared-history-race.md`

## 契约变更

无。未修改 REST、Proto、SSE envelope、错误码、数据库迁移或权限语义。

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-health.ps1
```

- `./scripts/setup-dev.ps1 -Up`：PASS。
- `./scripts/test.ps1 -Docker`：PASS；契约、Worker、前端状态、单用户 E2E、共享 2H+2A API、Project 权限、迁移重放全部通过。
- 前端状态测试新增 `out_of_order_history:true`：PASS。
- `./scripts/frontend-e2e.ps1`：PASS；临时 profile 清理无 EPERM。
- `./scripts/frontend-2h2a-e2e.ps1`：PASS；共享历史、双方具体 Run SSE、viewer 只读通过。
- `./scripts/test-health.ps1`：PASS；PostgreSQL 停止时 503，恢复后 200。


## 验收映射

| 项目 | 结果 |
|---|---:|
| 旧历史响应不能覆盖新响应 | PASS |
| 共享 Run 发现仍正常 | PASS |
| 单用户与双用户浏览器链路 | PASS |
| Windows profile 清理不误报 | PASS |
| 冻结契约边界不变 | PASS |

## 已知问题与风险

- 跨成员新 Run 发现仍依赖 2.5 秒轮询；本次只修复响应乱序覆盖，没有新增 Conversation SSE。
- 浏览器临时目录若在极端情况下仍被外部进程占用，会记录 cleanup warning，但不改变业务断言结果。

## 需要其他 Agent 处理的事项

审查本次提交后决定是否合并分支。

## 下一任务建议

如需要更低延迟，另立 Conversation 级实时通知任务，保持现有 Run SSE envelope 不变。
