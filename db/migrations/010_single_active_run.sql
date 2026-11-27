-- Defense-in-depth for the single active Run invariant. The application keeps the
-- Project + Conversation row locks so normal conflicts return 409 without relying
-- on a unique violation; this index protects every remaining insert path.
BEGIN;

DO $$
BEGIN
    IF EXISTS (
        SELECT 1
        FROM runs
        WHERE status IN ('queued', 'running')
        GROUP BY conversation_id
        HAVING count(*) > 1
    ) THEN
        RAISE EXCEPTION 'cannot create ux_runs_one_active_per_conversation: duplicate active runs exist';
    END IF;
END $$;

CREATE UNIQUE INDEX IF NOT EXISTS ux_runs_one_active_per_conversation
    ON runs(conversation_id)
    WHERE status IN ('queued', 'running');

INSERT INTO schema_migrations(version) VALUES ('010_single_active_run')
ON CONFLICT (version) DO NOTHING;

COMMIT;
