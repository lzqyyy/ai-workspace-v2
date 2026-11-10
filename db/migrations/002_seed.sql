-- Local/demo seed. Never place live API keys here.
BEGIN;

INSERT INTO model_providers(id, name, provider_type, base_url, model_name, secret_ref, enabled)
VALUES ('00000000-0000-4000-8000-000000000001', 'Mock Stream', 'mock', NULL, 'mock-chat-v1', NULL, true)
ON CONFLICT (id) DO NOTHING;

INSERT INTO users(id, username, password_hash, display_name, status)
VALUES (
    '00000000-0000-4000-8000-000000000101',
    'demo',
    'pbkdf2_sha256$120000$5f8e2f9c4d6b7a10$e7cac208b2448e138d2a708a43b455a749e6727f0ccf44d973a2b23986173ae5',
    'Demo User',
    'active'
)
ON CONFLICT (id) DO NOTHING;

INSERT INTO agents(id, owner_id, name, system_prompt, model_provider_id, status)
VALUES (
    '00000000-0000-4000-8000-000000000201',
    '00000000-0000-4000-8000-000000000101',
    'Workspace Assistant',
    'You are a concise, helpful workspace assistant.',
    '00000000-0000-4000-8000-000000000001',
    'active'
)
ON CONFLICT (id) DO NOTHING;

INSERT INTO schema_migrations(version) VALUES ('002_seed')
ON CONFLICT (version) DO NOTHING;

COMMIT;