param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
$composeArgs = @('compose')
if ($env:COMPOSE_PROJECT_NAME) { $composeArgs += @('-p', $env:COMPOSE_PROJECT_NAME) }
$composeArgs += @('--env-file', (Join-Path $root '.env'), '-f', (Join-Path $root 'deploy\docker-compose.yml'))

function Invoke-Compose([string[]]$Arguments) {
    & docker @composeArgs @Arguments
    if ($LASTEXITCODE -ne 0) { throw "docker compose failed: $($Arguments -join ' ')" }
}

function Invoke-ProjectSql([string]$Sql) {
    $Sql | & docker @composeArgs exec -T postgres sh -lc 'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" -v ON_ERROR_STOP=1'
    if ($LASTEXITCODE -ne 0) { throw 'Project fixture SQL failed' }
}

if ($Docker) {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker CLI not found.' }
    $network = if ($env:E2E_NETWORK) { $env:E2E_NETWORK } else { 'ai-workspace-v2_internal' }
    Push-Location $root
    try {
        & docker run --rm -v "${root}:/src" -v ai-workspace-v2-go-mod:/go/pkg/mod -w /src -e GOTOOLCHAIN=local golang:1.25-bookworm go build -buildvcs=false -o build/duplicate-callback-linux ./tests/duplicate_callback
        if ($LASTEXITCODE -ne 0) { throw 'Failed to build the Linux callback helper in Docker' }

        & docker run --rm --network $network -v "${root}:/src" -w /src `
            -e BASE_URL=http://core:8080 -e DUPLICATE_HELPER=/src/build/duplicate-callback-linux -e GRPC_ADDR=core:50051 `
            python:3.12-slim python tests/e2e.py
        if ($LASTEXITCODE -ne 0) { throw 'Docker E2E failed' }

        & docker run --rm --network $network -v "${root}:/src" -w /src `
            -e BASE_URL=http://core:8080 `
            python:3.12-slim python tests/shared_conversation.py
        if ($LASTEXITCODE -ne 0) { throw 'Docker shared Conversation E2E failed' }

        Invoke-Compose @('stop', 'worker')
        try {
            & docker run --rm --network $network -v "${root}:/src" -w /src `
                -e BASE_URL=http://core:8080 `
                python:3.12-slim python tests/conversation_realtime.py
            if ($LASTEXITCODE -ne 0) { throw 'Docker Conversation realtime E2E failed' }
        }
        finally { Invoke-Compose @('start', 'worker') }

        $fixtureProject = '00000000-0000-4000-8000-000000000301'
        $fixtureConversation = '00000000-0000-4000-8000-000000000401'
        $fixtureMessage = '00000000-0000-4000-8000-000000000501'
        $fixtureRun = '00000000-0000-4000-8000-000000000601'
        $fixtureJob = '00000000-0000-4000-8000-000000000701'
        $cleanupSql = @"
BEGIN;
DELETE FROM runs WHERE conversation_id='$fixtureConversation';
DELETE FROM messages WHERE conversation_id='$fixtureConversation';
DELETE FROM conversations WHERE id='$fixtureConversation';
DELETE FROM project_agents WHERE project_id='$fixtureProject';
DELETE FROM project_members WHERE project_id='$fixtureProject';
DELETE FROM projects WHERE id='$fixtureProject';
COMMIT;
"@
        $seedSql = @"
BEGIN;
INSERT INTO projects(id,name,description,status,created_by_user_id)
VALUES ('$fixtureProject','Package B active run fixture','Runtime removal guard fixture','active','00000000-0000-4000-8000-000000000101');
INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES
('$fixtureProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$fixtureProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000101');
INSERT INTO project_agents(project_id,agent_id,added_by_user_id)
VALUES ('$fixtureProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101');
INSERT INTO conversations(id,owner_id,agent_id,project_id,title,status)
VALUES ('$fixtureConversation','00000000-0000-4000-8000-000000000101','00000000-0000-4000-8000-000000000201','$fixtureProject','Package B guard conversation','active');
INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status)
VALUES ('$fixtureMessage','$fixtureConversation','user','00000000-0000-4000-8000-000000000102','user','fixture trigger','completed');
INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id)
VALUES ('$fixtureRun','$fixtureConversation','00000000-0000-4000-8000-000000000201','$fixtureMessage','00000000-0000-4000-8000-000000000001','queued','00000000-0000-4000-8000-000000000102');
-- Keep the worker from claiming the fixture; the Run itself remains active for the removal guard.
INSERT INTO jobs(id,run_id,status) VALUES ('$fixtureJob','$fixtureRun','queued');
COMMIT;
"@
        Invoke-Compose @('stop', 'worker')
        try {
            Invoke-ProjectSql $cleanupSql
            Invoke-ProjectSql $seedSql

            & docker run --rm --network $network -v "${root}:/src" -w /src `
                -e BASE_URL=http://core:8080 `
                -e PROJECT_RUN_FIXTURE_PROJECT_ID=$fixtureProject `
                -e PROJECT_RUN_FIXTURE_USER_ID=00000000-0000-4000-8000-000000000102 `
                -e PROJECT_RUN_FIXTURE_AGENT_ID=00000000-0000-4000-8000-000000000201 `
                -e PROJECT_RUN_FIXTURE_RUN_ID=$fixtureRun `
                -e PROJECT_RUN_FIXTURE_CONVERSATION_ID=$fixtureConversation `
                -e PROJECT_TEST_REPORT=/src/build/project-api-report.json `
                python:3.12-slim python tests/project_api.py
            if ($LASTEXITCODE -ne 0) { throw 'Docker Project API E2E failed' }

            $report = Get-Content -LiteralPath (Join-Path $root 'build\project-api-report.json') -Raw | ConvertFrom-Json
            if ($report.status -ne 'PASS' -or !$report.active_run_removal_guard) { throw 'Project API report did not confirm active-run guards' }
            $auditSql = @'
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.create' AND created_at > now()-interval '30 minutes') THEN
        RAISE EXCEPTION 'missing project.create audit';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.member.role_change' AND created_at > now()-interval '30 minutes') THEN
        RAISE EXCEPTION 'missing project.member.role_change audit';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.agent.add' AND created_at > now()-interval '30 minutes') THEN
        RAISE EXCEPTION 'missing project.agent.add audit';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='permission.denied' AND created_at > now()-interval '30 minutes') THEN
        RAISE EXCEPTION 'missing permission.denied audit';
    END IF;
