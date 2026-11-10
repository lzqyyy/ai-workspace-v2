# 基础对话平台：实施范围、任务与交付清单

- 版本：V0.1
- 日期：2026-11-10
- 定位：按现有架构先交付“能完整跑通的一次基础对话”，不直接展开完整 V2.0 插件平台
- 依据：`AI_Workspace_PRD_V1.0.docx`、`AI_Workspace_V2.0_轻量化插件平台与个性化定制指导文档.docx`、`轻量化协作平台_技术架构与交付清单.xlsx`、`轻量化协作平台_系统通信接口与端口规范_V1.0.xlsx`、`A01_总体系统架构图_V1.0.docx`、`_work/a02_module_map.html`

## 1. 先定范围

“基础对话平台”建议拆成四个层次，当前只承诺 L0 和 L1：

| 层级 | 目标 | 是否现在做 |
|---|---|---|
| L0 技术闭环 | 浏览器发消息，C++ 建 Run/Job，Go Worker 调模型，SSE 回流，消息落库 | 是 |
| L1 基础对话 MVP | 用户、Agent、会话、历史、模型切换、流式输出、失败重试、基础权限与审计 | 是 |
| L2 协作 MVP | 双 Owner、双 Agent、Project、任务交接、上下文隔离、两种模型 Provider | 下一阶段 |
| L3 V2.0 扩展平台 | 插件 Manifest、贡献点、Profile、主题、命令面板、第三方生态 | 暂不做 |

首个可验收版本确定为：

> 用户登录后创建或选择一个 Agent，发起会话，发送一条消息；系统异步调用一个模型并流式返回；刷新或重启后历史仍存在；模型失败可见、可重试；用户不能读取其他用户的会话。

这不是最终产品验收，但它是后续项目协作、任务系统和插件系统的真实底座。先跑通一条纵向链路，比先铺开所有页面和模块更可靠。

## 2. 架构基线

### 2.1 必须遵守的约束

1. 前端只用 HTML、CSS、JavaScript，不引入 React、Vue、Next.js。
2. C++ 是核心业务后端和唯一业务数据持久化入口。
3. Go Worker 只负责异步执行、模型调用和工具调用，不直接连接 PostgreSQL。
4. 浏览器只连 C++：REST + JSON，实时进度用 SSE。
5. C++ 与 Go Worker 用 gRPC + Protobuf，默认 `50051`，仅内部网络。
6. PostgreSQL 默认 `5432`，仅 C++ 访问。
7. 外部模型 API 由 Go Worker 出站访问 `443`。
8. API Key 只存在 Worker 侧环境变量或密钥存储中，不进入前端、数据库明文和普通日志。
9. 不新增面向浏览器的业务端口，生产环境由 443 反向代理到 C++ 8080。
10. 不引入 Redis、Kafka、微服务或自研消息队列；当前任务队列先复用 PostgreSQL。

### 2.2 已发现的基线问题

- PRD V1.0 中的 React/Next.js/FastAPI 是候选方案，已被 V2.0 技术基线覆盖，开发时不得混用。
- Drogon 仍是“待确认”，但当前应作为默认方案完成一次最小验证；在冻结 API 和数据库之前确认即可。
- 文件/对象存储是“待确认”，基础对话不需要附件，本轮暂不实现。
- A02 模块划分文档已完成（`A02_模块划分文档\A02_模块划分文档_V1.0.docx`）；基础对话的前端主视图仍建议明确放入 `FE-01 Workbench Shell`，或在后续版本补充 `FE-05 Conversation Workspace`。
- 当前 Workspace 只有 PRD、架构图和检查材料，没有平台源码。代码应放入新的独立子目录，避免继续混在现有个人工作区文件中。

## 3. 本轮模块切片

### 3.1 使用到的模块

| 层 | 模块 | 本轮实现内容 |
|---|---|---|
| 前端 | FE-01 Workbench Shell | 登录页、Agent 列表、会话列表、消息主区、输入区、运行状态 |
| 前端 | FE-04 Core API & Event Client | REST 请求、SSE 订阅、错误归一化、断线重连 |
| C++ | CORE-01 Identity & Access | 登录、当前用户、会话身份 |
| C++ | CORE-03 Agent Registry | Agent 创建、查询、Owner 关系、模型配置引用 |
| C++ | CORE-05 Communication | 会话与消息 CRUD、参与者、消息顺序 |
| C++ | CORE-06 Context Engine（最小版） | 只组装当前会话历史，不注入私人文件或跨项目资料 |
| C++ | CORE-09 Permission Gateway（最小版） | 检查用户是否有权访问 Agent、会话和 Run |
| C++ | CORE-10 Execution Orchestrator | Run/Job 状态机、领取、租约、超时、失败、重试、幂等 |
| C++ | CORE-12 Event Gateway | 事件持久化、SSE、游标与断线回放 |
| C++ | CORE-13 Audit & Activity | 记录登录、Agent、消息、Run 和权限拒绝事件 |
| C++ | CORE-14 Data & File Adapter | PostgreSQL 连接、事务、迁移和仓储 |
| Go Worker | WK-01 Worker Kernel | 领取 Job、心跳、执行超时、取消、优雅退出 |
| Go Worker | WK-02 Agent Runtime（最小版） | 一次模型对话执行、输入组装、流式事件输出 |
| Go Worker | WK-03 Capability Broker（最小版） | 统一模型适配、出站校验、错误与用量记录 |

