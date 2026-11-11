# AGENT 交付协作文件

> 项目：AI Workspace V2.0 · 基础对话平台  
> 文件版本：V1.0  
> 生效日期：2026-11-10  
> 适用范围：`D:\lzqy_\codex\Workspace` 下的基础对话平台设计、开发、测试与交付  
> 文件性质：多 Agent 协作、任务分派、交接、验收与变更控制的统一入口

## 0. 文件用途

本文件用于让多个 Agent 在不互相覆盖、不破坏架构边界的前提下，共同完成“基础对话平台”的首个可运行版本。

所有参与本项目的 Agent 在开始工作前必须：

1. 阅读本文件。
2. 阅读任务对应的输入文档和已冻结契约。
3. 确认自己的角色、任务编号、写入范围和依赖状态。
4. 只在授权范围内修改文件。
5. 完成后按统一模板提交交接记录。

本文件不是完整 PRD。产品范围和详细需求以第 2 节列出的基线文件为准。

## 1. 决策优先级

发生冲突时，按以下顺序处理：

1. 用户当前明确指令。
2. 本文件中的架构红线与安全约束。
3. 已冻结的 OpenAPI、Protobuf、数据库迁移、SSE 事件契约和 ADR。
4. `AI_Workspace_V2.0_轻量化插件平台与个性化定制指导文档.docx`。
5. `AI_Workspace_PRD_V1.0.docx`。
6. 未冻结的草案、草图和其他建议文档。

PRD V1.0 中的 React、Next.js、FastAPI 等是候选方案，不得覆盖已确认的 HTML/CSS/JavaScript、C++、Go Worker 技术基线。

任何 Agent 不得自行降低权限、修改协议或扩大范围来绕过阻塞。遇到冲突时按第 10 节升级给协调 Agent。

## 2. 项目目标与当前范围

### 2.1 本轮目标

交付一条可运行的纵向链路：

```text
浏览器
  -> REST / JSON
C++ 核心
  -> gRPC / Protobuf
Go Worker
  -> HTTPS
外部模型 API
  -> gRPC 事件回传
C++ 核心
  -> PostgreSQL 持久化 / SSE
浏览器
```

首个版本必须做到：

- 用户登录并创建或选择 Agent。
- 用户创建会话并发送消息。
- C++ 创建 Run 和 Job，返回 `run_id`。
- Go Worker 异步领取 Job 并调用模型。
- 模型输出通过 C++ 的 SSE 链路流式展示。
- 用户消息与最终 Agent 消息持久化。
- 页面刷新或服务重启后历史消息仍可读取。
- 模型失败、超时和取消都有明确状态。
- 用户不能访问其他用户的 Agent、会话和 Run。
- API Key 不出现在前端、数据库明文和普通日志中。

### 2.2 当前范围

| 层级 | 内容 | 当前状态 |
|---|---|---|
| L0 | 浏览器、C++、Go Worker、PostgreSQL、模型 API、SSE 技术闭环 | 本轮必须完成 |
| L1 | 用户、Agent、会话、消息、流式输出、基础权限、审计、错误重试 | 本轮必须完成 |
| L2 | 双 Owner、双 Agent、Project、Task/Handoff、两种模型 Provider | 下一阶段 |
| L3 | 插件 Manifest、贡献点、Profile、主题、命令面板、第三方生态 | 暂不做 |

### 2.3 明确不做

- 不实现插件市场和第三方插件执行。
- 不实现 Team、Project、Task/Handoff。
- 不实现附件、文件存储和成果物版本。
- 不实现复杂 Agent 自主循环、工具规划和多 Agent 辩论。
- 不引入 React、Vue、Next.js、FastAPI。
- 不引入微服务、Redis、Kafka 或自研消息队列。
- 不新增面向浏览器的业务端口。
- 不在本轮实现 Profile、主题、快捷键和命令面板。

## 3. 架构红线

以下规则不可由单个 Agent 自行改变：

