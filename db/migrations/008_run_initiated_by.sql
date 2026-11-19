-- Explicit Run attribution: distinguish the immutable trigger-message author
-- from the user who actually started or retried this Run. Additive and rerunnable.
BEGIN;

ALTER TABLE runs
    ADD COLUMN IF NOT EXISTS initiated_by_user_id UUID REFERENCES users(id);

-- Existing Runs were always started by the user who sent the trigger message.
UPDATE runs r
SET initiated_by_user_id = m.sender_id
FROM messages m
WHERE m.id = r.trigger_message_id
  AND m.sender_type = 'user'
  AND r.initiated_by_user_id IS NULL;

-- Defensive fallback for orphaned legacy data; new writes always set the column.
UPDATE runs r
SET initiated_by_user_id = c.owner_id
FROM conversations c
WHERE c.id = r.conversation_id
  AND r.initiated_by_user_id IS NULL;

DO $$
BEGIN
    IF EXISTS (SELECT 1 FROM runs WHERE initiated_by_user_id IS NULL) THEN
        RAISE EXCEPTION 'runs.initiated_by_user_id backfill left NULL rows';
    END IF;
END $$;

ALTER TABLE runs
    ALTER COLUMN initiated_by_user_id SET NOT NULL;

CREATE INDEX IF NOT EXISTS ix_runs_initiated_by_created
    ON runs(initiated_by_user_id, created_at DESC);

INSERT INTO schema_migrations(version) VALUES ('008_run_initiated_by')
ON CONFLICT (version) DO NOTHING;

COMMIT;
