from __future__ import annotations

import json
import sys
import uuid
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import ApiFailure, Client, assert_event_contract, login, send  # noqa: E402


def expect_error(client: Client, method: str, path: str, status: int, code: str, body=None):
    try:
        client.request(method, path, body)
        raise AssertionError(f"expected HTTP {status} for {method} {path}")
    except ApiFailure as error:
        assert error.status == status and error.code == code, (error.status, error.code, error)


def message_for_run(messages, run_id):
    items = [item for item in messages if item.get("run_id") == run_id]
    assert len(items) == 2, (run_id, items)
    assert items[0]["sender_type"] == "user" and items[1]["sender_type"] == "agent", items
    return items


def main() -> int:
    owner = Client()
    editor = Client()
    owner_user = login(owner, "demo", "demo1234", origin="http://localhost:8080")
    editor_user = login(editor, "other", "demo1234", origin="http://127.0.0.1:8080")

    project = owner.request(
        "POST",
        "/api/v1/projects",
        {"name": f"2H2A Project {uuid.uuid4().hex[:8]}", "description": "Shared conversation acceptance"},
    )
    project_id = project["id"]
    owner.request(
        "POST",
        f"/api/v1/projects/{project_id}/members",
        {"user_id": editor_user["id"], "role": "editor"},
    )

    owner_agent = owner.request(
        "POST",
        "/api/v1/agents",
        {"name": f"Owner Agent {uuid.uuid4().hex[:6]}", "system_prompt": "Reply as owner agent."},
    )
    editor_agent = editor.request(
        "POST",
        "/api/v1/agents",
        {"name": f"Editor Agent {uuid.uuid4().hex[:6]}", "system_prompt": "Reply as editor agent."},
    )
    owner.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": owner_agent["id"]})
    editor.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": editor_agent["id"]})

    conversation = owner.request(
        "POST",
        f"/api/v1/projects/{project_id}/conversations",
        {"agent_id": owner_agent["id"], "title": "Shared 2H2A conversation"},
    )
    conversation_id = conversation["id"]
    assert conversation["project_id"] == project_id
    assert conversation["owner_id"] == owner_user["id"]
    assert any(item["id"] == conversation_id for item in editor.request(
        "GET", f"/api/v1/projects/{project_id}/conversations"
    )["items"])

    expect_error(
        owner,
        "POST",
        f"/api/v1/conversations/{conversation_id}/messages",
        400,
        "VALIDATION_FAILED",
        {"content": "target agent is required"},
    )
    loose_agent = owner.request(
        "POST", "/api/v1/agents", {"name": f"Loose Agent {uuid.uuid4().hex[:6]}", "system_prompt": "Not shared"}
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/conversations/{conversation_id}/messages",
        403,
        "FORBIDDEN",
        {"content": "not allowed", "client_request_id": f"shared-loose-{uuid.uuid4()}", "target_agent_id": loose_agent["id"]},
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/conversations/{conversation_id}/messages",
        400,
        "VALIDATION_FAILED",
        {"content": "invalid target", "client_request_id": f"shared-invalid-{uuid.uuid4()}", "target_agent_id": "not-a-uuid"},
    )

    shared_request_id = f"shared-request-{uuid.uuid4()}"
    owner_send = send(owner, conversation_id, owner_agent["id"], "A sends to owner agent", shared_request_id)
    owner_run = owner_send["run_id"]
    owner_events = owner.events(owner_run)
    editor_events = editor.events(owner_run)
    assert_event_contract(owner_events)
    assert [(item["seq"], item["type"]) for item in owner_events] == [
        (item["seq"], item["type"]) for item in editor_events
    ]

    messages = owner.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    assert messages == editor.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    owner_pair = message_for_run(messages, owner_run)
    assert owner_pair[0]["sender_id"] == owner_user["id"]
    assert owner_pair[1]["sender_id"] == owner_agent["id"]

    editor_send = send(editor, conversation_id, editor_agent["id"], "B sends to editor agent", shared_request_id)
    editor_run = editor_send["run_id"]
    assert editor_run != owner_run
    assert editor_send["duplicate"] is False
    owner_events = owner.events(editor_run)
    editor_events = editor.events(editor_run)
    assert_event_contract(owner_events)
    assert [(item["seq"], item["type"]) for item in owner_events] == [
        (item["seq"], item["type"]) for item in editor_events
    ]

    editor_duplicate = send(editor, conversation_id, editor_agent["id"], "B sends to editor agent", shared_request_id)
    assert editor_duplicate["duplicate"] is True and editor_duplicate["run_id"] == editor_run
    messages = editor.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    editor_pair = message_for_run(messages, editor_run)
    assert editor_pair[0]["sender_id"] == editor_user["id"]
    assert editor_pair[1]["sender_id"] == editor_agent["id"]
    assert len([item for item in messages if item["content"] == "A sends to owner agent"]) == 1

    def concurrent_attempt(client, agent_id, content, request_id):
        try:
            return ("ok", send(client, conversation_id, agent_id, content, request_id))
        except ApiFailure as error:
            return ("error", error)

    with ThreadPoolExecutor(max_workers=2) as pool:
        concurrent_results = list(pool.map(lambda args: concurrent_attempt(*args), [
            (owner, owner_agent["id"], "concurrent owner send", f"shared-race-owner-{uuid.uuid4()}"),
            (editor, editor_agent["id"], "concurrent editor send", f"shared-race-editor-{uuid.uuid4()}"),
        ]))
    successes = [result for result in concurrent_results if result[0] == "ok"]
    conflicts = [result for result in concurrent_results if result[0] == "error"]
    assert len(successes) == 1, concurrent_results
    assert len(conflicts) == 1, concurrent_results
    assert conflicts[0][1].status == 409 and conflicts[0][1].code == "CONFLICT", conflicts[0][1]
    race_run = successes[0][1]["run_id"]
    race_events = owner.events(race_run) if successes[0][1]["message"]["sender_id"] == owner_user["id"] else editor.events(race_run)
    assert race_events[-1]["type"] == "message.completed", race_events[-1]

    expect_error(editor, "POST", f"/api/v1/runs/{owner_run}/retry", 403, "FORBIDDEN", None)

    cancel_send = send(editor, conversation_id, editor_agent["id"], "cancel this editor run", f"shared-cancel-{uuid.uuid4()}")
    cancel_run = cancel_send["run_id"]
    cancelled = editor.request("POST", f"/api/v1/runs/{cancel_run}/cancel")
    assert cancelled["status"] in {"queued", "running", "cancelled"}, cancelled
    cancel_events = editor.events(cancel_run)
    assert cancel_events[-1]["type"] == "run.cancelled", cancel_events[-1]
    owner_retry = owner.request(
        "POST",
        f"/api/v1/runs/{cancel_run}/retry",
        headers={"Idempotency-Key": f"shared-owner-retry-{uuid.uuid4()}"},
    )
    owner_retry_events = owner.events(owner_retry["run_id"])
    assert_event_contract(owner_retry_events)

    editor_retry = editor.request(
        "POST",
        f"/api/v1/runs/{cancel_run}/retry",
        headers={"Idempotency-Key": f"shared-editor-retry-{uuid.uuid4()}"},
    )
    editor_retry_events = editor.events(editor_retry["run_id"])
    assert_event_contract(editor_retry_events)

    owner_cancel_send = send(owner, conversation_id, owner_agent["id"], "owner run for cancel permission", f"shared-owner-cancel-{uuid.uuid4()}")
    owner_cancel_run = owner_cancel_send["run_id"]
    expect_error(editor, "POST", f"/api/v1/runs/{owner_cancel_run}/cancel", 403, "FORBIDDEN", None)
    try:
        owner_cancel = owner.request("POST", f"/api/v1/runs/{owner_cancel_run}/cancel")
        assert owner_cancel["status"] in {"queued", "running", "cancelled"}, owner_cancel
    except ApiFailure as error:
        assert error.status == 409 and error.code == "RUN_NOT_CANCELLABLE", (error.status, error.code)
    owner_cancel_events = owner.events(owner_cancel_run)
    assert owner_cancel_events[-1]["type"] in {"message.completed", "run.cancelled"}, owner_cancel_events[-1]

    editor_cancel_send = send(editor, conversation_id, editor_agent["id"], "editor run cancelled by owner", f"shared-owner-cancel-other-{uuid.uuid4()}")
    editor_cancel_run = editor_cancel_send["run_id"]
    try:
        cancelled_by_owner = owner.request("POST", f"/api/v1/runs/{editor_cancel_run}/cancel")
        assert cancelled_by_owner["status"] in {"queued", "running", "cancelled"}, cancelled_by_owner
    except ApiFailure as error:
        assert error.status == 409 and error.code == "RUN_NOT_CANCELLABLE", (error.status, error.code)
    editor_cancel_events = owner.events(editor_cancel_run)
    assert editor_cancel_events[-1]["type"] in {"message.completed", "run.cancelled"}, editor_cancel_events[-1]

    owner.request(
        "PATCH",
        f"/api/v1/projects/{project_id}/members/{editor_user['id']}",
        {"role": "viewer"},
    )
    viewer_messages = editor.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    assert viewer_messages
    viewer_events = editor.events(owner_run)
    assert_event_contract(viewer_events)
    expect_error(
        editor,
        "POST",
        f"/api/v1/conversations/{conversation_id}/messages",
        403,
        "FORBIDDEN",
        {"content": "viewer cannot send", "client_request_id": f"shared-viewer-{uuid.uuid4()}", "target_agent_id": owner_agent["id"]},
    )
    expect_error(editor, "POST", f"/api/v1/runs/{owner_run}/cancel", 403, "FORBIDDEN", None)
    expect_error(editor, "POST", f"/api/v1/runs/{cancel_run}/retry", 403, "FORBIDDEN", None)

    owner.request("DELETE", f"/api/v1/projects/{project_id}/members/{editor_user['id']}")
    expect_error(editor, "GET", f"/api/v1/conversations/{conversation_id}/messages", 403, "FORBIDDEN", None)
    expect_error(
        editor,
        "GET",
        f"/api/v1/runs/{owner_run}/events?request_id=shared-nonmember-{uuid.uuid4()}",
        403,
        "FORBIDDEN",
        None,
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/conversations/{conversation_id}/messages",
        403,
        "FORBIDDEN",
        {"content": "removed owner agent", "client_request_id": f"shared-removed-{uuid.uuid4()}", "target_agent_id": editor_agent["id"]},
    )

    print(json.dumps({
        "status": "PASS",
        "project_id": project_id,
        "conversation_id": conversation_id,
        "owner_agent_id": owner_agent["id"],
        "editor_agent_id": editor_agent["id"],
        "owner_run_id": owner_run,
        "editor_run_id": editor_run,
        "shared_history": True,
        "shared_sse": True,
        "sender_scoped_idempotency": True,
        "viewer_read_only": True,
        "non_member_denied": True,
        "removed_agent_owner_denied": True,
        "run_permissions": True,
        "invalid_target_validation": True,
        "concurrent_send_single_active": True,
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