### 3.2 本轮不实现的模块

- CORE-02 Team / Project：L2 再做，但数据库和 API 预留 `project_id` 可空字段。
- CORE-04 Task & Handoff：L2 再做。
- CORE-07 Extension Registry、CORE-08 Settings & Profile、CORE-11 Artifact & Version：V2.0 阶段再做。
- FE-02 Contribution UI、FE-03 Personalization：V2.0 阶段再做。
- WK-04 Extension Host、WK-05 Result Packager：V2.0 阶段再做。

## 4. 端到端数据流

```text
浏览器
  -> POST /api/v1/conversations/{id}/messages
C++ 核心
  -> 鉴权、检查 Agent/会话访问权限
  -> 事务写入 user message、run、job
  -> 返回 202 + run_id
Go Worker
  -> gRPC ClaimNextJob
C++ 核心
  -> 只返回当前会话的授权上下文
Go Worker
  -> HTTPS 调用模型 Provider
  -> 流式产生 delta 事件
  -> gRPC AppendEvent / UpdateJobStatus
C++ 核心
  -> 保存 run_events，通过 SSE 推送
  -> 成功时写入最终 assistant message
浏览器
  -> EventSource 接收并渲染
```

必须支持 SSE `Last-Event-ID` 或等价游标，确保页面短暂断线后能补齐事件。

## 5. 最小接口契约

### 5.1 REST API

| 方法 | 路径 | 用途 | 关键响应 |
|---|---|---|---|
| POST | `/api/v1/auth/login` | 登录 | 会话 Cookie 或访问令牌、当前用户 |
| GET | `/api/v1/me` | 当前用户 | 用户 ID、显示名 |
| GET | `/api/v1/agents` | 查询自己的 Agent | Agent 列表 |
| POST | `/api/v1/agents` | 创建 Agent | Agent ID、名称、模型配置 |
| GET | `/api/v1/conversations` | 查询会话 | 会话列表 |
| POST | `/api/v1/conversations` | 创建会话 | 会话 ID |
| GET | `/api/v1/conversations/{id}/messages` | 查询历史消息 | 按时间排序的消息 |
| POST | `/api/v1/conversations/{id}/messages` | 发送消息 | `202`、run_id、SSE 地址 |
| GET | `/api/v1/runs/{id}/events` | 订阅运行事件 | `text/event-stream` |
| POST | `/api/v1/runs/{id}/cancel` | 取消运行 | 当前状态 |
| GET | `/api/v1/health` | 健康检查 | 依赖状态（不泄露敏感信息） |

SSE 事件最小集合：

- `run.started`
- `message.delta`
- `message.completed`
- `run.failed`
- `run.cancelled`
- `heartbeat`

### 5.2 gRPC / Protobuf

至少冻结以下 RPC：

- `JobService.ClaimNextJob`
- `JobService.Heartbeat`
- `JobService.UpdateJobStatus`
- `ContextService.GetExecutionContext`
- `RunEventService.AppendEvent`
- `PermissionService.CheckPermission`

消息字段至少包含：`request_id`、`run_id`、`job_id`、`agent_id`、`conversation_id`、`lease_token`、`event_seq`、`status`、`error_code`，用于追踪、幂等和重试。

## 6. 最小数据模型

| 表 | 关键字段 | 说明 |
|---|---|---|
| `users` | id、username、password_hash、status、created_at | 人类用户 |
| `agents` | id、owner_id、name、avatar、system_prompt、model_provider_id、status | Agent 身份与模型解耦 |
| `model_providers` | id、name、provider_type、base_url、secret_ref、enabled | 只存密钥引用，不存明文密钥 |
| `conversations` | id、owner_id、agent_id、project_id、title、status | 本轮 `project_id` 可空 |
| `messages` | id、conversation_id、sender_type、sender_id、role、content、run_id、status、created_at | 用户消息和最终 Agent 消息 |
| `runs` | id、conversation_id、agent_id、trigger_message_id、status、model_provider_id、error_code、started_at、finished_at | 一次业务执行 |
| `jobs` | id、run_id、status、lease_token、lease_until、attempt_count、max_attempts | Worker 领取与重试 |
| `run_events` | id、run_id、seq、event_type、payload_jsonb、created_at | SSE 回放与过程审计 |
| `audit_events` | id、actor_type、actor_id、action、resource_type、resource_id、result、request_id、created_at | 不记录敏感原文 |

