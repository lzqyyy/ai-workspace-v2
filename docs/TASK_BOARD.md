# 基础对话平台任务看板

更新时间：2026-11-19
当前候选版本：V0.2.0（REVIEW）
发布状态：分支已推送 `origin`；未合并 `main`；未创建 `V0.2.0` tag

## 当前任务

| 任务 | 主责 | 状态 | 交付/证据 |
|---|---|---|---|
| G00 独立仓与目录骨架 | COORD | DONE | 独立 Git 仓、CMake、目录与 README |
| G01 冻结契约 | COORD | DONE | OpenAPI 3.1、Proto、SSE、错误码、ADR |
| G02 数据库 ER 与迁移 | CORE | DONE | ER 图、只增幂等迁移、空库自动初始化 |
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
| PHASE2-20261113 阶段二边界与稳定性修补 | CORE/WEB/QA | DONE | 并发 send 幂等、取消优先于 lease 过期、未知字段/长度校验、状态契约、retry 防重复与恢复测试；构建/E2E/专项 PASS |
| A00 AI 多 Agent 维护契约 | COORD | DONE | `AGENTS.md` V1.1、分支与单写者协议 |
| A01 GitHub 自动同步 | COORD | DONE | `.agents/skills/github-auto-sync/` |
| PHASE3-20261113 环境可移植性、生命周期与清理 | CORE/WORKER/QA | DONE | 阶段化启动日志、gRPC Shutdown/join、Docker-first 缓存工具镜像、路径清理；构建/E2E/lease/health PASS |
| PACKAGE-A-20261116 多人平台基础数据模型与 API 契约 | CORE/COORD | REVIEW | Project/成员/Agent schema、只增迁移、Project-first OpenAPI、契约测试与交接；Docker/PG16/E2E PASS |
| PACKAGE-B-20261116 Project API 与权限 | CORE | REVIEW | Project/成员/Agent C++ 路由、owner/editor/viewer 授权、最后 owner 与 active Run 移除保护、脱敏审计；Docker-first 全量测试 PASS |
| PACKAGE-C-20261116 共享 Conversation 与单 active Run 切换 | CORE | REVIEW | 已 rebase 到 Package B；Project-first Conversation、target Agent 发送、Project member Run/SSE 权限、单 active Run 与 2H+2A 测试；Docker-first 全量测试 PASS |
| PACKAGE-D-20261116 Project-first 多用户前端 | WEB | REVIEW | Project 切换/创建、成员/Agent 管理、共享 Conversation、target Agent 发送、共享 Run SSE、角色交互控制；状态测试与 headless 浏览器 E2E PASS |
| PACKAGE-E-20261116 2H+2A 验收与稳定性 | QA/CORE | REVIEW | 双浏览器 owner/editor + 两 Agent 同会话、双方 SSE、权限矩阵、并发 409、幂等、迁移重放；Docker-first 全量 PASS |
| PACKAGE-F-20261116 交付、演示与交接 | QA/COORD | REVIEW | Docker-first demo seed、2H+2A 手工/自动验收入口、健康证据、Runbook、验收记录与最终交接 |
| HOTFIX-20261116 MVP 合并审查阻塞 | CORE/QA | REVIEW | UUID 严格格式校验、双独立浏览器 2H+2A 竞态修复、Project 归档契约缺口记录；全量/浏览器/health PASS |
| A02 项目级免重复审批 | COORD | DONE | `.agents/skills/project-auto-approval/`、`.codex/config.toml` |
| HOTFIX-20261117 共享历史轮询竞态 | WEB/QA | REVIEW | 防止较早的慢响应覆盖较新的共享消息快照；修复 Windows 浏览器 profile 清理误报；全量/浏览器 PASS |
| HOTFIX-20261118 SSE 事件序号与窄屏回归 | CORE/WEB/QA | REVIEW | appendEvent 强制连续序号；跳号/乱序专项 PASS；窄屏侧栏关闭修复；Docker/E2E/Playwright PASS |
| DOC-20261117 当前事实与交付基线校准 | COORD | REVIEW | 版本/迁移/前端接入/push 状态、归档与幂等边界、UTF-8 长度语义修正；契约与本地验证通过 |
| IMP-20261119 Run 归属与发送幂等合同 | CORE/COORD | REVIEW | `client_request_id` 必填、`runs.initiated_by_user_id`、008 迁移、retry/取消权限、OpenAPI 1.0.1；Docker 全量与双浏览器 2H+2A PASS |

## Gate 结果

- Gate 1 契约门禁：PASS
- Gate 2 纵向链路门禁：PASS
- Gate 3 安全与可靠性门禁：PASS
- Gate 4 交付门禁：PASS（Docker-first 构建、E2E、lease 与 health 验证）
- Gate 5 最小多人 MVP：PASS（双浏览器 2H+2A、双方 SSE、权限矩阵、迁移重放与单用户回归）

## L2 后续任务

| 任务 | 建议主责 | 状态 | 说明 |
|---|---|---|---|
| L2-01 中心化多用户部署 | QA | TODO | HTTPS、账号管理、Worker 扩容、备份 |
| L2-02 Project / ProjectMember | CORE | REVIEW | Package A/B/C 完成契约、迁移、成员/Agent 管理、共享 Conversation、Project member 运行权限；D/E 前端与 2H+2A 验收 PASS |
| L2-03 私有 Conversation | CORE/WEB | TODO | 共享 Agent、每人独立会话上下文 |
| L2-04 会话级 SSE | CORE/WEB | REVIEW | Project member Run SSE、回放与双浏览器订阅已验收；Conversation 级新 Run 发现仍为短轮询 |
| L2-05 Task / Handoff | COORD/CORE | TODO | 第二协作阶段 |
| L2-06 双 Owner / 双 Agent | COORD | REVIEW | 2H+2A、多 Owner 角色保护、双 Agent 选择与单 active Run 已验收；多 Run 并行后置 |
| L2-07 第二模型 Provider | WORKER | TODO | 保持统一错误模型 |
| L2-08 锁粒度优化（5B） | CORE/QA | TODO | 同 Project 写吞吐成为瓶颈时，将 Project 行锁收窄为 Conversation 级锁并对 membership/Agent 授权行加共享锁；保持固定锁顺序、active Run 409 和死锁回归；当前 5A 继续使用 |

## 变更规则

任务状态、主责、写入范围或依赖发生变化时，必须更新本文件，并在 `docs/handoffs/` 建立对应交接。
