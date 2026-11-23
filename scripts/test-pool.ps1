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

function Get-PoolState {
    $states = Query-Scalar "SELECT count(*) FILTER (WHERE state='idle'), count(*) FILTER (WHERE state='active'), count(*) FROM pg_stat_activity WHERE application_name='ai-workspace-core';"
    $parts = $states.Split('|')
    if ($parts.Count -ne 3) { throw "unexpected pool state output: $states" }
    return [ordered]@{idle=[int]$parts[0]; active=[int]$parts[1]; total=[int]$parts[2]}
}

function Assert-PoolState([string]$Label) {
    $state = Get-PoolState
    if ($state.total -lt 1) { throw "${Label}: pool retained no PostgreSQL connections" }
    if ($state.total -gt $poolSize) { throw "${Label}: pool opened $($state.total) connections, max=$poolSize" }
    if ($state.idle -lt 1) { throw "${Label}: pool returned no idle connections" }
    if ($state.active -ne 0) { throw "${Label}: pool left active connections" }
    return $state
}

function Invoke-Stress {
    & docker run --rm --network $network -v "${root}:/src" -w /src `
        -e BASE_URL=http://core:8080 python:3.12-slim python tests/database_pool.py
    if ($LASTEXITCODE -ne 0) { throw 'Database pool stress check failed' }
}

$poolSize = if ($env:DB_POOL_SIZE) { [int]$env:DB_POOL_SIZE } else { 8 }
$network = if ($env:E2E_NETWORK) { $env:E2E_NETWORK } else { 'ai-workspace-v2_internal' }
$previousIdleValidation = $env:DB_IDLE_VALIDATE_SECONDS
Push-Location $root
try {
    # Force checkout-time validation so stale idle connections must be discarded.
    $env:DB_IDLE_VALIDATE_SECONDS = '0'
    Invoke-Compose @('up', '-d', '--force-recreate', '--wait', 'core')
    Invoke-Stress
    $first = Assert-PoolState 'initial stress'
    Write-Output (($first + @{status='PASS'; pool_size=$poolSize}) | ConvertTo-Json -Compress)

    Invoke-Compose @('stop', 'postgres')
    Invoke-Compose @('up', '-d', '--wait', 'postgres')
    Invoke-Stress
    $recovered = Assert-PoolState 'recovery stress'
    Write-Output (($recovered + @{status='PASS'; recovery='PASS'; pool_size=$poolSize}) | ConvertTo-Json -Compress)

    $connectWatch = [Diagnostics.Stopwatch]::StartNew()
    & docker run --rm --network $network `
        -e DATABASE_URL='host=203.0.113.1 port=5432 dbname=workspace user=workspace password=workspace' `
        -e DB_CONNECT_TIMEOUT_SECONDS=1 -e HTTP_PORT=18080 -e GRPC_PORT=150051 `
        ai-workspace-v2-core:latest *> $null
    $connectExit = $LASTEXITCODE
    $connectWatch.Stop()
    if ($connectExit -eq 0) { throw 'unreachable database unexpectedly allowed Core startup' }
    if ($connectWatch.ElapsedMilliseconds -gt 8000) {
        throw "database connect timeout exceeded budget: $($connectWatch.ElapsedMilliseconds)ms"
    }
    Write-Output (ConvertTo-Json -Compress @{status='PASS'; connect_timeout='PASS'; elapsed_ms=[int]$connectWatch.ElapsedMilliseconds})
}
finally {
    if ($null -eq $previousIdleValidation) {
        Remove-Item Env:DB_IDLE_VALIDATE_SECONDS -ErrorAction SilentlyContinue
    } else {
        $env:DB_IDLE_VALIDATE_SECONDS = $previousIdleValidation
    }
    try { Invoke-Compose @('up', '-d', '--force-recreate', '--wait', 'core') } catch { Write-Warning $_ }
    Pop-Location
}
