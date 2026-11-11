---
name: github-auto-sync
description: Use whenever a completed task changes tracked files in the ai-workspace-v2 repository. Validate the change, commit it, and push the current sanctioned branch automatically without requiring a separate user sync instruction. Do not use for read-only work or repositories other than ai-workspace-v2.
---

# GitHub Auto Sync

The user has granted standing authorization to synchronize this repository after every completed mutation. Do not ask for a separate "push to GitHub" instruction.

## Scope

- Repository: `D:\lzqy_\codex\Workspace\ai-workspace-v2`
- Remote: `origin`
- Sanctioned branches: `main` and `agent/<role>/<task-id>`
- Apply only after tracked project files, configuration, contracts, migrations, tests, or documentation change.
- Do not create a commit when the task is read-only or when there are no local changes to push.

Branch policy:

- A single writer may use `main`.
- Concurrent agents must use `agent/<role>/<task-id>` and complete through review.
- Never push the current branch to a different branch.

## Required workflow

1. Finish the requested change and inspect `git diff` plus `git status --short`.
2. Run the smallest relevant verification. Use `./scripts/test.ps1` for cross-cutting, backend, schema, security, or deployment changes. For documentation-only changes, at minimum run `./scripts/test-contracts.ps1`.
3. Do not sync when tests fail, secrets are present, changes are outside task scope, or the worktree contains unrelated edits that cannot be separated safely.
4. From the repository root, execute:

```powershell
./.agents/skills/github-auto-sync/scripts/github-sync.ps1 -Message "<conventional commit message>"
```

Use Conventional Commit style such as `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, or `chore:`.

5. Report the resulting commit hash, branch, and whether the remote branch was updated.

## Safety invariants

- Never force push, rewrite published history, use `git reset --hard`, or discard user changes.
- Never commit `.env`, `.tools/`, `build/`, database data, private keys, tokens, or credentials.
- Never place a GitHub token in the repository or command line. Use the configured SSH remote.
- Fetch before rebasing. Stop on conflicts and preserve both sides for review.
- If push fails, keep the local commit and report the failure; do not hide it.
- Automatically pushing is an external mutation explicitly authorized by the user for this repository. It does not authorize pushes to any other repository or branch.