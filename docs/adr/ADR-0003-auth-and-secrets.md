# ADR-0003：会话认证、权限与密钥边界

- 状态：Accepted
- 日期：2026-11-10
- 决策者：COORD

## 决策

- 浏览器认证使用同源 HttpOnly、SameSite=Lax 的随机会话 Cookie。
- 仅保存会话令牌的 SHA-256 摘要；密码使用 PBKDF2-HMAC-SHA256、独立盐和 120000 次迭代。
- 所有资源查询以服务端当前用户 ID 作为查询条件；未命中与无权限对外使用最小信息。
- Worker 不带用户会话，只能凭短期 Job 租约访问 C++ gRPC。
- 模型密钥只从 Worker 环境变量读取；数据库只保存 `secret_ref` 名称，不保存密钥值。
- 普通日志和审计仅记录资源 ID、request_id、错误码和结果，不记录消息正文、Authorization 或密钥。

## 影响

- 50051 与 5432 不发布到宿主机公网。
- 生产必须由 HTTPS 反向代理到 C++ 8080，并将 Cookie 标记 Secure。