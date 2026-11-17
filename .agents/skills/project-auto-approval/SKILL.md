---
name: project-auto-approval
description: Use in ai-workspace-v2 whenever a routine project operation could trigger a per-command approval. Apply the repository's standing user authorization for project reads, writes, package/tool installation, local processes, networking, tests, builds, and sanctioned Git operations without asking again; keep explicit safety boundaries for unrelated or irreversible actions.
---

# Project Auto Approval

The user has granted standing, repository-scoped authorization to reduce repeated permission prompts. In this repository, do not ask a separate approval question for each routine command.

## Scope

- Repository: `D:\lzqy_\codex\Workspace\ai-workspace-v2`
- Applies to reads and writes inside this repository, dependency/tool installation needed to build or test it, local service startup/stopping, outbound network access needed by the requested task, test/build commands, and Git operations on sanctioned branches.
- The project-level `.codex/config.toml` sets `approval_policy = "never"` and `sandbox_mode = "danger-full-access"` so project sessions do not stop for routine approval requests.
- This authorization is repository-scoped. It does not authorize unrelated repositories, global Codex configuration, production systems, credential disclosure, or destructive history/file operations that are not required by the current task.

## Operating Rules

1. Start with the task and safety rules in `AGENTS.md`; proceed with necessary project commands without asking for repetitive permission confirmation.
2. Prefer reversible, narrowly scoped commands. Do not turn broad authorization into unrelated scanning, deletion, force-pushing, or secret access.
3. Stop and ask only when an action is unrelated to the requested task, irreversible in a way the request did not authorize, touches external production data, or would disclose secrets.
4. After tracked project files change, follow `.agents/skills/github-auto-sync/SKILL.md` to validate, commit, and push the sanctioned branch.
5. In the final report, state that the project approval policy was changed and that an application/thread restart may be required for it to take effect.

## Verification

- Confirm `.codex/config.toml` contains the project approval and sandbox settings.
- Run the smallest relevant test suite and `git diff --check`.
- Never claim permissions are active in the current already-running session when the client has not reloaded project configuration.
