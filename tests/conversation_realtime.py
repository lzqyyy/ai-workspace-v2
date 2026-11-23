from __future__ import annotations

import json
import sys
import threading
import time
import urllib.parse
import urllib.request
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import ApiFailure, BASE, Client, login, send  # noqa: E402
from shared_conversation import expect_error  # noqa: E402


def read_conversation_events(client: Client, conversation_id: str, after: int = 0, limit: int = 1,
                             timeout: float = 10, opened: threading.Event | None = None):
    query = urllib.parse.urlencode({
        "request_id": f"conversation-sse-{uuid.uuid4()}",
        "after": after,
    })
    request = urllib.request.Request(f"{BASE}/api/v1/conversations/{conversation_id}/events?{query}")
    request.add_header("Accept", "text/event-stream")
    request.add_header("X-Request-ID", f"conversation-sse-{uuid.uuid4()}")
    events = []
    event_name = ""
    event_data = ""
    with client.opener.open(request, timeout=timeout) as response:
        if opened is not None:
            opened.set()
        while len(events) < limit:
            raw = response.readline()
            if not raw:
                break
            line = raw.decode().rstrip("\r\n")
            if line.startswith("event:"):
                event_name = line[6:].strip()
            elif line.startswith("data:"):
                event_data = line[5:].strip()
            elif line == "" and event_data:
                envelope = json.loads(event_data)
                if event_name and envelope.get("type") != event_name:
                    raise AssertionError("Conversation SSE event name does not match envelope type")
                events.append(envelope)
                event_name = ""
                event_data = ""
    return events


def main() -> int:
    owner = Client()
    editor = Client()
    owner_user = login(owner, "demo", "demo1234", origin="http://localhost:8080")
    editor_user = login(editor, "other", "demo1234", origin="http://127.0.0.1:8080")

    project = owner.request(
        "POST", "/api/v1/projects",
        {"name": f"Conversation realtime {uuid.uuid4().hex[:8]}", "description": "Conversation SSE acceptance"},
    )
    project_id = project["id"]
    owner.request("POST", f"/api/v1/projects/{project_id}/members", {"user_id": editor_user["id"], "role": "editor"})
    owner_agent = owner.request("POST", "/api/v1/agents", {"name": f"Realtime Owner Agent {uuid.uuid4().hex[:6]}"})
    editor_agent = editor.request("POST", "/api/v1/agents", {"name": f"Realtime Editor Agent {uuid.uuid4().hex[:6]}"})
    owner.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": owner_agent["id"]})
    editor.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": editor_agent["id"]})
    conversation = owner.request(
        "POST", f"/api/v1/projects/{project_id}/conversations",
        {"agent_id": owner_agent["id"], "title": "Realtime shared conversation"},
    )
    conversation_id = conversation["id"]

    opened = threading.Event()
    received = []
    errors = []

    def watch():
        try:
            received.extend(read_conversation_events(editor, conversation_id, after=0, limit=1, timeout=15, opened=opened))
        except Exception as error:  # retained for the main assertion
            errors.append(error)

    watcher = threading.Thread(target=watch, daemon=True)
    watcher.start()
    assert opened.wait(5), "Conversation stream did not open"
    request_id = f"realtime-{uuid.uuid4()}"
    started = time.monotonic()
    sent = send(owner, conversation_id, owner_agent["id"], "realtime message", request_id)
    watcher.join(10)
    assert not watcher.is_alive(), "Conversation stream did not deliver an event"
    if errors:
        raise errors[0]
    assert received, "Conversation stream delivered no run.available event"
    event = received[0]
    latency = time.monotonic() - started
    assert latency < 2.0, f"run.available latency exceeded budget: {latency:.3f}s"
    assert event["type"] == "run.available"
    assert event["conversation_id"] == conversation_id
    payload = event["payload"]
    assert payload["run_id"] == sent["run_id"]
    assert payload["target_agent_id"] == owner_agent["id"]
    assert payload["initiated_by_user_id"] == owner_user["id"]
    assert payload["status"] == "queued"

    # A duplicate send must not add another Conversation event. The next event
    # read below must therefore belong to the retry Run, not the duplicate send.
    duplicate = send(owner, conversation_id, owner_agent["id"], "realtime message", request_id)
    assert duplicate["duplicate"] is True and duplicate["run_id"] == sent["run_id"]
    cancelled = owner.request("POST", f"/api/v1/runs/{sent['run_id']}/cancel")
    assert cancelled["status"] == "cancelled", cancelled
    retried = owner.request(
        "POST", f"/api/v1/runs/{sent['run_id']}/retry",
        headers={"Idempotency-Key": f"realtime-retry-{uuid.uuid4()}"},
    )
    retry_event = read_conversation_events(editor, conversation_id, after=event["seq"], limit=1, timeout=10)[0]
    assert retry_event["seq"] == event["seq"] + 1, retry_event
    retry_payload = retry_event["payload"]
    assert retry_payload["run_id"] == retried["run_id"], retry_event
    assert retry_payload["target_agent_id"] == owner_agent["id"], retry_event
    assert retry_payload["initiated_by_user_id"] == owner_user["id"], retry_event
    assert retry_payload["status"] == "queued", retry_event
    owner.request("POST", f"/api/v1/runs/{retried['run_id']}/cancel")

    # Explicit cursor replay must return the same persisted event.
    replay = read_conversation_events(editor, conversation_id, after=0, limit=1, timeout=10)
    assert replay and replay[0]["seq"] == event["seq"] and replay[0]["payload"]["run_id"] == sent["run_id"]

    # Viewer remains read-only but may subscribe; a former member is denied.
    owner.request("PATCH", f"/api/v1/projects/{project_id}/members/{editor_user['id']}", {"role": "viewer"})
    viewer_event = read_conversation_events(editor, conversation_id, after=0, limit=1, timeout=10)
    assert viewer_event and viewer_event[0]["payload"]["run_id"] == sent["run_id"]
    owner.request("DELETE", f"/api/v1/projects/{project_id}/members/{editor_user['id']}")
    expect_error(editor, "GET", f"/api/v1/conversations/{conversation_id}/events?request_id=nonmember-{uuid.uuid4()}", 403, "FORBIDDEN")

    print(json.dumps({
        "status": "PASS",
        "project_id": project_id,
        "conversation_id": conversation_id,
        "run_id": sent["run_id"],
        "retry_run_id": retried["run_id"],
        "target_agent_id": payload["target_agent_id"],
        "latency_ms": round(latency * 1000, 1),
        "retry_event": True,
        "duplicate_no_event": True,
        "replay": True,
        "viewer_subscribe": True,
        "non_member_denied": True,
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
