# AI Workspace V2.0 · 基础对话平台

本仓是按 `AGENTS.md` 与 `docs/implementation-checklist.md` 交付的基础对话平台纵向切片。

## 架构

```text
Browser -> Drogon HTTP/SSE :8080 -> PostgreSQL :5432
                         \-> gRPC :50051 -> Go Worker -> model HTTPS / mock model
```

- 前端仅使用原生 HTML/CSS/JavaScript，且只访问 C++ `8080`。
- C++ 是唯一 PostgreSQL 访问者；Go Worker 只通过 gRPC 与 C++ 通信。
- Mock 模式默认启用，无需外部模型密钥即可验收完整链路。
- 内部端口 `50051`、`5432` 不向宿主机公网发布。

## 快速启动

```powershell
Copy-Item .env.example .env
docker compose --env-file .env -f deploy/docker-compose.yml up --build
```

浏览器打开 `http://localhost:8080`。本地演示账号：`demo / demo123`。

无 Docker 的本地开发：

```powershell
./scripts/dev.ps1
```

## 验证

```powershell
./scripts/test-contracts.ps1
./scripts/build.ps1
./scripts/test.ps1
```

E2E 默认使用 Mock 模型，断言登录、Agent、会话、消息、SSE、落库、取消/重复回调幂等。

## 文档索引

- REST：`api/openapi.yaml`
- gRPC：`proto/workspace.proto`
- SSE：`docs/contracts/sse-events.md`
- 错误码：`docs/contracts/error-codes.md`
- 数据设计：`docs/schema/erd.md`
- 部署：`docs/runbook/A03-deployment.md`
- 使用与故障排查：`docs/runbook/A14-operations.md`
- 交接记录：`docs/handoffs/`

## 本地工具链

仓内 `.tools/` 仅用于本机验证且已被忽略。生产与 CI 使用 `deploy/` 中的固定版本镜像，不依赖宿主机全局安装。