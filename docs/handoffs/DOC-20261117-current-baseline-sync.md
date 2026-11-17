# Handoff: DOC-20261117 current baseline sync

- 状态：REVIEW
- 主责 Agent：COORD
- 分支 / 提交：`agent/core/package-def-mvp-closeout` / 本次校准提交
- 开始时间：2026-11-17
- 完成时间：2026-11-17

## 目标

按当前已交付代码、冻结契约、验收记录和任务看板校准文档事实，只修正有直接证据的矛盾；对没有冻结依据的范围不擅自扩展。

## 完成内容

- 校准产品版本与发布状态：`v0.1.0` 保持既有 tag；`V0.2.0` 统一标记为 `REVIEW`，分支已推送 `origin`，未合并 `main`，未创建 tag。
- 校准 `AGENTS.md` 迁移范围：由 `001`-`004` 改为实际的 `001`-`007`。
- 校准 OpenAPI 描述：Project-first 路由已由 C++ 实现，原生前端已接入。
- 校准 ERD/README/验收记录/任务看板/Package F 后续状态，移除“前端未接入”“未 push”等过期事实。
- 将 V0.1 实施清单标记为历史计划，避免“L2 下一阶段”“Drogon 待确认”等历史表述继续冒充当前状态。
- 明确 Project 归档边界：PRD 有语义，但冻结 OpenAPI 无 operation，运行时无入口。
- 修正幂等说明：发送使用 sender-scoped `client_request_id`；retry 使用用户级 `Idempotency-Key`。
- 修正 C++ 字符串长度校验为 UTF-8 code point 计数，与 OpenAPI `minLength/maxLength` 语义一致。
- 修复 retry、重复发送、取消检查相关的查询索引/关联一致性问题，并补充回归断言。
- 修复 Windows/MSYS2 下 Protobuf 查找模式：`find_package(Protobuf REQUIRED CONFIG)`。
- 修复共享会话 E2E 的取消竞态：取消后等待 SSE 终态再发起下一 Run。

## 修改文件

- `AGENTS.md`
- `CMakeLists.txt`
- `README.md`
- `api/openapi.yaml`
- `docs/TASK_BOARD.md`
- `docs/acceptance/V0.2-multiplayer-mvp.md`
- `docs/handoffs/PACKAGE-F-20261116-delivery-closeout.md`
- `docs/implementation-checklist.md`
- `docs/runbook/A07-permissions.md`
- `docs/runbook/A12-reliability.md`
- `docs/runbook/A14-operations.md`
- `docs/schema/erd.md`
- `server/src/common.hpp`
- `server/src/core.cpp`
- `tests/e2e.py`
- `tests/shared_conversation.py`
- `docs/evidence/ui-project-first.png`
- `docs/evidence/ui-2h2a.png`
- `.dockerignore`

## 契约变更

- 无新增或删除 REST operation、Proto RPC、迁移、SSE event envelope、错误码或端口。
- OpenAPI 只修正实现状态描述，版本仍为 `1.0.0`。
- Project 归档仍不是可调用能力。
- 待确认语义：用户消息在多次 retry 后，`messages.run_id` 应表示首次 Run 还是最新 retry Run。当前实现与 retry 响应保持一致，指向最新 retry Run；冻结文档未显式定义该历史关联语义。

## 验证命令与结果

```powershell
./scripts/test-contracts.ps1
python -m py_compile tests/e2e.py tests/shared_conversation.py tests/project_api.py tests/validate_contracts.py
node --check frontend/app.js
node tests/frontend_state_test.js
go test -buildvcs=false ./worker/...
go vet -buildvcs=false ./worker/...
./scripts/build.ps1
./scripts/dev.ps1 -NoBrowser
./scripts/test.ps1
./scripts/e2e.ps1
./scripts/frontend-e2e.ps1
./scripts/frontend-2h2a-e2e.ps1
```

- 契约：PASS；OpenAPI 25 operations、6 RPC、5 SSE events。
- Python 语法：PASS。
- 前端语法与状态测试：PASS。
- Go Worker test/vet：PASS。
- C++/Worker/helper 构建：PASS。
- 单用户 E2E：PASS；包含中文 code point 长度边界、retry 幂等、SSE 回放、取消与重复回调。
- 共享 Conversation E2E：PASS；2H+2A、共享历史/SSE、viewer 只读、非成员拒绝、单 active Run。
- Project API E2E：PASS；成员/角色/最后 owner/Agent 自服务与权限矩阵。
- 单浏览器 E2E：PASS。
- 2H+2A 双浏览器 E2E：PASS；首次重跑无结果，清理残留 headless 进程后复跑 PASS。
- 本地数据库检查：迁移 `7`、`conversations.project_id` 为 `NO`、NULL 行 `0`、`007_project_id_required` 记录 `1` 条。

## 验收映射

- 版本/迁移事实：通过。
- OpenAPI/前端实现状态：通过。
- push/merge/tag 状态：通过。
- Project 归档边界：通过（明确未实现，不扩范围）。
- 幂等与长度语义：通过。
- 单用户与 2H+2A 回归：通过。
- 构建可重复性：通过（本机无 Docker 路径）。

## 已知问题与风险

- 当前宿主未提供 Docker CLI，未重复执行 `./scripts/test.ps1 -Docker`；已执行本机契约、构建、API E2E 和浏览器 E2E 等价链路。
- Project 归档仍无冻结 operation 和运行时入口。
- Conversation 级实时发现仍依赖短轮询；Run SSE 契约未改变。
- `messages.run_id` 在多次 retry 后的历史语义需 COORD 明确。
- 本分支仍为 `REVIEW`；V0.2.0 未合并 main、未打 tag。

## 需要其他 Agent 处理的事项

- COORD 审查并决定是否将本分支合并 `main`、是否创建 `V0.2.0` tag。
- 如需要用户消息保留“首次 Run”历史关联，应另行明确契约并调整实现/测试。
- 如需 Project 归档，应另开版本化契约变更，新增 method/path、权限、active Run 409、审计与回归测试。

## 下一任务建议

1. 完成 COORD 对 V0.2.0 候选的合并审查。
2. 明确 retry 后用户消息与 Run 的历史关联语义。
3. 单独评估 Conversation 级实时通知，消除短轮询发现延迟。