1. 前端只使用 HTML、CSS、JavaScript。
2. 浏览器只连接 C++ 的 `8080` HTTP 服务。
3. C++ 是唯一业务数据持久化入口。
4. PostgreSQL 只允许 C++ 访问，禁止 Worker 直连。
5. C++ 与 Go Worker 只通过 gRPC + Protobuf 通信，默认 `50051`。
6. 实时事件使用 SSE，不新增 WebSocket 业务端口。
7. 模型和外部工具由 Go Worker 出站访问，默认 HTTPS `443`。
8. `50051` 与 `5432` 只开放在内部网络。
9. 模型密钥只存 Worker 侧环境变量或密钥存储，不进入源码、前端、数据库明文和日志。
10. 任务队列先使用 PostgreSQL `jobs` 表和租约，不引入独立消息队列。
11. 所有跨语言对象必须通过 Protobuf 或 JSON 明确定义，不传递语言内部对象。
12. 未授权访问默认拒绝，权限判断必须在服务端完成。

## 4. Agent 角色与权限

| 角色编号 | 角色 | 核心职责 | 默认写入范围 |
|---|---|---|---|
| COORD | 协调 / 架构 Agent | 冻结契约、拆分任务、解决冲突、合并门禁 | `AGENTS.md`、`api/`、`proto/`、`docs/contracts/`、`docs/adr/` |
| CORE | C++ 核心 Agent | REST、SSE、gRPC、PostgreSQL、Run/Job、权限与审计 | `server/`、`db/migrations/` |
| WORKER | Go Worker Agent | 领取 Job、租约、模型适配、流式事件、错误与重试 | `worker/` |
| WEB | 前端 Agent | 原生 HTML/CSS/JS 工作台、对话 UI、REST 与 SSE 客户端 | `frontend/` |
| QA | 测试 / 部署 Agent | 测试夹具、Mock 模型、集成测试、Docker Compose、Runbook | `tests/`、`deploy/`、`scripts/`、`docs/runbook/` |

### 4.1 单写者规则

- 一个任务只能有一个主责 Agent。
- 同一时间不得有两个 Agent 修改同一路径。
- 协作 Agent 需要修改非自己范围时，先提交变更请求，由 COORD 调整任务边界。
- 共享契约只能由 COORD 定稿；CORE、WORKER、WEB 只能提出变更请求。
- 已合并的数据库迁移不得修改，只能新增迁移。
- `AGENTS.md` 只能由 COORD 更新。

### 4.2 合并权限

- COORD 负责合并契约、跨边界修改和最终集成。
- 各 Agent 只能提交自己任务范围内的改动。
- 没有交接记录的改动不得进入验收。
- 未通过 DoD 的任务不得标记为 `DONE`。

## 5. 共享契约与唯一事实源

项目初始化后，以下文件是 Agent 之间的接口边界：

| 文件 | 作用 | 所有者 |
|---|---|---|
| `api/openapi.yaml` | 前端与 C++ 的 REST 契约 | COORD |
| `proto/workspace.proto` | C++ 与 Go Worker 的 gRPC 契约 | COORD |
| `docs/contracts/sse-events.md` | SSE 事件名、字段、游标和示例 | COORD |
| `docs/contracts/error-codes.md` | 错误码、用户提示和重试语义 | COORD |
| `docs/schema/erd.md` | 核心对象与关系 | CORE |
| `db/migrations/*.sql` | 数据库迁移 | CORE |
| `docs/adr/ADR-*.md` | 架构决策记录 | COORD |
| `docs/handoffs/*.md` | Agent 交接记录 | 任务主责 Agent |
| `README.md` | 构建、启动和验证入口 | QA，COORD 审核 |

契约必须带版本号。任何破坏兼容性的字段删除、语义变化、端口变化或事件格式变化，都必须创建 ADR 并通知所有依赖方。

## 6. 初始任务板

