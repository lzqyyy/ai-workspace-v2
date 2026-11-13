# A14 部署与使用说明 V1.0

## 1. 环境变量

| 变量 | 进程 | 默认值 | 说明 |
|---|---|---|---|
| `DATABASE_URL` | Core | 本地 workspace | libpq conninfo |
| `HTTP_HOST/HTTP_PORT` | Core | 0.0.0.0/8080 | 唯一浏览器入口 |
| `GRPC_HOST/GRPC_PORT` | Core | 0.0.0.0/50051 | 仅内部网络 |
| `MIGRATIONS_DIR` | Core | db/migrations | 启动时幂等迁移 |
| `APP_ORIGIN` | Core | localhost + 127.0.0.1:8080 | 逗号分隔的精确允许源，用于 CSRF Origin 校验 |
| `COOKIE_SECURE` | Core | false | 生产 HTTPS 必须 true |
| `GRPC_ADDR` | Worker | 127.0.0.1:50051 | Core gRPC 地址 |
| `WORKER_ID` | Worker | 主机名-PID | 租约审计标识 |
| `MOCK_DELAY_MS` | Worker | 250 | Mock 片段间隔 |
| `MODEL_TIMEOUT_MS` | Worker | 120000 | 单次模型超时 |
| `MODEL_API_KEY` | Worker | 空 | 仅真实 Provider 使用，禁止提交 |

## 2. Docker 启动

```powershell
Copy-Item .env.example .env
docker compose --env-file .env -f deploy/docker-compose.yml up --build -d
docker compose --env-file .env -f deploy/docker-compose.yml logs -f core worker
```

停止：

```powershell
docker compose --env-file .env -f deploy/docker-compose.yml down
```

保留数据库卷。彻底清理测试数据需显式删除卷，执行前必须确认不会丢失生产数据。

## 3. 迁移

- Core 启动时按文件名顺序执行 `db/migrations/*.sql`。
- 所有迁移使用 `IF NOT EXISTS`、`ON CONFLICT` 和 `schema_migrations`，重复执行安全。
- 已合并迁移禁止修改，只能新增下一编号迁移。
- 回滚应用使用上一镜像版本；数据库破坏性回滚必须先新增 ADR。

## 4. 本地无 Docker 开发

```powershell
./scripts/build.ps1
./scripts/dev.ps1 -NoBrowser
python tests/e2e.py
./scripts/stop-dev.ps1
```

本地开发仅使用 `.tools/` 中的便携工具链，目录已被 Git 忽略。生产不依赖该目录。

## 5. 健康检查

```powershell
curl.exe http://localhost:8080/api/v1/health
```

期望：`{"status":"ok","database":"ok","version":"1.0.0"}`，HTTP 200。数据库不可用时返回 HTTP 503 和 `{"status":"degraded","database":"unavailable","version":"1.0.0"}`；可用 `./scripts/test-health.ps1` 验证停库与恢复。

## 6. 故障排查

| 现象 | 检查 |
|---|---|
| 401 | Cookie 是否被浏览器接受；`APP_ORIGIN` 是否与访问源一致 |
| 403 | 资源是否属于当前用户；写入请求 Origin 是否允许 |
| 任务长期 queued | Worker 日志、`GRPC_ADDR`、50051 内部连通性 |
| SSE 不流动 | 反向代理是否关闭 buffering；Curl 是否可读到 chunk |
| 任务卡 running | 查看 `jobs.lease_until`；超过 30 秒后应被下一 Worker 回收 |
| 模型失败 | 看统一错误码；检查 Worker 环境是否有密钥，禁止打印密钥 |
| 数据库不可用 | Compose Postgres 健康检查、DATABASE_URL、卷权限 |
| C++ 启动失败 | `build/server.err` 或 Compose core 日志；检查迁移目录 |

## 7. 生产注意

- 仅暴露 443，反向代理到 Core 8080。
- 禁止发布 50051/5432。
- 模型密钥通过 Secret 注入 Worker。
- 反向代理、Core、Postgres 使用私有网络和最小权限账户。
- 监控：健康检查、queued 任务年龄、running 租约过期数、失败率、SSE 重连率。