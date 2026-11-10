# 基础对话平台任务看板

更新时间：2026-11-10
当前版本：V0.1

| 任务 | 主责 | 状态 | 交付/证据 |
|---|---|---|---|
| G00 独立仓与目录骨架 | COORD | DONE | 独立 Git 仓、CMake、目录与 README |
| G01 冻结契约 | COORD | DONE | OpenAPI 3.1、Proto、SSE、错误码、ADR-0001~0003；契约测试通过 |
| G02 数据库 ER 与迁移 | CORE | DONE | ER 图、3 个幂等迁移；空库自动初始化通过 |
| G03 QA 运行骨架 | QA | DONE | Mock 模型内置于 Worker，Compose/Runbook 入口完成 |
| G10 C++ 基础 REST | CORE | DONE | 登录、Agent、会话、消息、权限 |
| G11 Run/Job 编排与 SSE | CORE | DONE | 事务创建、领取、租约、事件、回放、取消 |
| G12 C++ gRPC 服务 | CORE | DONE | 6 个 RPC 编译并通过 E2E 使用 |
| G13 Worker 主循环与租约 | WORKER | DONE | Claim、Heartbeat、执行、完成/失败/取消 |
| G14 模型适配器与 Mock | WORKER | DONE | Mock 流式 + OpenAI-compatible SSE 适配 |
| G15 前端登录与对话 | WEB | DONE | 原生 HTML/CSS/JS 工作台 |
| G16 SSE 与断线回放 | WEB | DONE | EventSource、游标持久化、刷新恢复 |
| G17 权限、审计与敏感信息 | CORE | DONE | 所有者隔离、403 测试、审计脱敏 |
| G18 超时、取消、重试、幂等 | CORE/WORKER | DONE | 租约回收、取消、重试、双回调幂等测试 |
| G20 集成与 E2E | QA | DONE | `tests/e2e.py` PASS |
| G21 部署包与 Runbook | QA | DONE | Compose、Dockerfile、环境模板、A03/A14 |
| G22 发布审查与归档 | COORD | DONE | 验收记录与交接记录 |

## Gate 结果

- Gate 1 契约门禁：PASS
- Gate 2 纵向链路门禁：PASS
- Gate 3 安全与可靠性门禁：PASS
- Gate 4 交付门禁：PASS（Docker 文件已提供；本机无 Docker，使用等价的本地 UCRT64/PostgreSQL/Go 运行验证）