状态取值：`TODO`、`READY`、`IN_PROGRESS`、`BLOCKED`、`REVIEW`、`DONE`。

| 任务 | 主责 | 依赖 | 写入范围 | 交付结果 | 初始状态 |
|---|---|---|---|---|---|
| G00 建立独立代码仓与目录骨架 | COORD | 无 | 项目根目录 | 可构建的目录骨架 | READY |
| G01 冻结 REST、Proto、SSE、错误码契约 | COORD | G00 | `api/`、`proto/`、`docs/contracts/` | 契约 V1.0 + 示例 | READY |
| G02 数据库 ER 图与迁移 | CORE | G01 | `docs/schema/`、`db/migrations/` | 空库可初始化 | BLOCKED |
| G03 测试夹具与 Docker Compose 骨架 | QA | G00 | `tests/`、`deploy/` | Postgres + Mock 模型可启动 | BLOCKED |
| G10 C++ 登录、Agent、会话和消息 REST | CORE | G01、G02 | `server/` | 可调用的基础业务 API | BLOCKED |
| G11 C++ Run/Job 编排与 SSE | CORE | G10 | `server/` | 创建任务、领取、事件推送 | BLOCKED |
| G12 C++ gRPC 服务 | CORE | G01、G11 | `server/` | 6 个最小 RPC 可调用 | BLOCKED |
| G13 Go Worker 主循环与租约 | WORKER | G01、G12 | `worker/` | 可领取、心跳、完成、失败 | BLOCKED |
| G14 模型适配器与 Mock 模型 | WORKER | G13 | `worker/` | 流式 Mock 完整回复 | BLOCKED |
| G15 前端登录、Agent 和对话界面 | WEB | G01、G10 | `frontend/` | 可发消息并展示历史 | BLOCKED |
| G16 前端 SSE 客户端与断线回放 | WEB | G11、G15 | `frontend/` | 流式渲染与重连 | BLOCKED |
| G17 基础权限、审计与敏感信息保护 | CORE | G10、G11 | `server/` | 越权拒绝、关键事件可追踪 | BLOCKED |
| G18 超时、取消、重试和幂等 | CORE、WORKER | G12、G13、G14 | 各自范围 | 重复回调不重复写入 | BLOCKED |
| G20 集成与端到端测试 | QA | G10-G18 | `tests/` | 自动 E2E 通过 | BLOCKED |
| G21 部署包与 Runbook | QA | G03、G20 | `deploy/`、`scripts/`、`docs/runbook/` | 新环境可按文档启动 | BLOCKED |
| G22 发布审查与交付归档 | COORD | G20、G21 | `docs/`、根目录 | V0.1 验收记录 | BLOCKED |

任务状态变化必须写入交接记录或任务看板，不得只停留在聊天消息中。

## 7. 并行开发波次

### Wave 0：契约与骨架

由 COORD 主责：

- G00：创建独立代码仓和目录。
- G01：冻结 OpenAPI、Proto、SSE、错误码、关键 ADR。
- QA 可同步准备 G03 的 Compose 和 Mock 模型设计，但不得提前实现依赖未冻结的接口。

Wave 0 退出条件：

- 所有契约有版本号和示例。
- 每个跨模块字段都有明确类型和来源。
- CORE、WORKER、WEB 能按契约独立生成代码或编写客户端。
- 没有未决的端口、协议和状态机问题。

### Wave 1：三条并行开发线

契约冻结后：

- CORE 执行 G02、G10、G11、G12、G17。
- WORKER 执行 G13、G14。
- WEB 执行 G15、G16。
- QA 执行 G03、测试夹具和 Mock 模型。

并行期间不得自行修改契约。发现缺口时：

1. 在交接记录中标记 `CONTRACT-CHANGE-REQUEST`。
2. 说明字段、原因、影响方和兼容策略。
3. 等待 COORD 更新契约版本。
4. 收到新版本后再继续受影响部分。

### Wave 2：集成与交付

