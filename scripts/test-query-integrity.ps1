param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if (!$Docker) { throw 'This check is Docker-first only. Use: ./scripts/test-query-integrity.ps1 -Docker' }
if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker CLI not found.' }

$root = Split-Path -Parent $PSScriptRoot
$composeArgs = @('compose')
if ($env:COMPOSE_PROJECT_NAME) { $composeArgs += @('-p', $env:COMPOSE_PROJECT_NAME) }
$composeArgs += @('--env-file', (Join-Path $root '.env'), '-f', (Join-Path $root 'deploy\docker-compose.yml'))

function Invoke-Sql([string]$Sql) {
    $output = $Sql | & docker @composeArgs exec -T postgres sh -lc 'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" -tA -v ON_ERROR_STOP=1'
    if ($LASTEXITCODE -ne 0) { throw 'Query integrity SQL failed' }
    return ($output | Out-String).Trim()
}

$project = '00000000-0000-4000-8000-000000000310'
$conversation = '00000000-0000-4000-8000-000000000410'
$run = '00000000-0000-4000-8000-000000000610'
$secondRun = '00000000-0000-4000-8000-000000000611'
$demoUser = '00000000-0000-4000-8000-000000000101'
$agent = '00000000-0000-4000-8000-000000000201'
$provider = '00000000-0000-4000-8000-000000000001'
$trigger = "md5('query-integrity-message-1')::uuid"

$cleanup = @"
BEGIN;
DELETE FROM runs WHERE conversation_id='$conversation';
DELETE FROM audit_events WHERE resource_id IN ('$project','$conversation','$run');
DELETE FROM messages WHERE conversation_id='$conversation';
DELETE FROM conversations WHERE id='$conversation';
DELETE FROM project_agents WHERE project_id='$project';
DELETE FROM project_members WHERE project_id='$project';
DELETE FROM projects WHERE id='$project';
COMMIT;
"@
Invoke-Sql $cleanup | Out-Null

$seed = @"
BEGIN;
INSERT INTO projects(id,name,status,created_by_user_id)
VALUES ('$project','Query integrity fixture','active','$demoUser');
INSERT INTO project_members(project_id,user_id,role,added_by_user_id)
VALUES ('$project','$demoUser','owner','$demoUser');
INSERT INTO project_agents(project_id,agent_id,added_by_user_id)
VALUES ('$project','$agent','$demoUser');
INSERT INTO conversations(id,owner_id,agent_id,project_id,title,status)
VALUES ('$conversation','$demoUser','$agent','$project','Query integrity conversation','active');
INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status)
SELECT md5('query-integrity-message-' || g)::uuid, '$conversation', 'user', '$demoUser', 'user', 'message ' || g, 'completed'
FROM generate_series(1,2000) AS g;
DO `$`$
BEGIN
    INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id)
    VALUES ('$run','$conversation','$agent',$trigger,'$provider','queued','$demoUser');
    BEGIN
        INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id)
        VALUES ('$secondRun','$conversation','$agent',$trigger,'$provider','queued','$demoUser');
        RAISE EXCEPTION 'second active Run unexpectedly succeeded';
    EXCEPTION WHEN unique_violation THEN
        NULL;
    END;
    IF (SELECT count(*) FROM runs WHERE conversation_id='$conversation' AND status IN ('queued','running')) <> 1 THEN
        RAISE EXCEPTION 'single active Run constraint did not hold';
    END IF;
    UPDATE runs SET status='succeeded',finished_at=now() WHERE id='$run';
END `$`$;
INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id)
SELECT '$run', g, 'message.delta', jsonb_build_object('delta','x'), 'query-integrity'
FROM generate_series(1,2000) AS g;
INSERT INTO audit_events(actor_type,actor_id,action,resource_type,resource_id,result,request_id,metadata_jsonb)
SELECT 'user','$demoUser','message.send','project','$project','success','query-integrity',jsonb_build_object('role','owner')
FROM generate_series(1,2000);
ANALYZE messages;
ANALYZE run_events;
ANALYZE audit_events;
COMMIT;
"@
Invoke-Sql $seed | Out-Null

$messagePlan = Invoke-Sql "EXPLAIN (FORMAT JSON) SELECT m.id FROM messages m WHERE m.conversation_id='$conversation' AND m.seq>1900 ORDER BY m.seq LIMIT 201;"
if ($messagePlan -match '"Node Type"\s*:\s*"Seq Scan"') { throw "message incremental query used Seq Scan: $messagePlan" }

$runEventPlan = Invoke-Sql "EXPLAIN (FORMAT JSON) SELECT seq FROM run_events WHERE run_id='$run' AND seq>1900 ORDER BY seq LIMIT 200;"
if ($runEventPlan -match '"Node Type"\s*:\s*"Seq Scan"') { throw "RunEvent incremental query used Seq Scan: $runEventPlan" }

$indexes = Invoke-Sql "SELECT count(*) FROM pg_indexes WHERE indexname IN ('ix_messages_conversation_seq','ix_run_events_replay','ix_audit_resource','ix_projects_created_by_updated','ix_project_members_user');"
if ([int]$indexes -ne 5) { throw "expected 5 query indexes, got $indexes" }

$constraint = Invoke-Sql "SELECT count(*) FROM pg_indexes WHERE indexname='ux_runs_one_active_per_conversation' AND indexdef LIKE '%UNIQUE INDEX%' AND indexdef LIKE '%status%';"
if ([int]$constraint -ne 1) { throw 'single active Run unique index is missing' }

Invoke-Sql $cleanup | Out-Null

Write-Output (([ordered]@{
    status = 'PASS'
    message_index = 'PASS'
    run_event_index = 'PASS'
    project_indexes = 'PASS'
    active_run_constraint = 'PASS'
}) | ConvertTo-Json -Compress)
