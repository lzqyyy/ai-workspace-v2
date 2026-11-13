# 基础对话平台任务看板

更新时间：2026-11-13
当前版本：V0.1.1

## 当前任务

| 任务 | 主责 | 状态 | 交付/证据 |
|---|---|---|---|
| G00 独立仓与目录骨架 | COORD | DONE | 独立 Git 仓、CMake、目录与 README |
| G01 冻结契约 | COORD | DONE | OpenAPI 3.1、Proto、SSE、错误码、ADR |
| G02 数据库 ER 与迁移 | CORE | DONE | ER 图、4 个幂等迁移、空库自动初始化 |
| G03 QA 运行骨架 | QA | DONE | Mock Provider、Compose、运行脚本 |
| G10 C++ 基础 REST | CORE | DONE | 登录、Agent、会话、消息、权限 |
| G11 Run/Job 编排与 SSE | CORE | DONE | 事务创建、租约、事件、回放、取消 |
| G12 C++ gRPC 服务 | CORE | DONE | 6 RPC |
| G13 Worker 主循环与租约 | WORKER | DONE | Claim、Heartbeat、完成/失败/取消 |
| G14 模型适配器与 Mock | WORKER | DONE | Mock + OpenAI-compatible |
| G15 前端登录与对话 | WEB | DONE | 原生 HTML/CSS/JS 工作台 |
| G16 SSE 与断线回放 | WEB | DONE | EventSource、游标持久化、刷新恢复 |
| G17 权限、审计与敏感信息 | CORE | DONE | 所有者隔离、审计脱敏 |
| G18 超时、取消、重试、幂等 | CORE/WORKER | DONE | 租约回收、双回调幂等 |
| G20 集成与 E2E | QA | DONE | `tests/e2e.py` PASS |
| G21 部署包与 Runbook | QA | DONE | Compose、Dockerfile、A03/A14 |
| G22 发布审查与归档 | COORD | DONE | V0.1 验收记录 |
| H01 Demo 密码策略对齐 | COORD/CORE/WEB | DONE | `004_demo_password`、8 位策略、E2E |
| H02 本地 Origin 允许列表 | COORD/CORE | DONE | localhost/127.0.0.1 允许，其他拒绝 |
| HOTFIX-20261112 Docker Core 构建依赖 | COORD/QA | DONE | 固定 Drogon 1.9.14，Compose 构建/启动/E2E 通过 |
| HOTFIX-20261113 Core 一致性与健康检查 | CORE/WEB/QA | DONE | Lease 校验、retry 幂等、字段类型、会话切换 SSE、health 503；专项与 E2E PASS |
| A00 AI 多 Agent 维护契约 | COORD | DONE | `AGENTS.md` V1.1、分支与单写者协议 |
| A01 GitHub 自动同步 | COORD | DONE | `.agents/skills/github-auto-sync/` |

## Gate 结果

- Gate 1 契约门禁：PASS
- Gate 2 纵向链路门禁：PASS
- Gate 3 安全与可靠性门禁：PASS
- Gate 4 交付门禁：PASS（本机无 Docker，使用等价本地进程验证）

## L2 后续任务

| 任务 | 建议主责 | 状态 | 说明 |
|---|---|---|---|
| L2-01 中心化多用户部署 | QA | TODO | HTTPS、账号管理、Worker 扩容、备份 |
| L2-02 Project / ProjectMember | CORE | TODO | owner/editor/viewer，共享 Agent |
| L2-03 私有 Conversation | CORE/WEB | TODO | 共享 Agent、每人独立会话上下文 |
| L2-04 会话级 SSE | CORE/WEB | TODO | 共享会话实时同步 |
| L2-05 Task / Handoff | COORD/CORE | TODO | 第二协作阶段 |
| L2-06 双 Owner / 双 Agent | COORD | TODO | 多所有者与多 Agent 协作 |
| L2-07 第二模型 Provider | WORKER | TODO | 保持统一错误模型 |

## 变更规则

任务状态、主责、写入范围或依赖发生变化时，必须更新本文件，并在 `docs/handoffs/` 建立对应交接。