消息、Run、Job 的创建应在一个数据库事务内完成，避免出现“有消息但没有执行任务”的半成品状态。

## 7. 任务拆解与完成标准

| 编号 | 任务 | 具体交付 | 完成标准 | 依赖 |
|---|---|---|---|---|
| B01 | 建独立代码仓与骨架 | `frontend/`、`server/`、`worker/`、`proto/`、`api/`、`db/`、`deploy/`、`docs/` | 各目录可独立构建，README 能说明启动顺序 | 无 |
| B02 | 冻结契约 | OpenAPI 3.1、`proto`、SSE 事件表、错误码表 | 前后端和 Worker 可据此并行开发，字段有示例 | B01 |
| B03 | 数据库设计 | ER 图、建表 SQL、迁移脚本、种子数据 | 新库可一次迁移；重复执行不破坏数据 | B02 |
| B04 | C++ 基础业务 | 登录、用户、Agent、会话、消息 REST | 用户只能访问自己的数据；消息顺序稳定 | B02、B03 |
| B05 | C++ 执行编排 | Run/Job、领取、租约、取消、超时、幂等 | 重复回调不会重复写消息；超时任务可回收 | B03、B04 |
| B06 | C++ gRPC 服务 | 6 个最小 RPC + protobuf 生成 | Worker 可领取 Job、提交事件和最终状态 | B02、B05 |
| B07 | Go Worker | Worker 循环、模型适配、流式事件、重试 | 用一个模型跑通完整回复；异常有可读错误码 | B06 |
| B08 | 前端对话界面 | 登录、Agent、会话、消息、流式状态、错误提示 | 浏览器可完整发起并收到回复；刷新不丢历史 | B04、B05 |
| B09 | 权限与审计 | 最小资源权限、权限拒绝、关键操作审计 | 越权访问被拒绝；日志含 request_id，但不含敏感原文 | B04、B05 |
| B10 | 可靠性 | 超时、重试、取消、幂等、SSE 重连 | 重复请求不重复创建消息；断线可补齐事件 | B05-B08 |
| B11 | 测试 | 单元测试、API 测试、gRPC 测试、一条 E2E | E2E 能自动跑通“发送消息 -> Mock 模型 -> 流式完成 -> 消息落库” | B03-B10 |
| B12 | 部署与使用文档 | Docker Compose、环境变量模板、迁移脚本、启动说明、故障排查 | 新环境按文档启动；不暴露 50051/5432 到公网 | B01-B11 |

## 8. 交付物清单

### 8.1 本轮代码与运行交付

| 编号 | 交付物 | 验收标准 |
|---|---|---|
| D01 | 独立代码仓 | 目录清晰，构建命令可复现 |
| D02 | OpenAPI 契约 | 所有前端调用都有定义，示例可执行 |
| D03 | Protobuf 契约 | C++ 与 Go 可生成客户端/服务端代码 |
| D04 | 数据库迁移包 | 空库可初始化，可重复执行 |
| D05 | C++ 核心服务 | REST、SSE、gRPC、PostgreSQL 事务可用 |
| D06 | Go Worker | 可领取 Job、调用模型、回传流式事件 |
| D07 | 前端基础对话工作台 | 登录、选 Agent、聊天、流式显示、历史查询可用 |
| D08 | 本地部署包 | `docker compose up` 或等价命令可启动 |
| D09 | 测试与验收记录 | 至少一条自动化 E2E，附手动验收步骤 |
| D10 | README / Runbook | 环境变量、构建、启动、迁移、回滚、排查齐全 |

### 8.2 架构文档交付映射

