# Handoff: HOTFIX-20261111 Local Origin Allow-list

- 状态：DONE
- 主责 Agent：COORD / CORE
- 分支 / 提交：main / 待本次修复提交
- 完成时间：2026-11-11

## 完成内容

- `APP_ORIGIN` 支持逗号分隔的精确允许列表。
- 本地默认同时允许 `http://localhost:8080` 和 `http://127.0.0.1:8080`。
- 未列出的第三方 Origin 仍返回 403，不降低 CSRF 防护。
- E2E 覆盖两个本地允许源和恶意 Origin 拒绝。

## 修改文件

- `server/src/common.hpp`、`server/src/app.cpp`
- `scripts/dev.ps1`、`.env.example`、`deploy/docker-compose.yml`
- `tests/e2e.py`、`docs/runbook/A14-operations.md`

## 验证命令与结果

```text
http://localhost:8080   -> 200
http://127.0.0.1:8080   -> 200
http://evil.example     -> 403
./scripts/test.ps1      -> PASS
```