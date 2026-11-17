# Handoff: A02 项目级免重复审批

- 状态：DONE
- 主责 Agent：COORD
- 分支 / 提交：main / 自动同步提交
- 开始时间：2026-11-17
- 完成时间：2026-11-17

## 目标

按用户当前明确指令，为 `ai-workspace-v2` 添加项目级 skill，并配置项目内不再逐次询问常规权限。

## 完成内容

- 新增 `.agents/skills/project-auto-approval/`。
- 新增 `.codex/config.toml`，项目作用域设置 `approval_policy = "never"` 与 `sandbox_mode = "danger-full-access"`。
- 更新 `AGENTS.md` 和任务看板中的项目权限事实。

## 修改文件

- `.agents/skills/project-auto-approval/SKILL.md`
- `.agents/skills/project-auto-approval/agents/openai.yaml`
- `.codex/config.toml`
- `AGENTS.md`
- `docs/TASK_BOARD.md`
- `docs/handoffs/A02-project-auto-approval.md`

## 契约变更

无产品 API、Proto、SSE、错误码或数据库契约变更。新增项目级 Codex 执行策略。

## 验证命令与结果

```text
python ...\quick_validate.py .agents\skills\project-auto-approval
Skill is valid!

.\scripts\test-contracts.ps1
contract validation: PASS
OpenAPI operations: 13; RPCs: 6; SSE events: 5
Contract checks passed.

codex doctor --summary --no-color --ascii
config loaded; 0 fail

git diff --check
PASS
```

## 验收映射

- 项目级 skill 可被发现：通过（quick_validate）
- 项目级配置存在且键值正确：通过（config loaded）
- 文档与任务看板同步：通过
- 自动同步：DONE（由 github-auto-sync 执行）

## 已知问题与风险

- `danger-full-access` 明显降低项目会话的执行隔离；仅适用于用户明确信任的仓库。
- 项目配置通常需要新建或重启会话后生效；当前会话是否已重载配置未覆盖。
- 授权范围仅限本仓库，不能外推到其他项目或全局设置。

## 需要其他 Agent 处理的事项

无。

## 下一任务建议

新建项目会话后确认不再出现逐次权限申请，并继续遵守 `AGENTS.md` 的安全红线。