| 文档 | 本轮状态 | 说明 |
|---|---|---|
| A01 总体系统架构图 | 已有 | 保留，不再重复画 |
| A02 模块划分 | 已完成 | `A02_模块划分文档_V1.0.docx`；对话主视图边界可在实现时进一步明确 |
| A03 部署架构图 | 本轮需交付 | 进程、端口、网络、数据库、反向代理 |
| A04 前后端 REST API | 本轮需交付 | 对应 D02 |
| A05 C++ / Go gRPC 接口 | 本轮需交付 | 对应 D03 |
| A06 数据库设计 | 本轮需交付 | 对应 D04 |
| A07 权限设计 | 本轮交最小版 | 用户、Agent、会话、Run 的访问规则 |
| A08 Agent 执行流程 | 本轮需交付 | Run/Job 状态机与事件流 |
| A09 插件系统设计 | 暂缓 | V2.0 插件阶段 |
| A10 多 Agent 协作设计 | 暂缓 | L2 协作阶段 |
| A11 版本管理设计 | 暂缓 | 成果物阶段 |
| A12 错误与重试机制 | 本轮需交付 | 超时、失败、重试、幂等、日志 |
| A13 测试文档 | 本轮需交付 | 对应 D09 |
| A14 部署与使用说明 | 本轮需交付 | 对应 D10 |

V2-D01 至 V2-D08 不在本轮交付范围，避免在基础对话尚未跑通时提前建设插件生态。

## 9. 验收标准

### 9.1 功能验收

- [ ] 用户可以登录并创建自己的 Agent。
- [ ] 用户可以创建会话并发送消息。
- [ ] C++ 创建 Run 和 Job，并返回 `run_id`。
- [ ] Go Worker 能领取任务并调用模型。
- [ ] 浏览器能通过 SSE 看到流式回复。
- [ ] 完成后的用户消息和 Agent 消息写入 PostgreSQL。
- [ ] 刷新页面或重启服务后历史消息仍可查看。
- [ ] 模型失败时显示明确错误，用户可以重试。
- [ ] 取消运行后不再产生新的模型输出。
- [ ] 用户不能读取其他用户的 Agent 和会话。

### 9.2 架构验收

- [ ] 浏览器没有直接访问 Worker、PostgreSQL 或模型 API。
- [ ] Go Worker 没有直接连接数据库。
- [ ] API Key 不出现在前端响应、数据库明文和普通日志中。
- [ ] 所有前端请求带 `request_id`，所有 Run 事件可追踪到 `run_id`。
- [ ] Worker 超时或崩溃后，Job 可以被回收，不会永久卡死。
- [ ] SSE 断线重连后能补齐缺失事件，不重复生成最终消息。
- [ ] `50051` 和 `5432` 不默认暴露到公网。
- [ ] 权限拒绝有审计记录，但日志不保存敏感消息原文。

### 9.3 自动化验收

至少保留一条 E2E：

1. 启动 PostgreSQL、C++、Go Worker 和 Mock 模型服务。
2. 创建用户和 Agent。
3. 创建会话并发送消息。
4. Mock 模型返回固定流式片段。
5. 断言 SSE 收齐、消息落库、Run 状态为成功。
6. 重复投递同一回调，断言不产生重复消息。

## 10. 待确认项与建议默认值

| 事项 | 建议默认值 | 何时必须确认 |
|---|---|---|
| C++ Web 框架 | Drogon | B01 结束前 |
| 数据库 | PostgreSQL 16 | B03 开始前 |
| 前端框架 | 原生 HTML/CSS/JS 模块化 | B08 开始前 |
| 登录方式 | 同源 HttpOnly Session Cookie | B04 开始前 |
| Worker 队列 | PostgreSQL `jobs` 表 + 租约 | B05 开始前 |
| SSE 事件格式 | 统一 envelope + `Last-Event-ID` | B02 结束前 |
| 第一个模型 Provider | 选一个可用且支持流式的 Provider | B07 开始前 |
| 第二个模型 Provider | L2 再做，但接口按多 Provider 设计 | L2 验收前 |
| 文件存储 | 本轮不做 | 有附件需求时 |
| Team / Project | 字段预留，L2 实现 | L2 开始前 |
| 插件系统 | 本轮不做 | 基础对话稳定后 |

## 11. 建议执行顺序

1. **先冻结契约**：OpenAPI、proto、数据库表、SSE 事件、错误码。
2. **先跑通 Mock 模型**：不要等真实 Provider 接好才开始做端到端。
3. **按纵向链路开发**：登录 -> 创建 Agent -> 发消息 -> Run/Job -> Worker -> SSE -> 落库。
4. **最后加权限和可靠性**：资源归属检查、租约、超时、幂等、SSE 回放。
5. **通过 E2E 后再扩 L2**：Project、双 Owner、双 Agent、任务交接、第二 Provider。

## 12. 当前最应该做的三件事

1. 建立独立代码仓和 B01 目录骨架。
2. 写出 B02 的 OpenAPI、proto 和 SSE 事件契约。
3. 用 Mock 模型打通 C++ -> Go Worker -> SSE -> 前端的最小闭环。

完成这三件事后，再讨论插件、Project、Task、Profile；否则很容易在业务对象还没有闭环时堆出一套无法验收的界面。
