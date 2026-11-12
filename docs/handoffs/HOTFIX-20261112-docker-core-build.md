# Handoff: HOTFIX-20261112 Docker Core 构建依赖

- 状态：DONE
- 主责 Agent：COORD / QA
- 分支 / 提交：main / 本地提交（自动同步推送受网络阻塞）
- 开始时间：2026-11-12
- 完成时间：2026-11-12

## 目标

让仓库 README 中的 Docker Compose 安装路径可以在干净环境完成构建并启动。

## 完成内容

- 定位并修复 Core 镜像缺少 Drogon 构建依赖的问题。
- 固定使用官方 `drogonframework/drogon:v1.9.14` 的静态库与 CMake 配置，避免 Ubuntu 24.04 自带 Drogon 1.8.7 与 SSE API 不兼容。
- 将 Protobuf 查找改为 CMake 模块模式，兼容 Ubuntu 包不提供 `ProtobufConfig.cmake` 的布局。
- 为 Core 的只读根文件系统挂载 `/app/uploads` tmpfs，消除 Drogon 上传临时目录启动报错。
- Compose 栈成功启动：PostgreSQL、Core、Worker 均运行，Core 健康检查通过。

## 修改文件

- `server/Dockerfile`
- `CMakeLists.txt`
- `deploy/docker-compose.yml`
- `docs/TASK_BOARD.md`
- `docs/handoffs/HOTFIX-20261112-docker-core-build.md`

## 契约变更

- 无 API、数据库、SSE 或 gRPC 契约变更。

## 验证命令与结果

```text
docker compose --env-file .env -f deploy/docker-compose.yml build core       PASS
docker compose --env-file .env -f deploy/docker-compose.yml up -d            PASS
docker compose --env-file .env -f deploy/docker-compose.yml config -q        PASS
http://127.0.0.1:8080/api/v1/health                                         200 {"database":"ok","status":"ok","version":"1.0.0"}
http://127.0.0.1:8080/                                                      200
tests/e2e.py                                                                PASS
tests/validate_contracts.py                                                 PASS
```

E2E 覆盖登录、Origin 拒绝、Agent、会话、消息、SSE、重复回调、事件回放、跨用户拒绝、取消和重试。

## 已知问题与风险

- 2026-11-12 自动同步已创建本地提交，但 git push 因无法连接 github.com:443 失败；远端 main 未更新。
- 首次构建需要拉取官方 Drogon 1.9.14 构建镜像，网络较慢时耗时会明显增加。
- 运行镜像仍使用 Ubuntu 24.04，并已按实际动态依赖精简。