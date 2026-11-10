BEGIN;
INSERT INTO users(id,username,password_hash,display_name,status)
VALUES ('00000000-0000-4000-8000-000000000102','other','pbkdf2_sha256$120000$a9c4d83e1f620b75$fb232a612527eb3952e6ab54eff23389c82955897069c3fd350853423591cabe','Other User','active')
ON CONFLICT (id) DO NOTHING;
INSERT INTO schema_migrations(version) VALUES ('003_test_user') ON CONFLICT (version) DO NOTHING;
COMMIT;