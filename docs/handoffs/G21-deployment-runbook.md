# Handoff: G21 部署包与 Runbook

- 状态：DONE
- 主责 Agent：QA
- 分支 / 提交：main / 88ae550（代码提交）
完成时间：2026-11-10

## 完成内容
- Compose Postgres + Core + Worker；仅 8080 对外。
- C++ 启动自动迁移，非 root 容器运行。
- A03 部署图、A14 环境/启动/迁移/故障排查。

## 修改文件
- `deploy/docker-compose.yml`、`server/Dockerfile`、`worker/Dockerfile`、`.env.example`、`docs/runbook/A03-deployment.md`、`docs/runbook/A14-operations.md`。

## 契约变更
- 无。

## 验证命令与结果
```text
本地进程等价部署：PASS
空库自动迁移：PASS
```

## 验收映射
- G21、D08、D10：通过。

## 已知问题与风险
- 需在 Docker 环境执行首次真实镜像构建。