END $$;
'@
            Invoke-ProjectSql $auditSql
        }
        finally {
            try { Invoke-ProjectSql $cleanupSql }
            finally { Invoke-Compose @('start', 'worker') }
        }
    }
    finally { Pop-Location }
    return
}

$python = $null
$pythonArgs = @()
if ($env:PYTHON) { $python = $env:PYTHON }
if (!$python) {
    foreach ($candidate in @(
        (Join-Path $root '.tools\python\python.exe'),
        (Join-Path $root '.tools\msys64\ucrt64\bin\python.exe')
    )) {
        if (Test-Path -LiteralPath $candidate) { $python = $candidate; break }
    }
}
if (!$python) {
    $command = Get-Command python -ErrorAction SilentlyContinue
    if (!$command) { $command = Get-Command python3 -ErrorAction SilentlyContinue }
    if ($command) { $python = $command.Source }
}
if (!$python) {
    $command = Get-Command py -ErrorAction SilentlyContinue
    if ($command) { $python = $command.Source; $pythonArgs = @('-3') }
}
if (!$python) {
    throw 'Python 3 is required for E2E. Install Python 3, or use Docker-first: ./scripts/e2e.ps1 -Docker.'
}

Push-Location $root
try {
    & $python @pythonArgs (Join-Path $root 'tests\e2e.py')
    if ($LASTEXITCODE -ne 0) { throw 'E2E failed' }
    & $python @pythonArgs (Join-Path $root 'tests\shared_conversation.py')
    if ($LASTEXITCODE -ne 0) { throw 'Shared Conversation E2E failed' }
    & $python @pythonArgs (Join-Path $root 'tests\conversation_realtime.py')
    if ($LASTEXITCODE -ne 0) { throw 'Conversation realtime E2E failed' }
    & $python @pythonArgs (Join-Path $root 'tests\project_api.py')
    if ($LASTEXITCODE -ne 0) { throw 'Project API E2E failed' }
}
finally { Pop-Location }
