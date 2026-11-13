# Handoff: PHASE3-20261113 环境可移植性、生命周期与清理

- 状态：DONE
- 主责 Agent：CORE / WORKER / QA
- 分支 / 提交：main / 本次阶段三本地提交
- 开始时间：2026-11-13
- 完成时间：2026-11-13

## 目标

修复阶段三 O2/O3/B5/B6/B7/T2/T3：启动失败阶段化日志、gRPC 线程显式生命周期、Docker-first 开发入口、工具探测与不可达测试清理。保持 C++ Drogon Core、Go Worker、PostgreSQL lease、REST/SSE + gRPC 与既有端口/协议不变。

## 根因

- O2：Core 启动的数据库连接和迁移步骤没有独立阶段日志，异常最终只显示通用 fatal 文案。
- O3：gRPC 服务线程被 detach，HTTP 退出后没有显式 Shutdown + join。
- B5：本地脚本依赖未提交且不可自动准备的 `.tools/`，仓库缺少可执行入口。
- B6：契约、Test、E2E 脚本直接调用 `python`/`protoc`/`.tools`，缺工具时没有统一的可执行处理方式。
- B7：维护契约和自动同步 Skill 写死旧工作区绝对路径。
- T2：`tests/e2e.py` 的 `login()` 在 return 后保留不可达重复代码。
- T3：helper 默认地址可安全回退到 `127.0.0.1:50051`，但实际可通过 `GRPC_ADDR` 覆盖；容器 E2E 已使用 `core:50051`。

## 完成内容

- O2：Core 增加 `config.load`、`database.connect`、`database.migrate`、`grpc.start`、`http.start`、`shutdown` 阶段日志；连接失败不打印 `DATABASE_URL`，迁移失败追加迁移文件名。
- O3：gRPC 服务改为共享所有权 server handle；启动后由主线程持有，HTTP 退出时先 `Shutdown()` 再 `join()`，无 detached 线程。
- B5：新增 `scripts/setup-dev.ps1`，提供 Docker-first env/Compose/build/up/health 引导；`build.ps1`、`dev.ps1` 缺 `.tools/` 时给出明确入口。
- B6：`test-contracts.ps1`、`test.ps1`、`e2e.ps1` 探测环境变量、`.tools/` 和 PATH；新增 `-Docker` 全量执行路径，覆盖 Python/PyYAML/protoc、Go、Node 与 E2E helper。
- B7：将旧绝对仓库路径改为运行时 `git rev-parse --show-toplevel` / 相对仓库描述。
- T2/T3：删除 E2E 不可达登录代码；容器 E2E 使用 `GRPC_ADDR=core:50051` 且 helper 构建/执行通过。
- 更新 README、测试与运维 Runbook、任务板和本交接。

## 修改文件

- `AGENTS.md`
- `.agents/skills/github-auto-sync/SKILL.md`
- `README.md`
- `server/src/main.cpp`
- `server/src/grpc_server.hpp`
- `server/src/grpc_server.cpp`
- `server/src/database.cpp`
- `scripts/setup-dev.ps1`
- `scripts/build.ps1`
- `scripts/dev.ps1`
- `scripts/test-contracts.ps1`
- `scripts/test.ps1`
- `scripts/e2e.ps1`
- `tests/e2e.py`
- `docs/runbook/A13-testing.md`
- `docs/runbook/A14-operations.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PHASE3-20261113-lifecycle-portability.md`

## 契约变更

- 无。OpenAPI、Protobuf、SSE、错误码、端口、数据库 schema/migration 与模块职责均未改变。
- 未引入 Redis、Kafka、React、Vue、新服务或并行 Run。

## 验证命令与结果

```text
git fetch origin --prune
PASS：origin/main 仍为 03ecffd；提交前 HEAD 为 8e4cccb，本地领先 3 个提交。

git diff --check
PASS。

PowerShell parser（scripts/*.ps1）
PASS。

docker compose --env-file .env -f deploy/docker-compose.yml config -q
PASS。

docker compose --env-file .env -f deploy/docker-compose.yml build core worker
PASS：Core 与 Worker 镜像构建成功。

./scripts/test.ps1 -Docker
PASS：契约、Go test、Go vet、前端语法与状态测试、完整 E2E 全部通过。

契约校验
PASS：13 OpenAPI operations / 6 RPC / 5 SSE events。

Protobuf descriptor 编译
PASS：workspace_contract.pb 生成成功。

./scripts/test-lease.ps1
PASS：错误 lease 返回 JOB_LEASE_INVALID；取消后 lease 过期保持 cancelled，无 assistant message。

./scripts/test-health.ps1
PASS：PostgreSQL 停机返回 503 degraded/unavailable；恢复后返回 200。

O2 连接失败验证
PASS：`stage=database.connect`，fatal 日志包含同一 stage，未输出连接串凭据。

O2 迁移失败验证
PASS：`stage=database.migrate directory=/missing`，fatal 日志包含 migration stage。

缓存工具链验证
PASS：contract-tools 镜像可重复构建并复用 Docker 缓存；Go test/vet 与 E2E helper 使用命名卷复用 module cache。

O3 停机验证
PASS：Compose stop core 后 exit code 0，日志出现 `startup stage=shutdown`；重启后 health 200。

工具探测验证
PASS：宿主缺少 protoc 时返回可执行的 `./scripts/test-contracts.ps1 -Docker` 提示；Docker E2E 使用 `core:50051` 通过。
```

## 验收映射

- O2：连接和迁移失败均可从阶段日志定位，敏感连接信息未进入日志。
- O3：无 detached gRPC 线程；正常 SIGTERM 后 Shutdown/join 完成且进程 exit 0。
- B5：Docker-first 入口可执行；可选 `.tools/` 路径有明确缺失提示。
- B6：宿主工具缺失时不再静默失败；`-Docker` 路径可在无 Python/protoc/Go 宿主环境执行。
- B7：两个维护文档中的旧绝对路径已移除。
- T2/T3：不可达代码已删除；容器 helper 地址由 `GRPC_ADDR` 覆盖并通过 E2E。
- 回归：lease、retry 幂等、类型校验、并发 send、重复回调、取消、SSE 回放、前端状态和 health 均通过。

## 已知问题与风险

- 宿主机 PATH 仍无全局 `python`/`go`/`protoc`，但不影响验收：contract-tools 镜像和 Go/Node 官方镜像覆盖全部检查。
- 宿主 E2E 在沙箱内仍可能因本机 socket 权限被拒绝；Docker E2E 使用 Compose 内部网络，已覆盖同一测试。
- 当前 GitHub 账号无仓库写权限，按用户指令未 push。

## 需要其他 Agent 处理的事项

- 获得写权限后推送 `main`；不要 amend 既有提交。

## 下一任务建议

- 将 `test.ps1 -Docker` 与 lease/health 容器回归纳入 CI；contract-tools 镜像和 Go module 命名卷已提供本地缓存基础。