- G18：CORE 与 WORKER 联合验证超时、取消、重试和幂等。
- G20：QA 跑完整 E2E。
- G21：QA 完成部署包和 Runbook。
- G22：COORD 完成发布审查和交付归档。

## 8. Agent 开始任务的固定流程

每个 Agent 开始任务时必须执行：

1. 读取 `AGENTS.md`。
2. 读取任务依赖的契约、ADR 和交接记录。
3. 确认任务状态为 `READY` 或已获 COORD 授权。
4. 写明本次任务 ID、目标、写入范围和验收命令。
5. 检查工作区是否已有其他人的未提交修改。
6. 只修改授权路径。
7. 运行最小必要检查。
8. 更新任务状态并提交交接记录。
9. 通知 COORD 进行审查或合并。

如果任务状态不是 `READY`，Agent 不得自行开始实现，只能做不产生代码的调研，并报告依赖仍缺失。

## 9. 交接协议

### 9.1 交接文件命名

```text
docs/handoffs/G10-core-rest.md
docs/handoffs/G13-worker-kernel.md
docs/handoffs/G15-frontend-chat.md
```

同一天多次交接可使用：

```text
docs/handoffs/G10-core-rest-20261110-01.md
```

### 9.2 交接模板

````markdown
# Handoff: <任务编号> <任务名称>

- 状态：REVIEW / BLOCKED
- 主责 Agent：<角色>
- 分支 / 提交：<branch> / <commit>
- 开始时间：
- 完成时间：

## 完成内容

- ...

## 修改文件

- ...

## 契约变更

- 无 / 有（列出 ADR、OpenAPI、Proto、迁移和兼容影响）

## 验证命令与结果

```text
<command>
<result>
```

## 验收映射

- 验收项 1：通过 / 失败 / 未覆盖
- 验收项 2：通过 / 失败 / 未覆盖

## 已知问题与风险

- ...

## 需要其他 Agent 处理的事项

- ...

## 下一任务建议

- ...
````

### 9.3 Blocked 模板

```markdown
BLOCKED
- 任务：
- 阻塞原因：
- 缺失依赖：
- 需要谁决策：
- 已尝试的方案：
- 是否影响其他 Agent：
- 建议解除方式：
```

### 9.4 交接原则

- 交接记录写在仓库中，不只写在聊天里。
- 交接必须包含可重复执行的验证命令。
- 未完成项必须明确，不得用“基本完成”描述。
- 契约变更必须单独列出。
- 涉及安全、权限、数据迁移和端口的变化必须通知 COORD。
- 下游 Agent 只依赖已合并的契约和交接，不依赖口头说明。

## 10. 阻塞与升级

出现以下情况时必须升级给 COORD：

- API、Proto、SSE 或数据库契约存在冲突。
- 需要新增框架、依赖或外部服务。
- 需要改变端口、协议或网络暴露方式。
- 需要改变权限模型或数据边界。
- 需要修改已合并的数据库迁移。
- 需要接触密钥、外部账号或生产数据。
- 一个任务需要跨两个以上 Agent 的写入范围。
- 测试无法通过且原因涉及其他模块。
- 发现敏感信息可能进入日志、前端或仓库。

升级时必须附带：

1. 事实和相关文件。
2. 影响范围。
3. 已尝试方案。
4. 推荐决策。
5. 不决策会导致的后果。

COORD 通过 ADR 或契约更新给出决定；在决定前，阻塞 Agent 不得自行采用临时协议继续开发。

## 11. Definition of Ready

任务只有同时满足以下条件才能进入 `READY`：

- 任务编号、目标和验收标准明确。
- 主责 Agent 和写入范围明确。
- 依赖任务已完成或已明确可并行。
- 所需契约已冻结。
- 测试或验证方式明确。
- 不依赖未确认的端口、框架和数据格式。
- 涉及安全的边界已说明。

## 12. Definition of Done

任务只有同时满足以下条件才能标记为 `DONE`：

