-- Minimal multiplayer foundation. This migration is additive and safe to rerun.
-- C++ Core remains the only business writer. Worker access is unchanged.
BEGIN;

CREATE TABLE IF NOT EXISTS projects (
    id UUID PRIMARY KEY,
    name VARCHAR(120) NOT NULL CHECK (btrim(name) <> ''),
    description TEXT,
    status VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'archived')),
    created_by_user_id UUID NOT NULL REFERENCES users(id),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_projects_created_by_updated
    ON projects(created_by_user_id, updated_at DESC);

CREATE TABLE IF NOT EXISTS project_members (
    project_id UUID NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role VARCHAR(16) NOT NULL CHECK (role IN ('owner', 'editor', 'viewer')),
    added_by_user_id UUID REFERENCES users(id) ON DELETE SET NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (project_id, user_id)
);
CREATE INDEX IF NOT EXISTS ix_project_members_user
    ON project_members(user_id, project_id);
CREATE INDEX IF NOT EXISTS ix_project_members_project_role
    ON project_members(project_id, role);

CREATE TABLE IF NOT EXISTS project_agents (
    project_id UUID NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    agent_id UUID NOT NULL REFERENCES agents(id) ON DELETE CASCADE,
    added_by_user_id UUID NOT NULL REFERENCES users(id),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (project_id, agent_id)
);
CREATE INDEX IF NOT EXISTS ix_project_agents_agent
    ON project_agents(agent_id, project_id);

ALTER TABLE conversations ADD COLUMN IF NOT EXISTS project_id UUID;

-- One deterministic legacy Project per owner represented by existing Agents or Conversations.
WITH legacy_owners AS (
    SELECT owner_id AS user_id FROM agents
    UNION
    SELECT owner_id AS user_id FROM conversations
)
INSERT INTO projects(id, name, description, status, created_by_user_id)
SELECT
    md5('ai-workspace-v2:legacy-project:' || lo.user_id::text)::uuid,
    left('Legacy project - ' || COALESCE(NULLIF(btrim(u.display_name), ''), u.username), 120),
    'Auto-created for data that predates Project membership.',
    'active',
    lo.user_id
FROM legacy_owners lo
JOIN users u ON u.id = lo.user_id
ON CONFLICT (id) DO NOTHING;

WITH legacy_owners AS (
    SELECT owner_id AS user_id FROM agents
    UNION
    SELECT owner_id AS user_id FROM conversations
)
INSERT INTO project_members(project_id, user_id, role, added_by_user_id)
SELECT
    md5('ai-workspace-v2:legacy-project:' || lo.user_id::text)::uuid,
    lo.user_id,
    'owner',
    lo.user_id
FROM legacy_owners lo
ON CONFLICT (project_id, user_id) DO NOTHING;

INSERT INTO project_agents(project_id, agent_id, added_by_user_id)
SELECT
    md5('ai-workspace-v2:legacy-project:' || a.owner_id::text)::uuid,
    a.id,
    a.owner_id
FROM agents a
ON CONFLICT (project_id, agent_id) DO NOTHING;

UPDATE conversations c
SET project_id = md5('ai-workspace-v2:legacy-project:' || c.owner_id::text)::uuid
WHERE c.project_id IS NULL;

DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'fk_conversations_project') THEN
        ALTER TABLE conversations
            ADD CONSTRAINT fk_conversations_project
            FOREIGN KEY (project_id) REFERENCES projects(id);
    END IF;
END $$;

CREATE INDEX IF NOT EXISTS ix_conversations_project_updated
    ON conversations(project_id, updated_at DESC)
    WHERE project_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS ix_conversations_project_id
    ON conversations(project_id, id)
    WHERE project_id IS NOT NULL;

-- The previous index treated the same client key as global within a Conversation.
-- User idempotency must instead be scoped to the sender.
DROP INDEX IF EXISTS ux_messages_client_request;
CREATE UNIQUE INDEX ux_messages_client_request
    ON messages(conversation_id, sender_id, client_request_id)
    WHERE sender_type = 'user'
      AND sender_id IS NOT NULL
      AND client_request_id IS NOT NULL;


INSERT INTO schema_migrations(version) VALUES ('006_multiplayer_foundation')
ON CONFLICT (version) DO NOTHING;

COMMIT;
