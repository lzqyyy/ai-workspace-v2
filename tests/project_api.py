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
    if all((fixture_project_id, fixture_user_id, fixture_agent_id, fixture_run_id)):
        expect_error(owner, "DELETE", f"/api/v1/projects/{fixture_project_id}/members/{fixture_user_id}", 409, "CONFLICT")
        expect_error(owner, "DELETE", f"/api/v1/projects/{fixture_project_id}/agents/{fixture_agent_id}", 409, "CONFLICT")
        owner.request("POST", f"/api/v1/runs/{fixture_run_id}/cancel")
        assert owner.request("DELETE", f"/api/v1/projects/{fixture_project_id}/agents/{fixture_agent_id}") is None
        assert owner.request("DELETE", f"/api/v1/projects/{fixture_project_id}/members/{fixture_user_id}") is None
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

    report = {
        "status": "PASS",
        "project_id": project_id,
        "member_id": member_user["id"],
        "owner_agent_id": owner_agent["id"],
        "member_agent_id": member_agent["id"],
        "active_run_removal_guard": fixture_checked,
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
