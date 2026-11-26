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
        $permissionEditorProject = '00000000-0000-4000-8000-000000000302'
        $permissionViewerProject = '00000000-0000-4000-8000-000000000303'
        $permissionAgentlessProject = '00000000-0000-4000-8000-000000000304'
        $permissionArchivedProject = '00000000-0000-4000-8000-000000000305'
        $fixtureConversation = '00000000-0000-4000-8000-000000000401'
        $fixtureMessage = '00000000-0000-4000-8000-000000000501'
        $fixtureRun = '00000000-0000-4000-8000-000000000601'
        $fixtureJob = '00000000-0000-4000-8000-000000000701'
        $fixtureOwnerConversation = '00000000-0000-4000-8000-000000000402'
        $fixtureOwnerMessage = '00000000-0000-4000-8000-000000000502'
        $fixtureOwnerRun = '00000000-0000-4000-8000-000000000602'
        $fixtureOwnerJob = '00000000-0000-4000-8000-000000000702'
        $cleanupSql = @"
BEGIN;
DELETE FROM runs WHERE conversation_id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM messages WHERE conversation_id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM conversations WHERE id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM project_agents WHERE project_id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM project_members WHERE project_id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM projects WHERE id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM conversations WHERE project_id=md5('ai-workspace-v2:legacy-project:' || '00000000-0000-4000-8000-000000000102')::uuid AND title='Legacy archive fixture';
UPDATE projects SET status='active',updated_at=now() WHERE id=md5('ai-workspace-v2:legacy-project:' || '00000000-0000-4000-8000-000000000102')::uuid;
COMMIT;
"@
        $seedSql = @"
