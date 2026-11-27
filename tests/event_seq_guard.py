from __future__ import annotations

import json
import subprocess
import uuid

from e2e import Client, HELPER, login, send


def run_helper(mode: str) -> list[dict]:
    result = subprocess.run(
        [str(HELPER), "-mode", mode],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    assert lines, result.stdout
    return [json.loads(line) for line in lines]


def main() -> int:
    client = Client()
    login(client, origin="http://localhost:8080")
    agent = client.request(
        "POST",
        "/api/v1/agents",
        {"name": f"Event Seq {uuid.uuid4().hex[:6]}", "system_prompt": "Event sequence guard."},
    )
    conversation = client.request(
        "POST",
        "/api/v1/conversations",
        {"agent_id": agent["id"], "title": "Event sequence guard"},
    )
    sent = send(
        client,
        conversation["id"],
        agent["id"],
        "event sequence guard",
        f"e2e-event-seq-{uuid.uuid4()}",
    )
    assert sent["status"] == "queued", sent

    result = run_helper("event-seq")
    assert len(result) == 1 and result[0]["status"] == "PASS", result
    events = client.events(sent["run_id"])
    assert events[-1]["type"] == "run.cancelled", events[-1]
    print(
        json.dumps(
            {
                "status": "PASS",
                "gap_rejected": True,
                "duplicate_preserved": True,
                "out_of_order_rejected": True,
                "terminal_event": "run.cancelled",
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
