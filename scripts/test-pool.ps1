param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
if (!$Docker) { throw 'This check is Docker-first only. Use: ./scripts/test-pool.ps1 -Docker' }
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

$poolSize = if ($env:DB_POOL_SIZE) { [int]$env:DB_POOL_SIZE } else { 8 }
$network = if ($env:E2E_NETWORK) { $env:E2E_NETWORK } else { 'ai-workspace-v2_internal' }
Push-Location $root
try {
    Invoke-Compose @('up', '-d', '--force-recreate', '--wait', 'core')
    & docker run --rm --network $network -v "${root}:/src" -w /src `
        -e BASE_URL=http://core:8080 python:3.12-slim python tests/database_pool.py
    if ($LASTEXITCODE -ne 0) { throw 'Database pool stress check failed' }

    $states = Query-Scalar "SELECT count(*) FILTER (WHERE state='idle'), count(*) FILTER (WHERE state='active'), count(*) FROM pg_stat_activity WHERE application_name='ai-workspace-core';"
    $parts = $states.Split('|')
    if ($parts.Count -ne 3) { throw "unexpected pool state output: $states" }
    $idle = [int]$parts[0]
    $active = [int]$parts[1]
    $total = [int]$parts[2]
    if ($total -lt 1) { throw 'pool retained no PostgreSQL connections' }
    if ($total -gt $poolSize) { throw "pool opened $total connections, max=$poolSize" }
    if ($idle -lt 1) { throw "pool returned no idle connections after stress: $states" }
    if ($active -ne 0) { throw "pool left active connections after stress: $states" }
    Write-Output ((@{status='PASS'; pool_size=$poolSize; idle=$idle; active=$active; total=$total} | ConvertTo-Json -Compress))
}
finally { Pop-Location }
