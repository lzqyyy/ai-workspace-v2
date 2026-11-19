param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
if (!$Docker) { throw 'This check is Docker-first only. Use: ./scripts/test-migrations.ps1 -Docker' }
if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker CLI not found.' }

$composeArgs = @('compose')
if ($env:COMPOSE_PROJECT_NAME) { $composeArgs += @('-p', $env:COMPOSE_PROJECT_NAME) }
$composeArgs += @('--env-file', (Join-Path $root '.env'), '-f', (Join-Path $root 'deploy\docker-compose.yml'))

function Invoke-Compose([string[]]$Arguments) {
    & docker @composeArgs @Arguments
    if ($LASTEXITCODE -ne 0) { throw "docker compose failed: $($Arguments -join ' ')" }
}

function Query-Scalar([string]$Sql) {
    $value = $Sql | & docker @composeArgs exec -T postgres sh -lc 'psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" -tA -v ON_ERROR_STOP=1'
    if ($LASTEXITCODE -ne 0) { throw "SQL check failed: $Sql" }
    return ($value | Out-String).Trim()
}

function Assert-Schema {
    $nullable = Query-Scalar "SELECT is_nullable FROM information_schema.columns WHERE table_name='conversations' AND column_name='project_id';"
    if ($nullable -ne 'NO') { throw "conversations.project_id must be NOT NULL, got $nullable" }
    $nullRows = Query-Scalar "SELECT count(*) FROM conversations WHERE project_id IS NULL;"
    if ($nullRows -ne '0') { throw "conversations.project_id contains $nullRows NULL rows" }
    $migrations = Query-Scalar "SELECT count(*) FROM schema_migrations;"
    $projectMigration = Query-Scalar "SELECT count(*) FROM schema_migrations WHERE version='007_project_id_required';"
    $initiatedNullable = Query-Scalar "SELECT is_nullable FROM information_schema.columns WHERE table_name='runs' AND column_name='initiated_by_user_id';"
    $initiatedNullRows = Query-Scalar "SELECT count(*) FROM runs WHERE initiated_by_user_id IS NULL;"
    $initiatedMigration = Query-Scalar "SELECT count(*) FROM schema_migrations WHERE version='008_run_initiated_by';"
    if ([int]$migrations -lt 8) { throw "expected at least 8 migrations, got $migrations" }
    if ($projectMigration -ne '1') { throw "007_project_id_required must appear exactly once, got $projectMigration" }
    if ($initiatedNullable -ne 'NO') { throw "runs.initiated_by_user_id must be NOT NULL, got $initiatedNullable" }
    if ($initiatedNullRows -ne '0') { throw "runs.initiated_by_user_id contains $initiatedNullRows NULL rows" }
    if ($initiatedMigration -ne '1') { throw "008_run_initiated_by must appear exactly once, got $initiatedMigration" }
    return [ordered]@{migrations=[int]$migrations; project_id_nullable=$nullable; null_rows=[int]$nullRows; migration_007=$projectMigration; initiated_by_nullable=$initiatedNullable; initiated_by_null_rows=[int]$initiatedNullRows; migration_008=$initiatedMigration}
}

Push-Location $root
try {
    $before = Assert-Schema
    Invoke-Compose @('restart', 'core')
    Invoke-Compose @('up', '-d', '--wait', 'core')
    $after = Assert-Schema
    if ($before.migrations -ne $after.migrations) { throw "migration count changed after restart: $($before.migrations) -> $($after.migrations)" }
    Write-Output (($before + @{restart_replay='PASS'; status='PASS'}) | ConvertTo-Json -Compress)
}
finally { Pop-Location }
