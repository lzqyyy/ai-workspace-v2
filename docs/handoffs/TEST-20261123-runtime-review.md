# Handoff: TEST-20261123 Conversation realtime, pool and runtime review

- 状态：REVIEW
- 主责 Agent：CORE / QA
- 分支 / 提交：`agent/core/conversation-realtime` / base `0c8f212`
- 开始时间：2026-11-23
- 完成时间：2026-11-23

## 目标

在不依赖 Docker 的本机工具链环境中，完整执行构建、契约、Worker、前端、API E2E、Conversation realtime、Project 权限/归档、浏览器、连接池、迁移和可靠性检查；修复测试与契约审查发现的问题。

## 完成内容

- 使用项目 `.tools` 中的 PostgreSQL、MSYS2、CMake、Ninja、Go 和本机 Node/Python，完成本地 C++ Core 与 Go Worker 构建。
- 修复非 Docker `scripts/e2e.ps1` 的实时取消竞态：Conversation realtime 期间暂停 Worker，使 Run 保持 queued，并在 finally 中恢复 Worker。
- 将 Docker 路径已有的 Project active Run fixture、Project API、权限 gRPC 和审计检查补齐到非 Docker E2E。
- 修复审计接口 `limit=1x` 被 `std::stoi` 静默接受的问题；现在要求完整整数。
- 增加 `limit=1x` 与 `limit=101` 的 400 `VALIDATION_FAILED` 回归断言。
- 完成连接池并发、连接失效恢复和不可达数据库 connect_timeout 检查。
- 完成迁移重放、单用户浏览器、双浏览器 2H+2A、Conversation SSE、Run SSE、取消/重试、幂等和权限边界回归。

## 修改文件

- `scripts/e2e.ps1`
- `server/src/app.cpp`
- `tests/project_api.py`
- `docs/handoffs/TEST-20261123-runtime-review.md`

## 契约变更

无。仅修复实现与 OpenAPI 整数参数契约不一致的问题，并增强测试。

## 验证命令与结果

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/e2e.ps1
./scripts/frontend-e2e.ps1 -Screenshot build/ui-project-first.png
./scripts/frontend-2h2a-e2e.ps1 -Screenshot build/ui-2h2a.png
go vet ./worker/...
python -m py_compile ...
git diff --check
```

- `build.ps1`：PASS；C++ Core、Go Worker、gRPC 生成代码和辅助程序构建成功。
- `test.ps1`：PASS；契约 28 operations / 6 RPC、Worker test、前端语法/状态、单用户 E2E。
- `e2e.ps1`：PASS；共享 Conversation、Conversation realtime、Project active Run 保护、归档、权限 RPC、审计检查。
- Conversation `run.available` 延迟实测约 505ms；retry 事件、重复发送不增事件、viewer 订阅和非成员拒绝均 PASS。
- 单用户浏览器 E2E：PASS。
- 双浏览器 2H+2A E2E：PASS；双方 Conversation/Run SSE、共享历史、viewer 只读、owner 审计/归档 UI 均通过。
- 连接池压力：256 requests / 32 workers，至少两轮 PASS；结束后 `idle=5, active=0, total=5`，未超过 `pool_size=8`。
- 强制终止 Core 池内 PostgreSQL 后端连接后，首批并发请求出现一次 503，随后健康接口立即恢复 200，下一轮 256 请求全部 PASS。
- 不可达数据库启动测试：Core 退出码 1，建连超时实测约 1223ms，低于 8s 预算。
- 迁移重放：9 条迁移、007/008/009 各一次、`conversation_events` 表存在、关键 NOT NULL 列无空值，PASS。
- `go vet ./worker/...`、Node 语法/状态检查、Python 编译检查、`git diff --check`：PASS。

## 验收映射

- Conversation 新 Run 实时发现：PASS。
- 事件回放、幂等、retry 事件和真实目标 Agent：PASS。
- viewer 订阅、非成员 403、Project member 权限边界：PASS。
- Run SSE 与 Conversation SSE 串联：PASS。
- Project active Run 移除/归档保护：PASS。
- owner-only 审计与归档：PASS。
- 连接池复用、并发、归还、失效连接恢复和 connect_timeout：PASS。
- 迁移幂等重放：PASS。
- 浏览器刷新、SSE 重连、双浏览器协作：PASS。

## 已知问题与风险

- Docker CLI 当前不在本机 PATH，未重新执行 `test.ps1 -Docker`；已用等价的本地工具链覆盖构建、数据库、API、权限与浏览器链路。
- Conversation SSE 当前按每连接 0.5 秒周期同步查询 PostgreSQL，且每次都计算 `MAX(seq)`；活跃订阅增加时会放大数据库和 Drogon 事件循环压力。MVP 可用，生产扩展前应改为通知机制或减少 tail 查询。
- 连接池默认空闲 30 秒后才执行借出前探活。强制终止后端连接后，失效连接被首次使用时可能产生一轮 503；随后自动恢复。若要求数据库重启后零瞬时失败，可评估每次借出验证或有限重试，但需权衡每请求额外查询成本。
- `Last-Event-ID` 非数字时当前回退到 0，可能触发历史回放；契约尚未明确必须返回 400。
- 日志中观察到一次 Drogon `FATAL no writing but write callback called`，发生在 SSE 客户端断开附近；全部浏览器重连与功能断言通过，未复现为功能失败，合并前建议持续观察日志。
- 非 Docker E2E 新增了与 Docker 路径等价的 fixture SQL，当前存在两处同源测试数据，后续可抽取共享夹具，避免漂移。

## 需要其他 Agent 处理的事项

- COORD 审查本次测试修复并决定是否纳入当前实时链路合并。
- 合并前确认上述 SSE 轮询和连接池瞬时 503 风险是否接受为 MVP 已知限制。
- 合并并稳定后创建 `V0.2.0` tag。

## 下一任务建议

- 实施 Conversation SSE 查询降载：将 tail 查询限制为首连，或引入 PostgreSQL LISTEN/NOTIFY。
- 按 L2-08 评估 Conversation 级锁粒度。
- 增加 SSE 断连与连接池故障注入的持续压力测试。