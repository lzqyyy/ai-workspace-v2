from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
OPENAPI = ROOT / "api" / "openapi.yaml"
PROTO = ROOT / "proto" / "workspace.proto"
SSE = ROOT / "docs" / "contracts" / "sse-events.md"
ERRORS = ROOT / "docs" / "contracts" / "error-codes.md"

EXPECTED_PATHS = {
    ("/api/v1/auth/login", "post"),
    ("/api/v1/me", "get"),
    ("/api/v1/agents", "get"),
    ("/api/v1/agents", "post"),
    ("/api/v1/conversations", "get"),
    ("/api/v1/conversations", "post"),
    ("/api/v1/conversations/{conversation_id}/messages", "get"),
    ("/api/v1/conversations/{conversation_id}/messages", "post"),
    ("/api/v1/runs/{run_id}/events", "get"),
    ("/api/v1/runs/{run_id}/cancel", "post"),
    ("/api/v1/health", "get"),
}
EXPECTED_RPCS = {
    "ClaimNextJob",
    "Heartbeat",
    "UpdateJobStatus",
    "GetExecutionContext",
    "AppendEvent",
    "CheckPermission",
}
EXPECTED_EVENTS = {
    "run.started",
    "message.delta",
    "message.completed",
    "run.failed",
    "run.cancelled",
}


def main() -> int:
    api = yaml.safe_load(OPENAPI.read_text(encoding="utf-8"))
    assert api["openapi"] == "3.1.0"
    assert api["info"]["version"] == "1.0.0"

    operations = {
        (path, method)
        for path, item in api["paths"].items()
        for method in item
        if method.lower() in {"get", "post", "put", "patch", "delete"}
    }
    missing = EXPECTED_PATHS - operations
    assert not missing, f"missing OpenAPI operations: {sorted(missing)}"

    schemas = api["components"]["schemas"]
    for name in ("User", "Agent", "Conversation", "Message", "Run", "Error"):
        assert name in schemas, f"missing schema: {name}"

    api_error_codes = set(schemas["Error"]["properties"]["code"]["enum"])
    documented_errors = set(re.findall(r"\| `([A-Z][A-Z0-9_]+)` \|", ERRORS.read_text(encoding="utf-8")))
    assert api_error_codes == documented_errors, (
        f"error-code mismatch: api={sorted(api_error_codes)} docs={sorted(documented_errors)}"
    )

    proto = PROTO.read_text(encoding="utf-8")
    assert 'package aiworkspace.v1;' in proto
    rpcs = set(re.findall(r"\brpc\s+(\w+)\s*\(", proto))
    assert rpcs == EXPECTED_RPCS, f"RPC mismatch: {sorted(rpcs)}"
    for field in ("request_id", "run_id", "job_id", "lease_token", "event_seq", "status", "error_code"):
        assert re.search(rf"\b{field}\b", proto), f"missing proto field: {field}"

    sse = SSE.read_text(encoding="utf-8")
    for event in EXPECTED_EVENTS:
        assert f"`{event}`" in sse, f"missing SSE event: {event}"
    assert "queued -> running -> succeeded" in sse.replace("\\", "")
    assert "Last-Event-ID" in sse

    compose = yaml.safe_load((ROOT / "deploy" / "docker-compose.yml").read_text(encoding="utf-8"))
    services = compose["services"]
    assert {"postgres", "core", "worker"} <= set(services)
    assert not services["postgres"].get("ports"), "PostgreSQL must not publish a host port"
    assert not services["worker"].get("ports"), "Worker must not publish a host port"
    assert services["core"].get("ports") == ["8080:8080"], "Only Core 8080 may be published"
    migrations = sorted((ROOT / "db" / "migrations").glob("*.sql"))
    assert migrations, "database migrations are missing"
    text_corpus = "\n".join(p.read_text(encoding="utf-8") for p in [OPENAPI, PROTO, SSE, ERRORS])
    assert "sk-" not in text_corpus, "possible API key entered the contract tree"
    print("contract validation: PASS")
    print(f"OpenAPI operations: {len(operations)}; RPCs: {len(rpcs)}; SSE events: {len(EXPECTED_EVENTS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())