- 代码、配置或文档已完成。
- 改动只在授权范围内。
- OpenAPI、Proto、迁移和事件格式保持同步。
- 关键分支、权限、状态机和幂等逻辑有可执行检查。
- 至少一条自动化或可重复的验收命令通过。
- 没有密钥、令牌或敏感原文进入仓库和日志。
- 交接记录完整。
- 主责 Agent 完成自检。
- COORD 或指定审查者确认验收项。

前端任务额外要求：

- 浏览器刷新后状态可恢复。
- SSE 断线后能重连或给出明确错误。
- 网络失败和模型失败有用户可理解的提示。
- 不信任前端缓存进行权限判断。

Worker 任务额外要求：

- 不直接连接 PostgreSQL。
- 超时、取消和租约失效可终止执行。
- 重复 Job 或重复事件不会重复写最终消息。
- 模型 Provider 错误被转换为统一错误码。

C++ 任务额外要求：

- 写业务数据前完成鉴权和权限检查。
- 消息、Run 和 Job 的创建保持事务一致性。
- SSE 事件可回放，事件序号单调递增。
- 审计日志不保存敏感消息原文。

## 13. 变更控制

### 13.1 契约变更

REST、Proto、SSE、错误码、数据库表的变更必须：

1. 创建变更说明或 ADR。
2. 标明兼容性：兼容 / 不兼容。
3. 更新契约文件和版本号。
4. 更新示例和测试。
5. 通知所有依赖方。
6. 由 COORD 审核后合并。

### 13.2 数据库迁移

- 已合并迁移不可修改。
- 所有结构变化新增迁移文件。
- 迁移必须有向前执行说明和受影响表。
- 删除列、重命名列和破坏性变更必须先创建 ADR。
- 开发环境迁移脚本应可重复执行或通过版本表保证只执行一次。

### 13.3 依赖变更

新增依赖前必须说明：

- 现有标准库或已安装依赖为什么不够。
- 依赖的维护状态、许可证和安全风险。
- 是否影响构建、部署和镜像大小。
- 是否可以删除或延后。
- 谁负责升级和修复。

禁止为了一个简单功能引入大型框架、消息队列或插件系统。

## 14. 验收门禁

### Gate 1：契约门禁

- OpenAPI、Proto、SSE 和错误码已冻结。
- C++、Worker 和前端能按契约独立开发。
- 状态机 `queued -> running -> succeeded / failed / cancelled` 已定义。
- 数据库对象和字段有明确来源。

### Gate 2：纵向链路门禁

- 登录 -> 创建 Agent -> 创建会话 -> 发送消息均可执行。
- C++ 创建 Run/Job 并返回 `run_id`。
- Worker 可领取 Job 并调用 Mock 模型。
- SSE 能收到 `run.started`、`message.delta`、`message.completed`。
- 最终消息写入 PostgreSQL，刷新后可见。

### Gate 3：安全与可靠性门禁

- 用户不能访问其他用户的 Agent、会话和 Run。
- Worker 无法连接数据库。
- 浏览器无法直接访问 Worker、PostgreSQL 或模型 API。
- 模型密钥不出现在前端、数据库明文和日志中。
- 重复事件和重复回调不会重复写最终消息。
- 超时、取消、租约失效可恢复。
- SSE 重连能补齐事件。

### Gate 4：交付门禁

- 一条自动化 E2E 通过。
- Docker Compose 或等价启动方式可用。
- 环境变量、迁移、启动、回滚和故障排查有文档。
- A02-A08、A12-A14 的对应文档已同步。
- 交接记录和验收记录完整。

## 15. 文档交付状态

