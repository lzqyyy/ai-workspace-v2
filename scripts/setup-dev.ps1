[CmdletBinding()]
param(
    [switch]$Build,
    [switch]$Up
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
$envFile = Join-Path $root '.env'
$composeFile = Join-Path $root 'deploy\docker-compose.yml'

function Invoke-Compose([string[]]$Arguments) {
    & docker compose --env-file $envFile -f $composeFile @Arguments
    if ($LASTEXITCODE -ne 0) { throw "docker compose failed: $($Arguments -join ' ')" }
}

Push-Location $root
try {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) {
        throw 'Docker CLI not found. Install Docker Desktop, then rerun ./scripts/setup-dev.ps1.'
    }
    & docker compose version | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'Docker Compose v2 is required.' }

    if (!(Test-Path -LiteralPath $envFile)) {
        Copy-Item -LiteralPath (Join-Path $root '.env.example') -Destination $envFile
        Write-Host 'Created .env from .env.example.'
    }

    Invoke-Compose @('config', '-q')

    if ($Build -or $Up) {
        Invoke-Compose @('build', 'core', 'worker')
    }
    if ($Up) {
        Invoke-Compose @('up', '-d')

        $healthy = $false
        for ($i = 0; $i -lt 60; $i++) {
            try {
                $health = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/health' -TimeoutSec 2
                if ($health.status -eq 'ok') { $healthy = $true; break }
            } catch {}
            Start-Sleep -Seconds 1
        }
        if (!$healthy) { throw 'Core did not become healthy. Inspect: docker compose --env-file .env -f deploy/docker-compose.yml logs core' }
    }

    Write-Host 'Docker-first development is ready.'
    Write-Host 'Start: ./scripts/setup-dev.ps1 -Up'
    Write-Host 'Full container checks: ./scripts/test.ps1 -Docker'
    Write-Host 'The optional no-Docker workflow still requires a prepared .tools/ directory.'
}
finally {
    Pop-Location
}
