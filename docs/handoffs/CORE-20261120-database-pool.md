# Handoff: CORE-20261120 PostgreSQL connection pool

- 状态：REVIEW
- 主责 Agent：CORE / QA
- 分支 / 提交：`agent/core/conversation-realtime` / HEAD（提交见最终报告）
- 开始时间：2026-11-20
- 完成时间：2026-11-20

## 目标

在保持 C++ Core、Go Worker、PostgreSQL、gRPC 和 SSE 架构边界不变的前提下，为 C++ PostgreSQL 访问增加固定上限连接池，避免 SSE 事件查询、health 和普通 REST 请求频繁 `PQconnectdb/PQfinish`。

## 完成内容

- `Database` 持有共享连接池状态；`PgSession` 从池中借用连接，析构时 RAII 归还。
- 默认 `DB_POOL_SIZE=8`、`DB_ACQUIRE_TIMEOUT_MS=5000`、`DB_CONNECT_TIMEOUT_SECONDS=5`、`DB_IDLE_VALIDATE_SECONDS=30`。
- 连接获取超过上限时等待可用连接；超时返回可重试 `INTERNAL`。
- libpq conninfo 始终注入 `connect_timeout`；连接空闲超过阈值时借出前执行 `SELECT 1`，失败则丢弃重建。
- `PgSession` 未提交事务归还前执行 `ROLLBACK`。
- 查询失败、迁移失败或连接状态异常时丢弃物理连接，不把失败事务放回池。
- health 检查复用同一连接池。
- Core 的 `/app/uploads` tmpfs 改为运行用户 `10001` 拥有，修复重建后 Drogon 上传临时目录权限导致的健康失败。
- 新增 `tests/database_pool.py` 和 `scripts/test-pool.ps1 -Docker`：32 并发、256 请求，并通过 `pg_stat_activity` 检查连接上限与归还状态。

## 修改文件

- `server/src/database.hpp`
- `server/src/database.cpp`
- `server/src/common.hpp`
- `server/src/core.cpp`
- `server/src/main.cpp`
- `deploy/docker-compose.yml`
- `.env.example`
- `tests/database_pool.py`
- `scripts/test-pool.ps1`
- `scripts/test.ps1`
- `README.md`
- `AGENTS.md`
- `docs/runbook/A12-reliability.md`
- `docs/runbook/A13-testing.md`
- `docs/runbook/A14-operations.md`
- `docs/TASK_BOARD.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- 本交接文件

## 契约变更

- REST/OpenAPI、Proto、SSE envelope、错误码、数据库迁移：无变化。
- 新增运行配置：`DB_POOL_SIZE`、`DB_ACQUIRE_TIMEOUT_MS`。
- C++ 仍是唯一 PostgreSQL 业务写入者；Worker 仍不访问 PostgreSQL。

## 验证命令与结果

```powershell
./scripts/setup-dev.ps1 -Up
./scripts/test-pool.ps1 -Docker
./scripts/test.ps1 -Docker
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
./scripts/test-health.ps1
```

- setup/build：PASS。
- `test-pool.ps1 -Docker`：PASS；256 requests / 32 workers；idle=4、active=0、total=4、pool_size=8；PostgreSQL 重启恢复 PASS；connect_timeout 1s 配置实测 约 2.7s 内失败。
- `test.ps1 -Docker`：PASS；契约、Worker、前端状态、连接池、单用户 E2E、2H+2A API、权限 fixture 与迁移重放通过。
- 单浏览器 E2E：PASS。
- 双浏览器 2H+2A E2E：PASS；Conversation/Run SSE 与真实 Agent 均通过。
- health：PASS；数据库停止 503，恢复 200。

## 已知问题与风险

- 空闲连接验证采用借出前检查，不运行后台探活线程；默认空闲 30 秒后才验证，未超时连接直接复用。
- `connect_timeout` 是 libpq 每次连接建立的超时；测试中的 约 2.7s 包含一次性 Core 容器启动开销。
- 池指标目前通过测试脚本查询 `pg_stat_activity.application_name='ai-workspace-core'`，未暴露为 HTTP 管理接口。

## 下一任务建议

1. COORD 审查后合并本分支。
2. 生产部署时根据 PostgreSQL `max_connections` 和 Core 副本数设置 `DB_POOL_SIZE`。
