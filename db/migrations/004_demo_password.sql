-- Demo credentials must satisfy the 8-character password policy.
BEGIN;

UPDATE users
SET password_hash = 'pbkdf2_sha256$120000$5f8e2f9c4d6b7a10$2db6d2170bcc5f45db9bb35532ccd3cecf1bd133fd53a1bb3e292af94f3570cc',
    updated_at = now()
WHERE username = 'demo';

UPDATE users
SET password_hash = 'pbkdf2_sha256$120000$a9c4d83e1f620b75$c6cae05ca92966a5d638417a7e2e07b1547f40058b62d8136f31773e73a89d5f',
    updated_at = now()
WHERE username = 'other';

INSERT INTO schema_migrations(version) VALUES ('004_demo_password')
ON CONFLICT (version) DO NOTHING;

COMMIT;