# Handoff: G02 数据库 ER 与迁移

- 状态：DONE
- 主责 Agent：CORE
- 完成时间：2026-11-10

## 完成内容
- users/sessions/agents/model_providers/conversations/messages/runs/jobs/run_events/audit_events。
- 唯一约束覆盖 client_request_id、assistant(run_id)、run_event(run_id,seq) 与终态事件。
- C++ 启动按文件名执行幂等迁移。

## 修改文件
- `db/migrations/001_init.sql`、`002_seed.sql`、`003_test_user.sql`、`docs/schema/*`、`server/src/database.*`。

## 契约变更
- 初始 schema，无兼容性破坏。

## 验证命令与结果
```text
新空库启动 Core -> schema_migrations=3, users=2
PASS
```

## 验收映射
- D04、B03：通过。
- Gate 1 数据对象来源：通过。

## 已知问题与风险
- 已合并迁移不得修改，只能新增编号迁移。

## 下一任务建议
- C++ 业务实现。