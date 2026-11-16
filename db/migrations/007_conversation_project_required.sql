-- Package C compatibility prerequisite: after the C++ conversation writer always
-- supplies project_id, existing rows are backfilled by 006 and the column can be
-- tightened without a nullable-write window. This migration is additive and safe
-- to rerun.
BEGIN;

ALTER TABLE conversations ALTER COLUMN project_id SET NOT NULL;

INSERT INTO schema_migrations(version) VALUES ('007_project_id_required')
ON CONFLICT (version) DO NOTHING;

COMMIT;
