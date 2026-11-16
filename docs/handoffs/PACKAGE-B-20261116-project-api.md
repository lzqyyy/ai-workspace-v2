# Handoff: PACKAGE-B-20261116 Project API and permissions

- 状态：REVIEW
- 主责 Agent：CORE
- 分支：`agent/core/package-b-project-api`
- 基线：`1f0ead3fbe1a9c7f5d818372667f73000f9aba18`
- 提交：单个本地提交（hash 见最终报告）
- 开始时间：2026-11-16
- 完成时间：2026-11-16

## 目标

实现 Project、ProjectMember、ProjectAgent 的 C++ REST API、角色授权、最后 owner 保护、active Run 移除保护和脱敏审计；不实现共享 Conversation、`target_agent_id` 发送切换或 Run/SSE Project 权限。

## 完成内容

- 实现 `POST/GET /api/v1/projects` 与 `GET /api/v1/projects/{project_id}`。
- 创建 Project 时在同一事务内把调用者写入 `owner` 成员。
- 实现 Project member 的添加、列表、角色修改和移除路由。
- 实现 Project Agent 的添加、列表和移除路由。
- 角色严格限制为 `owner/editor/viewer`；owner 管理成员与角色，owner/editor 只能添加自己的 active Agent，viewer 禁止写操作。
- 非 Project 成员访问 Project 资源统一返回 `403 FORBIDDEN`，并在返回前不查询目标资源细节。
- 成员移除/降级使用 Project 行锁串行化 owner 管理；禁止移除或降级最后一个 owner。
- 移除成员或 Agent 前检查 Project 内 queued/running Run；存在 active Run 时返回 `409 CONFLICT`。
- Project/成员/Agent 关键成功操作写 `audit_events`；权限拒绝写 `permission.denied`。审计 metadata 只含资源 ID、角色、动作等脱敏字段。
- 保持 C++ Drogon + PostgreSQL、REST/SSE 8080、gRPC 50051、PostgreSQL 5432，不修改 Go Worker、Proto、前端或端口。
- 未实现 Package C 范围：共享 Conversation、target Agent 发送切换、Run/SSE Project 权限、发送单 active Run 锁。

## 修改文件

- `server/src/app.cpp`
- `server/src/core.cpp`
- `server/src/core.hpp`
- `tests/project_api.py`
- `scripts/e2e.ps1`
- `docs/schema/erd.md`
- `docs/schema/data-dictionary.md`
- `docs/runbook/A07-permissions.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/PACKAGE-B-20261116-project-api.md`

## 契约变更

无 OpenAPI、Proto、SSE envelope 或数据表结构变更。实现遵循 Package A 已冻结契约。
`conversations.project_id NOT NULL`、旧 Conversation 写入切换及发送锁仍留给 Package C。

## 验证命令与结果

- C++ Docker 镜像编译：PASS。
- `git diff --check`：PASS。
- Docker-first `./scripts/test.ps1 -Docker`：PASS。
- OpenAPI contract validation：PASS；25 operations、6 RPC、5 SSE events。
- Go Worker unit tests：PASS。
- Frontend syntax/state tests：PASS。
- 现有单用户 E2E：PASS；消息、SSE、并发幂等、取消/重试、跨用户拒绝均未回归。
- 新增 `tests/project_api.py`：PASS；Project CRUD、成员角色、viewer 写拒绝、Agent 自服务、最后 owner 保护。
- 隔离 PostgreSQL fixture：PASS；有 active Run 时移除成员/Agent 均返回 409，取消后可移除。
- 审计 SQL 断言：PASS；存在 `project.create`、`project.member.role_change`、`project.agent.add`、`permission.denied` 脱敏审计。

## 验收映射

- Project 创建并自动 owner：PASS。
- Project 列表/详情仅成员可见：PASS。
- 成员添加/列表/角色修改/移除：PASS。
- 最后 owner 保护：PASS。
- owner/editor 添加自己的 Agent：PASS。
- 非本人 Agent / viewer 添加 Agent：PASS（403）。
- active Run 成员/Agent 移除保护：PASS（409，取消后 204）。
- 非成员统一 403：PASS。
- 关键操作脱敏审计：PASS。
- 单用户链路与 Package A 迁移回归：PASS。

## 已知问题与风险

- Project 成员/Agent 管理已可用，但共享 Conversation 与多人发送链路尚未切换，不能对外宣称完整 2H+2A 已完成。
- 成员/Agent 移除保护在检查与删除之间没有与 Package C 发送事务共享锁顺序；C 必须按 Project/Conversation 锁顺序复核并在同一事务内校验 active Run。
- 当前成员移除不会自动清理该成员仍映射的 Agent；Package C 的 Agent 使用校验必须依赖当前 Project membership 与 active mapping，而不是历史映射存在性。
- 本包未新增数据库 `NOT NULL` 或唯一约束；Package C 完成写入切换后再做收紧迁移。

## 需要其他 Agent 处理的事项

- Package C 复用本包的 Project membership/role 查询和审计语义，实现共享 Conversation、`target_agent_id` 发送、单 active Run 锁以及 Conversation/Message/Run/SSE 的 Project 授权。
- Package C 需要为发送/重试事务定义与成员/Agent 移除一致的锁顺序，避免移除检查与新 Run 创建竞态。
- WEB 仍需等待 Package C 完成后接入 Project-first 多人对话，不得仅凭本包宣称多人 UI 可用。

## 下一任务建议

进入 Package C：共享 Conversation + `target_agent_id` 发送切换 + Run/SSE Project 权限与单 active Run 事务锁。
