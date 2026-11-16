from __future__ import annotations

import json
import os
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from e2e import Client, assert_event_contract, login, send  # noqa: E402

PROJECT_NAME = os.environ.get("DEMO_PROJECT_NAME", "2H+2A 本地演示")
CONVERSATION_TITLE = os.environ.get("DEMO_CONVERSATION_TITLE", "共享协作演示")
OWNER_AGENT_NAME = os.environ.get("DEMO_OWNER_AGENT_NAME", "演示 Agent 1")
EDITOR_AGENT_NAME = os.environ.get("DEMO_EDITOR_AGENT_NAME", "演示 Agent 2")
OWNER_PROMPT = os.environ.get("DEMO_OWNER_PROMPT", "A 选择 Agent 1：请汇报项目当前协作状态。")
EDITOR_PROMPT = os.environ.get("DEMO_EDITOR_PROMPT", "B 选择 Agent 2：请补充下一项可执行工作。")


def find(items, **fields):
    return next((item for item in items if all(item.get(key) == value for key, value in fields.items())), None)


def ensure_agent(client, name, prompt):
    agent = find(client.request("GET", "/api/v1/agents")["items"], name=name)
    if agent:
        return agent
    return client.request("POST", "/api/v1/agents", {"name": name, "system_prompt": prompt})


def main() -> int:
    owner = Client()
    editor = Client()
    owner_user = login(owner, "demo", "demo1234", origin="http://localhost:8080")
    editor_user = login(editor, "other", "demo1234", origin="http://127.0.0.1:8080")

    projects = owner.request("GET", "/api/v1/projects")["items"]
    project = find(projects, name=PROJECT_NAME)
    if not project:
        project = owner.request(
            "POST",
            "/api/v1/projects",
            {"name": PROJECT_NAME, "description": "2 人 + 2 Agent 的本地演示 Project；可重复运行 seed 脚本。"},
        )
    project_id = project["id"]

    members = owner.request("GET", f"/api/v1/projects/{project_id}/members")["items"]
    editor_member = find(members, user_id=editor_user["id"])
    if not editor_member:
        owner.request("POST", f"/api/v1/projects/{project_id}/members", {"user_id": editor_user["id"], "role": "editor"})
    elif editor_member["role"] != "editor":
        owner.request("PATCH", f"/api/v1/projects/{project_id}/members/{editor_user['id']}", {"role": "editor"})

    owner_agent = ensure_agent(owner, OWNER_AGENT_NAME, "你是演示中的 Agent 1，代表 owner 回复。")
    editor_agent = ensure_agent(editor, EDITOR_AGENT_NAME, "你是演示中的 Agent 2，代表 editor 回复。")

    project_agents = owner.request("GET", f"/api/v1/projects/{project_id}/agents")["items"]
    if not find(project_agents, agent_id=owner_agent["id"]):
        owner.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": owner_agent["id"]})
    if not find(project_agents, agent_id=editor_agent["id"]):
        editor.request("POST", f"/api/v1/projects/{project_id}/agents", {"agent_id": editor_agent["id"]})

    conversations = owner.request("GET", f"/api/v1/projects/{project_id}/conversations")["items"]
    conversation = find(conversations, title=CONVERSATION_TITLE)
    if not conversation:
        conversation = owner.request(
            "POST",
            f"/api/v1/projects/{project_id}/conversations",
            {"agent_id": owner_agent["id"], "title": CONVERSATION_TITLE},
        )
    conversation_id = conversation["id"]

    messages = owner.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    has_owner = any(item.get("content") == OWNER_PROMPT for item in messages)
    has_editor = any(item.get("content") == EDITOR_PROMPT for item in messages)
    created_runs = []
    if not has_owner:
        result = send(owner, conversation_id, owner_agent["id"], OWNER_PROMPT, f"demo-owner-{uuid.uuid4()}")
        events = owner.events(result["run_id"])
        assert_event_contract(events)
        created_runs.append(result["run_id"])
    if not has_editor:
        result = send(editor, conversation_id, editor_agent["id"], EDITOR_PROMPT, f"demo-editor-{uuid.uuid4()}")
        events = editor.events(result["run_id"])
        assert_event_contract(events)
        created_runs.append(result["run_id"])

    messages = editor.request("GET", f"/api/v1/conversations/{conversation_id}/messages")["items"]
    report = {
        "status": "PASS",
        "project_id": project_id,
        "conversation_id": conversation_id,
        "accounts": {"owner": "demo / demo1234", "editor": "other / demo1234"},
        "agents": {"owner": owner_agent["name"], "editor": editor_agent["name"]},
        "messages": len(messages),
        "created_runs": created_runs,
        "browser_url": "http://localhost:8080",
        "note": "首次创建演示数据和消息；重复运行会复用同一 Project/Conversation 并补齐缺失数据。",
    }
    report_path = os.environ.get("DEMO_REPORT")
    if report_path:
        Path(report_path).write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
