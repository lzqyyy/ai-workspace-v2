# Handoff: G17-G18 权限与可靠性

- 状态：DONE
- 主责 Agent：CORE / WORKER
- 分支 / 提交：main / 88ae550（代码提交）
完成时间：2026-11-10

## 完成内容
- 服务端所有者检查覆盖 Agent、会话、消息、Run。
- 审计不含正文/密钥；密码与会话令牌只存摘要。
- 租约过期回收、取消、失败重试、消息和回调幂等。
- SSE 数值序号回放，避免 1,10,2 的文本排序问题。

## 修改文件
- `server/src/core.cpp`、`server/src/app.cpp`、`worker/cmd/worker/*`、`docs/runbook/A12-reliability.md`。

## 契约变更
- 无。

## 验证命令与结果
```text
python tests/e2e.py
cross_user_denied: PASS
duplicate_callback: PASS
sse_replay: PASS
cancel_and_retry: PASS
```

## 验收映射
- G17、G18、Gate 3：通过。

## 已知问题与风险
- 自动重试上限由 `jobs.max_attempts=3` 控制。