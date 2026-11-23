-- Conversation-level realtime notifications. C++ Core is the only writer.
-- Additive and safe to rerun; Run SSE remains unchanged.
BEGIN;

CREATE TABLE IF NOT EXISTS conversation_events (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    conversation_id UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    seq BIGINT NOT NULL CHECK (seq > 0),
    event_type VARCHAR(40) NOT NULL,
    payload_jsonb JSONB NOT NULL DEFAULT '{}'::jsonb,
    request_id VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (conversation_id, seq)
);
CREATE INDEX IF NOT EXISTS ix_conversation_events_replay
    ON conversation_events(conversation_id, seq);

INSERT INTO schema_migrations(version) VALUES ('009_conversation_events')
ON CONFLICT (version) DO NOTHING;

COMMIT;
