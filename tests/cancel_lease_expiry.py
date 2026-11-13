from __future__ import annotations

import json
import subprocess
import time
import uuid

from e2e import Client, HELPER, login, send


def helper(mode: str) -> dict:
    result = subprocess.run([str(HELPER), "-mode", mode], capture_output=True, text=True, timeout=45, check=False)
    assert result.returncode == 0, result.stderr
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    assert lines, result.stdout
    return json.loads(lines[-1])


def main() -> int:
    client = Client()
    login(client, origin="http://localhost:8080")
    agent = client.request(
        "POST",
        "/api/v1/agents",
        {"name": f"Cancel Lease {uuid.uuid4().hex[:6]}", "system_prompt": "Cancel lease expiry test."},
    )
    conversation = client.request(
        "POST",
        "/api/v1/conversations",
        {"agent_id": agent["id"], "title": "Cancel lease expiry"},
    )
    sent = send(client, conversation["id"], "取消后租约过期必须保持 cancelled", f"e2e-cancel-lease-{uuid.uuid4()}")
    claim = helper("claim")
    assert claim["run_id"] == sent["run_id"], (claim, sent)

    cancelled = client.request("POST", f'/api/v1/runs/{sent["run_id"]}/cancel')
    assert cancelled["status"] == "running", cancelled
    assert cancelled["cancel_requested"] is True, cancelled

    wait_seconds = max(0.0, int(claim["lease_expires_at_unix_ms"]) / 1000 - time.time() + 1.0)
    time.sleep(wait_seconds)
    helper("claim-once")

    events = client.events(sent["run_id"])
    assert events[-1]["type"] == "run.cancelled", events[-1]
    assert not any(event["type"] == "run.failed" for event in events), events
    assert events[-1]["payload"].get("reason") == "user_requested", events[-1]
    messages = client.request("GET", f'/api/v1/conversations/{conversation["id"]}/messages')["items"]
    assert len(messages) == 1 and messages[0]["sender_type"] == "user", messages
    print(
        json.dumps(
            {
                "status": "PASS",
                "cancel_requested": True,
                "lease_expired": True,
                "terminal_event": "run.cancelled",
                "assistant_messages": 0,
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