| 文档 | 状态 | 说明 |
|---|---|---|
| A01 总体系统架构图 | 已完成 | 按现有交付物保留 |
| A02 模块划分文档 | 已完成 | `A02_模块划分文档_V1.0.docx` |
| A03 部署架构图 | 待交付 | 对应部署包与端口约束 |
| A04 前后端 REST API | 待交付 | 对应 `api/openapi.yaml` |
| A05 C++ / Go gRPC 接口 | 待交付 | 对应 `proto/workspace.proto` |
| A06 数据库设计 | 待交付 | 对应 ER 图与迁移 |
| A07 权限设计 | 待交付 | 本轮为最小资源权限版 |
| A08 Agent 执行流程 | 待交付 | Run/Job/事件状态机 |
| A09 插件系统设计 | 暂缓 | V2.0 插件阶段 |
| A10 多 Agent 协作设计 | 暂缓 | L2 协作阶段 |
| A11 版本管理设计 | 暂缓 | 成果物阶段 |
| A12 错误与重试机制 | 待交付 | 超时、失败、取消、幂等 |
| A13 测试文档 | 待交付 | 单元、接口、gRPC、E2E |
| A14 部署与使用说明 | 待交付 | 环境、迁移、启动、排查 |

V2-D01 至 V2-D08 不在本轮协作范围。

## 16. 当前状态与首批派发

### 16.1 当前状态

- A01 总体架构已交付。
- A02 模块划分文档已交付。
- 基础对话平台实施范围与交付清单已形成。
- 平台源代码尚未建立。
- Drogon 仍需在 G00/G01 阶段完成最小可行性确认。
- 文件存储本轮不做。
- 当前没有冻结的 OpenAPI、Proto、SSE 或数据库契约。

### 16.2 首批派发

只派发以下任务：

1. COORD：执行 G00，建立独立代码仓与目录骨架。
2. COORD：执行 G01，冻结 REST、Proto、SSE、错误码和第一批 ADR。
3. QA：在 G00 完成后执行 G03，准备 PostgreSQL 与 Mock 模型运行骨架。

其余任务保持 `BLOCKED`，直到 Gate 1 通过。

## 17. Agent 启动提示模板

向单个 Agent 派发任务时，使用以下模板：

```text
你是 <角色编号：CORE / WORKER / WEB / QA / COORD>。

先阅读：
- AGENTS.md
- 基础对话平台_实施范围与交付清单.md
- AGENTS.md 第 6 节中分配给你的任务
- 任务依赖的契约、ADR 和交接记录

当前任务：<任务编号与名称>
目标：<一段话>
允许写入：<明确路径>
禁止修改：<明确路径>
依赖：<已完成的契约或任务>
验收：<可执行命令或检查项>

要求：
1. 先确认任务为 READY，并检查依赖是否已合并。
2. 只修改允许写入的路径。
3. 如发现契约缺口，不要自行扩展，提交 CONTRACT-CHANGE-REQUEST。
4. 完成后创建 docs/handoffs/<任务编号>-<名称>.md。
5. 报告修改文件、测试命令、测试结果、风险和下一步。
```

## 18. 最终交付原则

本项目以“可运行、可追踪、可交接、可验收”为完成标准，不以页面数量、文档数量或代码行数衡量进度。

优先级顺序：

1. 架构边界正确。
2. 纵向链路可运行。
3. 权限和数据安全。
4. 历史与状态可恢复。
5. 测试和部署可重复。
6. 再扩展 Project、Task、插件和个性化能力。

任何 Agent 都不得为了提前展示功能而绕过契约、权限、数据库边界或交付门禁。

## 19. GitHub 自动同步规则

用户已对 `ai-workspace-v2` 仓库授予持续同步授权。任何任务只要修改了已跟踪的项目文件，Agent 在结束任务前必须使用项目 Skill：

```text
.agents/skills/github-auto-sync/SKILL.md
```

并执行：

```powershell
./.agents/skills/github-auto-sync/scripts/github-sync.ps1 -Message "<commit message>"
```

该授权不要求用户再次说“提交”或“推送”。若没有文件变化、测试失败、发现密钥、存在远端冲突或推送失败，则不得伪装为同步成功；需保留现场并向用户报告。