# Handoff: HOTFIX-20261118-sse-event-seq SSE 事件序号连续性

- 状态：REVIEW
- 主责 Agent：CORE / QA
- 分支 / 提交：agent/core/hotfix-20261118-event-seq / 待自动同步
- 开始时间：2026-11-18
- 完成时间：2026-11-18

## 目标

修复 S4：`appendEvent()` 必须拒绝跳号和乱序事件，同时保留同一事件重复投递的幂等行为。

## 完成内容

- `appendEvent()` 的 INSERT 改为只在 `event_seq = max(seq)+1` 时写入。
- 跳号/乱序且不存在同序号旧事件时返回 `CONFLICT`。
- 同序号重复事件改为 PostgreSQL `jsonb` 语义比较，避免 JSON 文本规范化造成误冲突。
- 新增事件序号专项 helper 和 Python 检查，并纳入 `test-lease.ps1`。
- 更新测试 Runbook。

## 修改文件

- `server/src/core.cpp`
- `tests/duplicate_callback/main.go`
- `tests/event_seq_guard.py`
- `scripts/test-lease.ps1`
- `docs/runbook/A13-testing.md`
- `docs/TASK_BOARD.md`

## 契约变更

无。复用现有 `CONFLICT` 错误码；未新增 REST、Proto、SSE 或数据库字段。

## 验证命令与结果

- `./scripts/setup-dev.ps1 -Up`：PASS，Core 重新构建且 healthy。
- `./scripts/test-lease.ps1`：PASS
  - wrong lease：PASS
  - cancel + lease expiry：PASS
  - event seq gap/out-of-order rejection：PASS
  - duplicate event idempotency：PASS
- `./scripts/test.ps1 -Docker`：PASS
  - contracts：PASS
  - worker test/vet：PASS
  - frontend state：PASS
  - full E2E：PASS

## 验收映射

- 正常连续事件被接受。
- 跳号事件返回 `CONFLICT`。
- 乱序事件返回 `CONFLICT`。
- 同一事件重复投递仍返回 `duplicate=true`。
- 原有 E2E、lease、health 检查不回归。

## 已知问题与风险

- 测试前发现持久化测试库残留 `006/007` 迁移；已重置本地测试卷后复测通过。
- 该修复依赖现有 job/run 行锁事务，保持与现有并发模型一致。

## 需要其他 Agent 处理的事项

无。

## 下一任务建议

自动同步后从远端全新 clone，执行 `./scripts/setup-dev.ps1 -Up` 与 `./scripts/test.ps1 -Docker`，再完成浏览器回归。
