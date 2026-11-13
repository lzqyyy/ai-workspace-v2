-- Idempotency keys for retry requests. C++ Core is the only writer.
BEGIN;

CREATE TABLE IF NOT EXISTS retry_idempotency (
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    operation VARCHAR(40) NOT NULL,
    key VARCHAR(100) NOT NULL,
    run_id UUID NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
    source_run_id UUID REFERENCES runs(id) ON DELETE SET NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, operation, key)
);

CREATE INDEX IF NOT EXISTS ix_retry_idempotency_run ON retry_idempotency(run_id);

INSERT INTO schema_migrations(version) VALUES ('005_retry_idempotency')
ON CONFLICT (version) DO NOTHING;

COMMIT;