BEGIN;
INSERT INTO projects(id,name,description,status,created_by_user_id)
VALUES ('$fixtureProject','Package B active run fixture','Runtime removal guard fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','Permission editor fixture','Action-aware permission fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','Permission viewer fixture','Viewer permission fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionAgentlessProject','Permission agentless fixture','Agent project context fixture','active','00000000-0000-4000-8000-000000000102'),
('$permissionArchivedProject','Permission archived fixture','Archived action permission fixture','archived','00000000-0000-4000-8000-000000000101');
INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES
('$fixtureProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$fixtureProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000102','viewer','00000000-0000-4000-8000-000000000101'),
('$permissionAgentlessProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000102'),
('$permissionArchivedProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101');
INSERT INTO project_agents(project_id,agent_id,added_by_user_id)
VALUES ('$fixtureProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionArchivedProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101');
INSERT INTO conversations(id,owner_id,agent_id,project_id,title,status)
VALUES ('$fixtureConversation','00000000-0000-4000-8000-000000000101','00000000-0000-4000-8000-000000000201','$fixtureProject','Package B editor guard conversation','active'),
('$fixtureOwnerConversation','00000000-0000-4000-8000-000000000101','00000000-0000-4000-8000-000000000201','$fixtureProject','Package B owner guard conversation','active');
INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status)
VALUES ('$fixtureMessage','$fixtureConversation','user','00000000-0000-4000-8000-000000000102','user','editor fixture trigger','completed'),
('$fixtureOwnerMessage','$fixtureOwnerConversation','user','00000000-0000-4000-8000-000000000101','user','owner fixture trigger','completed');
INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id)
VALUES ('$fixtureRun','$fixtureConversation','00000000-0000-4000-8000-000000000201','$fixtureMessage','00000000-0000-4000-8000-000000000001','queued','00000000-0000-4000-8000-000000000102'),
('$fixtureOwnerRun','$fixtureOwnerConversation','00000000-0000-4000-8000-000000000201','$fixtureOwnerMessage','00000000-0000-4000-8000-000000000001','cancelled','00000000-0000-4000-8000-000000000101');
-- Keep the worker from claiming the fixture; the Run itself remains active for the removal guard.
INSERT INTO jobs(id,run_id,status) VALUES ('$fixtureJob','$fixtureRun','queued'),
('$fixtureOwnerJob','$fixtureOwnerRun','cancelled');
COMMIT;
"@
        Invoke-Compose @('stop', 'worker')
        try {
            Invoke-ProjectSql $cleanupSql
            Invoke-ProjectSql $seedSql

            & docker run --rm --network $network -v "${root}:/src" -v ai-workspace-v2-go-mod:/go/pkg/mod -w /src `
                -e GOTOOLCHAIN=local -e GRPC_ADDR=core:50051 `
                -e PERMISSION_OWNER=00000000-0000-4000-8000-000000000101 `
                -e PERMISSION_EDITOR=00000000-0000-4000-8000-000000000102 `
                -e PERMISSION_AGENT=00000000-0000-4000-8000-000000000201 `
                -e PERMISSION_EDITOR_PROJECT=$permissionEditorProject `
                -e PERMISSION_VIEWER_PROJECT=$permissionViewerProject `
                -e PERMISSION_AGENTLESS_PROJECT=$permissionAgentlessProject `
                -e PERMISSION_ARCHIVED_PROJECT=$permissionArchivedProject `
                -e PERMISSION_FIXTURE_PROJECT=$fixtureProject `
                -e PERMISSION_OWNER_RUN=$fixtureOwnerRun `
                -e PERMISSION_EDITOR_RUN=$fixtureRun `
                golang:1.25-bookworm go run -buildvcs=false ./tests/permission_rpc
            if ($LASTEXITCODE -ne 0) { throw 'Docker Permission RPC E2E failed' }

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
            if ($report.status -ne 'PASS' -or !$report.active_run_removal_guard -or !$report.archive_owner_only -or !$report.archived_readonly -or !$report.archive_audit) { throw 'Project API report did not confirm permission/archive guards' }
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
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.archive' AND created_at > now()-interval '30 minutes') THEN
        RAISE EXCEPTION 'missing project.archive audit';
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

    # Match the Docker path: keep fixture Runs queued while running cancellation and permission checks.
    $workerPidFile = Join-Path $root 'build\worker.pid'
    if (!(Test-Path -LiteralPath $workerPidFile)) { throw 'Local worker PID file is missing; start with ./scripts/dev.ps1 -NoBrowser first.' }
    $workerProcessId = [int](Get-Content -LiteralPath $workerPidFile -Raw)
    Stop-Process -Id $workerProcessId -Force -ErrorAction Stop
    Remove-Item -LiteralPath $workerPidFile -Force
    try {
        & $python @pythonArgs (Join-Path $root 'tests\conversation_realtime.py')
        if ($LASTEXITCODE -ne 0) { throw 'Conversation realtime E2E failed' }

        $fixtureProject = '00000000-0000-4000-8000-000000000301'
        $permissionEditorProject = '00000000-0000-4000-8000-000000000302'
        $permissionViewerProject = '00000000-0000-4000-8000-000000000303'
        $permissionAgentlessProject = '00000000-0000-4000-8000-000000000304'
        $permissionArchivedProject = '00000000-0000-4000-8000-000000000305'
        $fixtureConversation = '00000000-0000-4000-8000-000000000401'
        $fixtureMessage = '00000000-0000-4000-8000-000000000501'
        $fixtureRun = '00000000-0000-4000-8000-000000000601'
        $fixtureJob = '00000000-0000-4000-8000-000000000701'
        $fixtureOwnerConversation = '00000000-0000-4000-8000-000000000402'
        $fixtureOwnerMessage = '00000000-0000-4000-8000-000000000502'
        $fixtureOwnerRun = '00000000-0000-4000-8000-000000000602'
        $fixtureOwnerJob = '00000000-0000-4000-8000-000000000702'
        $psql = Join-Path $root '.tools\msys64\ucrt64\bin\psql.exe'
        if (!(Test-Path -LiteralPath $psql)) { throw 'Local PostgreSQL client is missing.' }
        function Invoke-LocalProjectSql([string]$Sql) {
            $Sql | & $psql -h 127.0.0.1 -p 55432 -U postgres -d workspace -v ON_ERROR_STOP=1 | Out-Host
            if ($LASTEXITCODE -ne 0) { throw 'Local Project fixture SQL failed' }
        }
        $cleanupSql = @"
BEGIN;
DELETE FROM runs WHERE conversation_id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM messages WHERE conversation_id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM conversations WHERE id IN ('$fixtureConversation','$fixtureOwnerConversation');
DELETE FROM project_agents WHERE project_id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM project_members WHERE project_id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM projects WHERE id IN ('$fixtureProject','$permissionEditorProject','$permissionViewerProject','$permissionAgentlessProject','$permissionArchivedProject');
DELETE FROM conversations WHERE project_id=md5('ai-workspace-v2:legacy-project:' || '00000000-0000-4000-8000-000000000102')::uuid AND title='Legacy archive fixture';
UPDATE projects SET status='active',updated_at=now() WHERE id=md5('ai-workspace-v2:legacy-project:' || '00000000-0000-4000-8000-000000000102')::uuid;
COMMIT;
"@
        $seedSql = @"
BEGIN;
INSERT INTO projects(id,name,description,status,created_by_user_id)
VALUES ('$fixtureProject','Package B active run fixture','Runtime removal guard fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','Permission editor fixture','Action-aware permission fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','Permission viewer fixture','Viewer permission fixture','active','00000000-0000-4000-8000-000000000101'),
('$permissionAgentlessProject','Permission agentless fixture','Agent project context fixture','active','00000000-0000-4000-8000-000000000102'),
('$permissionArchivedProject','Permission archived fixture','Archived action permission fixture','archived','00000000-0000-4000-8000-000000000101');
INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES
('$fixtureProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$fixtureProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000102','viewer','00000000-0000-4000-8000-000000000101'),
('$permissionAgentlessProject','00000000-0000-4000-8000-000000000102','editor','00000000-0000-4000-8000-000000000102'),
('$permissionArchivedProject','00000000-0000-4000-8000-000000000101','owner','00000000-0000-4000-8000-000000000101');
INSERT INTO project_agents(project_id,agent_id,added_by_user_id)
VALUES ('$fixtureProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionEditorProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionViewerProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101'),
('$permissionArchivedProject','00000000-0000-4000-8000-000000000201','00000000-0000-4000-8000-000000000101');
INSERT INTO conversations(id,owner_id,agent_id,project_id,title,status)
VALUES ('$fixtureConversation','00000000-0000-4000-8000-000000000101','00000000-0000-4000-8000-000000000201','$fixtureProject','Package B editor guard conversation','active'),
('$fixtureOwnerConversation','00000000-0000-4000-8000-000000000101','00000000-0000-4000-8000-000000000201','$fixtureProject','Package B owner guard conversation','active');
INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status)
VALUES ('$fixtureMessage','$fixtureConversation','user','00000000-0000-4000-8000-000000000102','user','editor fixture trigger','completed'),
('$fixtureOwnerMessage','$fixtureOwnerConversation','user','00000000-0000-4000-8000-000000000101','user','owner fixture trigger','completed');
INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id)
VALUES ('$fixtureRun','$fixtureConversation','00000000-0000-4000-8000-000000000201','$fixtureMessage','00000000-0000-4000-8000-000000000001','queued','00000000-0000-4000-8000-000000000102'),
('$fixtureOwnerRun','$fixtureOwnerConversation','00000000-0000-4000-8000-000000000201','$fixtureOwnerMessage','00000000-0000-4000-8000-000000000001','cancelled','00000000-0000-4000-8000-000000000101');
INSERT INTO jobs(id,run_id,status) VALUES ('$fixtureJob','$fixtureRun','queued'),
('$fixtureOwnerJob','$fixtureOwnerRun','cancelled');
COMMIT;
"@
        $auditSql = @"
