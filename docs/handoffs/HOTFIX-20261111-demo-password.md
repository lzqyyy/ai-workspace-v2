# Handoff: HOTFIX-20261111 Demo 密码策略对齐

- 状态：DONE
- 主责 Agent：COORD / CORE / WEB
- 分支 / 提交：main / 11a6ece（密码修复提交）
- 完成时间：2026-11-11

## 完成内容

- 将演示密码从 7 位 `demo123` 调整为 8 位 `demo1234`。
- 前端预填值、提示文本、OpenAPI 示例、E2E、README/Runbook 同步更新。
- 服务端登录增加最少 8 位密码校验，避免绕过浏览器校验。
- 新增幂等迁移 `004_demo_password.sql` 更新已有演示用户，不修改历史迁移。

## 修改文件

- `frontend/index.html`
- `server/src/core.cpp`
- `db/migrations/004_demo_password.sql`
- `tests/e2e.py`
- `api/openapi.yaml`
- 相关 Runbook、验收与交接文档

## 契约变更

- 无破坏性变更；登录请求的既有 `minLength: 8` 现在由服务端同步强制执行。

## 验证命令与结果

```text
./scripts/build.ps1                         PASS
./scripts/dev.ps1 -NoBrowser                PASS
./scripts/test.ps1                          PASS
7-character password rejected               PASS
8-character demo password accepted          PASS
schema_migrations includes 004_demo_password PASS
```

## 验收映射

- 登录输入校验：通过。
- 已有数据库升级：通过。
- E2E 与权限回归：通过。

## 已知问题与风险

- 用户需使用新演示密码 `demo1234`；旧密码不再接受。