# ADR-0001：C++ HTTP/SSE 框架选择

- 状态：Accepted
- 日期：2026-11-10
- 决策者：COORD

## 背景

契约需要 C++ 核心同时提供 REST/JSON、SSE 长连接和 gRPC 服务。PRD 中 Drogon 为候选默认方案，但尚未验证。

## 决策

采用 Drogon 1.9.x 提供 HTTP/REST/SSE；gRPC C++ 与 Drogon 在同一进程内独立监听内部端口。PostgreSQL 通过 libpq 参数化查询访问。

## 依据

- Drogon 提供异步 HTTP、静态文件、SSE chunked stream，不引入第二种前端框架或额外业务端口。
- MSYS2 UCRT64 的 Drogon、gRPC、Protobuf、PostgreSQL 预编译包可用于本机完整构建验证。
- 不采用 Drogon ORM，避免代码生成对象成为跨层隐式契约；SQL 只存在于 C++ 仓储层。

## 影响

- C++ 代码必须显式检查数据库错误、事务和权限。
- SSE 连接在应用事件循环中轮询持久化事件；MVP 可接受，后续可改为 PostgreSQL LISTEN/NOTIFY，但不改变事件契约。
- 依赖由 Docker 镜像锁定并在 Runbook 说明。