DO `$`$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.create' AND created_at > now()-interval '30 minutes') THEN RAISE EXCEPTION 'missing project.create audit'; END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.member.role_change' AND created_at > now()-interval '30 minutes') THEN RAISE EXCEPTION 'missing project.member.role_change audit'; END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.agent.add' AND created_at > now()-interval '30 minutes') THEN RAISE EXCEPTION 'missing project.agent.add audit'; END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='permission.denied' AND created_at > now()-interval '30 minutes') THEN RAISE EXCEPTION 'missing permission.denied audit'; END IF;
    IF NOT EXISTS (SELECT 1 FROM audit_events WHERE action='project.archive' AND created_at > now()-interval '30 minutes') THEN RAISE EXCEPTION 'missing project.archive audit'; END IF;
END `$`$;
"@
        try {
            Invoke-LocalProjectSql $cleanupSql
            Invoke-LocalProjectSql $seedSql
            $env:BASE_URL = 'http://127.0.0.1:8080'
            $env:GRPC_ADDR = '127.0.0.1:50051'
            $env:PERMISSION_OWNER = '00000000-0000-4000-8000-000000000101'
            $env:PERMISSION_EDITOR = '00000000-0000-4000-8000-000000000102'
            $env:PERMISSION_AGENT = '00000000-0000-4000-8000-000000000201'
            $env:PERMISSION_EDITOR_PROJECT = $permissionEditorProject
            $env:PERMISSION_VIEWER_PROJECT = $permissionViewerProject
            $env:PERMISSION_AGENTLESS_PROJECT = $permissionAgentlessProject
            $env:PERMISSION_ARCHIVED_PROJECT = $permissionArchivedProject
            $env:PERMISSION_FIXTURE_PROJECT = $fixtureProject
            $env:PERMISSION_OWNER_RUN = $fixtureOwnerRun
            $env:PERMISSION_EDITOR_RUN = $fixtureRun
            $go = Join-Path $root '.tools\go\bin\go.exe'
            if (!(Test-Path -LiteralPath $go)) { throw 'Local Go executable is missing.' }
            & $go run -buildvcs=false ./tests/permission_rpc
            if ($LASTEXITCODE -ne 0) { throw 'Permission RPC E2E failed' }

            $env:PROJECT_RUN_FIXTURE_PROJECT_ID = $fixtureProject
            $env:PROJECT_RUN_FIXTURE_USER_ID = '00000000-0000-4000-8000-000000000102'
            $env:PROJECT_RUN_FIXTURE_AGENT_ID = '00000000-0000-4000-8000-000000000201'
            $env:PROJECT_RUN_FIXTURE_RUN_ID = $fixtureRun
            $env:PROJECT_RUN_FIXTURE_CONVERSATION_ID = $fixtureConversation
            $env:PROJECT_TEST_REPORT = Join-Path $root 'build\project-api-report.json'
            & $python @pythonArgs (Join-Path $root 'tests\project_api.py')
            if ($LASTEXITCODE -ne 0) { throw 'Project API E2E failed' }
            $report = Get-Content -LiteralPath $env:PROJECT_TEST_REPORT -Raw | ConvertFrom-Json
            if ($report.status -ne 'PASS' -or !$report.active_run_removal_guard -or !$report.archive_active_run_guard -or !$report.archive_owner_only -or !$report.archived_readonly -or !$report.archive_audit) { throw 'Project API report did not confirm all guards' }
            Invoke-LocalProjectSql $auditSql
        }
        finally {
            Invoke-LocalProjectSql $cleanupSql
        }
    }
    finally {
        $workerExe = Join-Path $root 'build\workspace-worker.exe'
        if (!(Test-Path -LiteralPath $workerExe)) { throw 'Worker executable is missing after local E2E.' }
        $env:GRPC_ADDR = '127.0.0.1:50051'
        $env:WORKER_ID = 'local-worker'
        $env:MOCK_DELAY_MS = '250'
        $worker = Start-Process -FilePath $workerExe -WorkingDirectory $root -WindowStyle Hidden `
            -RedirectStandardOutput (Join-Path $root 'build\worker-e2e.log') `
            -RedirectStandardError (Join-Path $root 'build\worker-e2e.err') -PassThru
        $worker.Id | Set-Content -LiteralPath $workerPidFile
        Start-Sleep -Seconds 1
        if ($worker.HasExited) { throw 'Worker failed to restart after local E2E.' }
    }
}
finally { Pop-Location }
