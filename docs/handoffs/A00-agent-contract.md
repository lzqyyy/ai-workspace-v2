# Handoff: A00 AI 多 Agent 维护契约

- 状态：DONE
- 主责 Agent：COORD
- 分支 / 提交：main / 本次同步提交
- 完成时间：2026-11-11

## 目标

提供一份 Agent 可自动读取、跨工具可执行的项目维护契约，降低多 Agent 覆盖、越权和契约漂移风险。

## 完成内容

- 重写根目录 `AGENTS.md` 为 V1.1。
- 增加机器可读元数据、仓库地图、事实源、角色边界和并行协议。
- 明确单 Agent `main` 与多 Agent `agent/<role>/<task-id>` 分支策略。
- 补齐契约变更、测试、数据库、安全、DoR/DoD、交接模板和当前已知陷阱。
- 自动同步 Skill 支持受控的 main/agent 分支推送。
- 更新任务看板与 L2 后续任务。

## 修改文件

- `AGENTS.md`
- `docs/TASK_BOARD.md`
- `.agents/skills/github-auto-sync/SKILL.md`
- `.agents/skills/github-auto-sync/scripts/github-sync.ps1`
- `.agents/skills/github-auto-sync/agents/openai.yaml`
- `docs/handoffs/A00-agent-contract.md`

## 契约变更

无 REST/gRPC/SSE/数据库契约变更；仅维护协作契约和自动化流程。

## 验证命令与结果

```text
Skill quick_validate                    PASS
PowerShell syntax parse                 PASS
AGENTS required sections check          PASS
./scripts/test-contracts.ps1            PASS
automatic GitHub sync                   PASS
```

## 验收映射

- Agent 可独立读取项目事实和边界。
- 并行 Agent 有明确分支和写锁规则。
- 每次变更可按项目 Skill 自动 commit/push。
- 未降低架构、安全和验收门禁。

## 已知问题与风险

- 真正并行修改共享文件仍需要独立 worktree/clone 与 COORD 合并。
- 远端分支保护尚未在 GitHub 设置中强制，后续建议启用 main 分支保护。