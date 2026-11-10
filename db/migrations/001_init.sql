-- AI Workspace V2.0 base conversation schema
-- Migration: 001_init
-- Safe to rerun: all objects use IF NOT EXISTS and seeds use ON CONFLICT.
BEGIN;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version VARCHAR(32) PRIMARY KEY,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS users (
    id UUID PRIMARY KEY,
    username VARCHAR(80) NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    display_name VARCHAR(120) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'disabled')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_users_username_lower ON users (lower(username));

CREATE TABLE IF NOT EXISTS sessions (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash CHAR(64) NOT NULL UNIQUE,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    last_seen_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    revoked_at TIMESTAMPTZ
);
CREATE INDEX IF NOT EXISTS ix_sessions_user_active ON sessions(user_id, expires_at)
    WHERE revoked_at IS NULL;

CREATE TABLE IF NOT EXISTS model_providers (
    id UUID PRIMARY KEY,
    name VARCHAR(80) NOT NULL UNIQUE,
    provider_type VARCHAR(40) NOT NULL,
    base_url TEXT,
    model_name VARCHAR(120) NOT NULL,
    secret_ref VARCHAR(120),
    enabled BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS agents (
    id UUID PRIMARY KEY,
    owner_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(80) NOT NULL,
    avatar TEXT,
    system_prompt TEXT NOT NULL DEFAULT '',
    model_provider_id UUID NOT NULL REFERENCES model_providers(id),
    status VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'disabled')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_agents_owner ON agents(owner_id, created_at DESC);

CREATE TABLE IF NOT EXISTS conversations (
    id UUID PRIMARY KEY,
    owner_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    agent_id UUID NOT NULL REFERENCES agents(id),
    project_id UUID,
    title VARCHAR(200) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'archived')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_conversations_owner_updated ON conversations(owner_id, updated_at DESC);
CREATE INDEX IF NOT EXISTS ix_conversations_agent ON conversations(agent_id);

CREATE TABLE IF NOT EXISTS messages (
    id UUID PRIMARY KEY,
    conversation_id UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    seq BIGINT GENERATED ALWAYS AS IDENTITY,
    sender_type VARCHAR(16) NOT NULL CHECK (sender_type IN ('user', 'agent')),
    sender_id UUID,
    role VARCHAR(16) NOT NULL CHECK (role IN ('user', 'assistant')),
    content TEXT NOT NULL,
    run_id UUID,
    status VARCHAR(16) NOT NULL DEFAULT 'completed'
        CHECK (status IN ('streaming', 'completed', 'failed')),
    client_request_id VARCHAR(100),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_messages_seq ON messages(seq);
CREATE INDEX IF NOT EXISTS ix_messages_conversation_seq ON messages(conversation_id, seq);
CREATE UNIQUE INDEX IF NOT EXISTS ux_messages_client_request
    ON messages(conversation_id, client_request_id)
    WHERE sender_type = 'user' AND client_request_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS runs (
    id UUID PRIMARY KEY,
    conversation_id UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    agent_id UUID NOT NULL REFERENCES agents(id),
    trigger_message_id UUID NOT NULL REFERENCES messages(id),
    status VARCHAR(16) NOT NULL DEFAULT 'queued'
        CHECK (status IN ('queued', 'running', 'succeeded', 'failed', 'cancelled')),
    model_provider_id UUID NOT NULL REFERENCES model_providers(id),
    error_code VARCHAR(64),
    cancel_requested BOOLEAN NOT NULL DEFAULT false,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ
);
CREATE INDEX IF NOT EXISTS ix_runs_conversation_created ON runs(conversation_id, created_at DESC);
CREATE INDEX IF NOT EXISTS ix_runs_status_created ON runs(status, created_at);

DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'fk_messages_run') THEN
        ALTER TABLE messages
            ADD CONSTRAINT fk_messages_run FOREIGN KEY (run_id) REFERENCES runs(id) ON DELETE SET NULL;
    END IF;
END $$;
CREATE UNIQUE INDEX IF NOT EXISTS ux_messages_assistant_run
    ON messages(run_id) WHERE sender_type = 'agent' AND run_id IS NOT NULL;

CREATE TABLE IF NOT EXISTS jobs (
    id UUID PRIMARY KEY,
    run_id UUID NOT NULL UNIQUE REFERENCES runs(id) ON DELETE CASCADE,
    status VARCHAR(16) NOT NULL DEFAULT 'queued'
        CHECK (status IN ('queued', 'running', 'succeeded', 'failed', 'cancelled')),
    lease_token VARCHAR(96) UNIQUE,
    lease_until TIMESTAMPTZ,
    attempt_count INTEGER NOT NULL DEFAULT 0,
    max_attempts INTEGER NOT NULL DEFAULT 3 CHECK (max_attempts > 0),
    last_error_code VARCHAR(64),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_jobs_claim ON jobs(status, created_at) WHERE status = 'queued';
CREATE INDEX IF NOT EXISTS ix_jobs_lease_expiry ON jobs(lease_until) WHERE status = 'running';

CREATE TABLE IF NOT EXISTS run_events (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    run_id UUID NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
    seq BIGINT NOT NULL CHECK (seq > 0),
    event_type VARCHAR(40) NOT NULL,
    payload_jsonb JSONB NOT NULL DEFAULT '{}'::jsonb,
    request_id VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (run_id, seq)
);
CREATE INDEX IF NOT EXISTS ix_run_events_replay ON run_events(run_id, seq);
CREATE UNIQUE INDEX IF NOT EXISTS ux_run_events_terminal
    ON run_events(run_id, event_type)
    WHERE event_type IN ('run.started', 'message.completed', 'run.failed', 'run.cancelled');

CREATE TABLE IF NOT EXISTS audit_events (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    actor_type VARCHAR(16) NOT NULL CHECK (actor_type IN ('user', 'worker', 'system')),
    actor_id VARCHAR(120),
    action VARCHAR(80) NOT NULL,
    resource_type VARCHAR(40) NOT NULL,
    resource_id VARCHAR(120),
    result VARCHAR(16) NOT NULL CHECK (result IN ('allowed', 'denied', 'success', 'failure')),
    request_id VARCHAR(100) NOT NULL,
    metadata_jsonb JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS ix_audit_request ON audit_events(request_id);
CREATE INDEX IF NOT EXISTS ix_audit_resource ON audit_events(resource_type, resource_id, created_at DESC);

INSERT INTO schema_migrations(version) VALUES ('001_init')
ON CONFLICT (version) DO NOTHING;

COMMIT;