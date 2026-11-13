$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$envFile = Join-Path $root '.env'
$composeFile = Join-Path $root 'deploy\docker-compose.yml'
$output = Join-Path $root 'build\health-response.json'
New-Item -ItemType Directory -Force (Split-Path -Parent $output) | Out-Null

function Invoke-Compose {
    & docker compose --env-file $envFile -f $composeFile @args
    if ($LASTEXITCODE -ne 0) { throw "docker compose failed: $args" }
}
function Get-HealthStatus {
    $status = & curl.exe -sS -o $output -w '%{http_code}' 'http://127.0.0.1:8080/api/v1/health'
    if ($LASTEXITCODE -ne 0) { throw 'health endpoint request failed' }
    return [int]$status
}

Push-Location $root
try {
    if ((Get-HealthStatus) -ne 200) { throw 'health must return 200 while PostgreSQL is available' }
    try {
        Invoke-Compose stop postgres | Out-Host
        $status = 0
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Seconds 1
            $status = Get-HealthStatus
            if ($status -eq 503) { break }
        }
        if ($status -ne 503) { throw "health returned $status while PostgreSQL was unavailable" }
        $body = Get-Content -Raw $output | ConvertFrom-Json
        if ($body.status -ne 'degraded' -or $body.database -ne 'unavailable') { throw "unexpected unhealthy body: $body" }
        Write-Host 'health outage check: PASS (503 degraded/unavailable)'
    }
    finally {
        Invoke-Compose start postgres | Out-Host
        $status = 0
        for ($i = 0; $i -lt 30; $i++) {
            Start-Sleep -Seconds 1
            $status = Get-HealthStatus
            if ($status -eq 200) { break }
        }
        if ($status -ne 200) { throw "health did not recover after PostgreSQL restart (last status $status)" }
        Write-Host 'health recovery check: PASS (200)'
    }
}
finally {
    Pop-Location
}
