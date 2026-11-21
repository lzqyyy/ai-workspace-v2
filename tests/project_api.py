from __future__ import annotations

import json
import os
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import ApiFailure, Client, login  # noqa: E402


def expect_error(client: Client, method: str, path: str, status: int, code: str, body=None):
    try:
        client.request(method, path, body)
        raise AssertionError(f"expected HTTP {status} for {method} {path}")
    except ApiFailure as error:
        assert error.status == status and error.code == code, (error.status, error.code, error)


def main() -> int:
    owner = Client()
    member = Client()
    owner_user = login(owner, "demo", "demo1234")
    member_user = login(member, "other", "demo1234", origin="http://127.0.0.1:8080")
    fixture_checked = False

    fixture_project_id = os.environ.get("PROJECT_RUN_FIXTURE_PROJECT_ID")
    fixture_user_id = os.environ.get("PROJECT_RUN_FIXTURE_USER_ID")
    fixture_agent_id = os.environ.get("PROJECT_RUN_FIXTURE_AGENT_ID")
    fixture_run_id = os.environ.get("PROJECT_RUN_FIXTURE_RUN_ID")
    fixture_conversation_id = os.environ.get("PROJECT_RUN_FIXTURE_CONVERSATION_ID")
    if all((fixture_project_id, fixture_user_id, fixture_agent_id, fixture_run_id)):
        expect_error(owner, "DELETE", f"/api/v1/projects/{fixture_project_id}/members/{fixture_user_id}", 409, "CONFLICT")
        expect_error(owner, "DELETE", f"/api/v1/projects/{fixture_project_id}/agents/{fixture_agent_id}", 409, "CONFLICT")
        expect_error(owner, "POST", f"/api/v1/projects/{fixture_project_id}/archive", 409, "CONFLICT")
        if fixture_conversation_id:
            expect_error(
                owner,
                "POST",
                f"/api/v1/conversations/{fixture_conversation_id}/messages",
                400,
                "VALIDATION_FAILED",
                {"content": "missing target agent"},
            )
            expect_error(
                owner,
                "POST",
                f"/api/v1/conversations/{fixture_conversation_id}/messages",
                409,
                "CONFLICT",
                {"content": "active run blocks send", "client_request_id": f"project-active-{uuid.uuid4()}", "target_agent_id": fixture_agent_id},
            )
        owner.request("POST", f"/api/v1/runs/{fixture_run_id}/cancel")
        assert owner.request("DELETE", f"/api/v1/projects/{fixture_project_id}/agents/{fixture_agent_id}") is None
        assert owner.request("DELETE", f"/api/v1/projects/{fixture_project_id}/members/{fixture_user_id}") is None
        fixture_archive = owner.request("POST", f"/api/v1/projects/{fixture_project_id}/archive")
        assert fixture_archive["status"] == "archived", fixture_archive
        expect_error(
            owner,
            "POST",
            f"/api/v1/runs/{fixture_run_id}/retry",
            409,
            "CONFLICT",
            None,
        )
        assert owner.request("GET", f"/api/v1/projects/{fixture_project_id}")["status"] == "archived"
        fixture_checked = True

    project = owner.request(
        "POST",
        "/api/v1/projects",
        {"name": f"Package B Project {uuid.uuid4().hex[:8]}", "description": "Project API acceptance"},
    )
    project_id = project["id"]
    assert project["created_by_user_id"] == owner_user["id"] and project["status"] == "active"

    owner_projects = owner.request("GET", "/api/v1/projects")["items"]
    assert any(item["id"] == project_id for item in owner_projects)
    assert not any(item["id"] == project_id for item in member.request("GET", "/api/v1/projects")["items"])
    expect_error(member, "GET", f"/api/v1/projects/{project_id}", 403, "FORBIDDEN")
    expect_error(member, "GET", f"/api/v1/projects/{project_id}/members", 403, "FORBIDDEN")
    expect_error(member, "GET", f"/api/v1/projects/{project_id}/audit-events", 403, "FORBIDDEN")
    owner_audit = owner.request("GET", f"/api/v1/projects/{project_id}/audit-events")
    assert any(item["action"] == "project.create" for item in owner_audit["items"]), owner_audit

    added = owner.request(
        "POST",
        f"/api/v1/projects/{project_id}/members",
        {"user_id": member_user["id"], "role": "viewer"},
    )
    assert added["role"] == "viewer" and added["user_id"] == member_user["id"]
    assert member.request("GET", f"/api/v1/projects/{project_id}")["id"] == project_id
    members = member.request("GET", f"/api/v1/projects/{project_id}/members")["items"]
    assert {item["role"] for item in members} == {"owner", "viewer"}
    assert member.request("GET", f"/api/v1/projects/{project_id}/agents")["items"] == []
    expect_error(member, "GET", f"/api/v1/projects/{project_id}/audit-events", 403, "FORBIDDEN")

    member_agent = member.request(
        "POST", "/api/v1/agents", {"name": f"Member Agent {uuid.uuid4().hex[:8]}", "system_prompt": "Read only"}
    )
    expect_error(member, "POST", f"/api/v1/projects/{project_id}/agents", 403, "FORBIDDEN", {"agent_id": member_agent["id"]})
    expect_error(
        member,
        "POST",
        f"/api/v1/projects/{project_id}/members",
        403,
        "FORBIDDEN",
        {"user_id": owner_user["id"], "role": "viewer"},
    )

    owner_agent = owner.request("GET", "/api/v1/agents")["items"][0]
    project_agent = owner.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": owner_agent["id"]})
    assert project_agent["agent_id"] == owner_agent["id"] and project_agent["agent"]["id"] == owner_agent["id"]
    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{project_id}/agents",
        403,
        "FORBIDDEN",
        {"agent_id": member_agent["id"]},
    )

    promoted = owner.request(
        "PATCH", f"/api/v1/projects/{project_id}/members/{member_user['id']}", {"role": "editor"}
    )
    assert promoted["role"] == "editor"
    expect_error(member, "GET", f"/api/v1/projects/{project_id}/audit-events", 403, "FORBIDDEN")
    member_project_agent = member.request(
        "POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": member_agent["id"]}
    )
    assert member_project_agent["agent_id"] == member_agent["id"]
    expect_error(
        member, "POST", f"/api/v1/projects/{project_id}/agents", 409, "CONFLICT", {"agent_id": member_agent["id"]}
    )
    expect_error(
        member,
        "POST",
        f"/api/v1/projects/{project_id}/agents",
        403,
        "FORBIDDEN",
        {"agent_id": owner_agent["id"]},
    )
    expect_error(
        member,
        "PATCH",
        f"/api/v1/projects/{project_id}/members/{owner_user['id']}",
        403,
        "FORBIDDEN",
        {"role": "viewer"},
    )
    assert member.request("DELETE", f"/api/v1/projects/{project_id}/agents/{member_agent['id']}") is None
    member.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": member_agent["id"]})
    assert owner.request("DELETE", f"/api/v1/projects/{project_id}/agents/{member_agent['id']}") is None

    owner.request("PATCH", f"/api/v1/projects/{project_id}/members/{member_user['id']}", {"role": "owner"})
    downgraded = owner.request("PATCH", f"/api/v1/projects/{project_id}/members/{owner_user['id']}", {"role": "editor"})
    assert downgraded["role"] == "editor"
    expect_error(
        member,
        "PATCH",
        f"/api/v1/projects/{project_id}/members/{member_user['id']}",
        409,
        "CONFLICT",
        {"role": "editor"},
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{project_id}/members",
        403,
        "FORBIDDEN",
        {"user_id": owner_user["id"], "role": "viewer"},
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{project_id}/members",
        400,
        "VALIDATION_FAILED",
        {"user_id": member_user["id"], "role": "admin"},
    )

    archive_project = owner.request(
        "POST",
        "/api/v1/projects",
        {"name": f"Archive Project {uuid.uuid4().hex[:8]}", "description": "Soft close acceptance"},
    )
    archive_project_id = archive_project["id"]
    archive_conversation = None
    expect_error(member, "POST", f"/api/v1/projects/{archive_project_id}/archive", 403, "FORBIDDEN")

    owner.request(
        "POST",
        f"/api/v1/projects/{archive_project_id}/members",
        {"user_id": member_user["id"], "role": "viewer"},
    )
    expect_error(member, "POST", f"/api/v1/projects/{archive_project_id}/archive", 403, "FORBIDDEN")
    owner.request(
        "PATCH",
        f"/api/v1/projects/{archive_project_id}/members/{member_user['id']}",
        {"role": "editor"},
    )
    expect_error(member, "POST", f"/api/v1/projects/{archive_project_id}/archive", 403, "FORBIDDEN")
    owner.request("POST", f"/api/v1/projects/{archive_project_id}/agents", {"agent_id": owner_agent["id"]})
    archive_conversation = owner.request(
        "POST",
        f"/api/v1/projects/{archive_project_id}/conversations",
        {"agent_id": owner_agent["id"], "title": "Read-only after archive"},
    )
    owner.request("DELETE", f"/api/v1/projects/{archive_project_id}/members/{member_user['id']}")
    expect_error(member, "POST", f"/api/v1/projects/{archive_project_id}/archive", 403, "FORBIDDEN")

    archived = owner.request("POST", f"/api/v1/projects/{archive_project_id}/archive")
    assert archived["status"] == "archived", archived
    repeated_archive = owner.request("POST", f"/api/v1/projects/{archive_project_id}/archive")
    assert repeated_archive["status"] == "archived", repeated_archive
    assert owner.request("GET", f"/api/v1/projects/{archive_project_id}")["status"] == "archived"
    assert owner.request("GET", f"/api/v1/projects/{archive_project_id}/members")["items"]
    assert any(
        item["agent_id"] == owner_agent["id"]
        for item in owner.request("GET", f"/api/v1/projects/{archive_project_id}/agents")["items"]
    )
    assert any(
        item["id"] == archive_conversation["id"]
        for item in owner.request("GET", f"/api/v1/projects/{archive_project_id}/conversations")["items"]
    )
    assert owner.request("GET", f"/api/v1/conversations/{archive_conversation['id']}/messages")["items"] == []

    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{archive_project_id}/conversations",
        409,
        "CONFLICT",
        {"agent_id": owner_agent["id"], "title": "blocked"},
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/conversations/{archive_conversation['id']}/messages",
        409,
        "CONFLICT",
        {"content": "blocked after archive", "client_request_id": f"archive-block-{uuid.uuid4()}", "target_agent_id": owner_agent["id"]},
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{archive_project_id}/members",
        409,
        "CONFLICT",
        {"user_id": member_user["id"], "role": "viewer"},
    )
    expect_error(
        owner,
        "DELETE",
        f"/api/v1/projects/{archive_project_id}/members/{member_user['id']}",
        409,
        "CONFLICT",
    )
    expect_error(
        owner,
        "POST",
        f"/api/v1/projects/{archive_project_id}/agents",
        409,
        "CONFLICT",
        {"agent_id": owner_agent["id"]},
    )
    expect_error(
        owner,
        "PATCH",
        f"/api/v1/projects/{archive_project_id}/members/{owner_user['id']}",
        409,
        "CONFLICT",
        {"role": "editor"},
    )
    expect_error(
        owner,
        "DELETE",
        f"/api/v1/projects/{archive_project_id}/agents/{owner_agent['id']}",
        409,
        "CONFLICT",
    )

    archive_audit = owner.request("GET", f"/api/v1/projects/{archive_project_id}/audit-events")
    archive_events = [item for item in archive_audit["items"] if item["action"] == "project.archive"]
    assert archive_events, archive_audit
    sensitive_keys = {"content", "cookie", "authorization", "api_key", "password", "token"}
    for item in archive_audit["items"]:
        assert not (sensitive_keys & {key.lower() for key in item.get("metadata", {})}), item

    report = {
        "status": "PASS",
        "project_id": project_id,
        "member_id": member_user["id"],
        "owner_agent_id": owner_agent["id"],
        "member_agent_id": member_agent["id"],
        "active_run_removal_guard": fixture_checked,
        "archive_owner_only": True,
        "archive_active_run_guard": fixture_checked,
        "archived_readonly": True,
        "archive_audit": True,
        "audit_owner_only": True,
        "non_member_denied": True,
        "viewer_write_denied": True,
        "owner_manage_members": True,
        "editor_agent_self_service": True,
        "last_owner_guard": True,
    }
    report_path = os.environ.get("PROJECT_TEST_REPORT")
    if report_path:
        Path(report_path).write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
