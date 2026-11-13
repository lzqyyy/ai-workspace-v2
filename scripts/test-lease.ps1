$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$envFile = Join-Path $root '.env'
$composeFile = Join-Path $root 'deploy\docker-compose.yml'
$helper = Join-Path $root 'build\duplicate-callback-linux'
if (!(Test-Path -LiteralPath $helper)) { throw 'build\duplicate-callback-linux is missing; build the Linux E2E helper first' }

function Invoke-Compose {
    & docker compose --env-file $envFile -f $composeFile @args
    if ($LASTEXITCODE -ne 0) { throw "docker compose failed: $args" }
}

Push-Location $root
try {
    Invoke-Compose stop worker | Out-Host
    try {
        & docker run --rm --network ai-workspace-v2_internal -v "${root}:/src" -w /src `
            -e BASE_URL=http://core:8080 -e DUPLICATE_HELPER=/src/build/duplicate-callback-linux -e GRPC_ADDR=core:50051 `
            python:3.12-slim sh -lc 'chmod +x build/duplicate-callback-linux && python tests/lease_guard.py && python tests/cancel_lease_expiry.py'
        if ($LASTEXITCODE -ne 0) { throw 'lease guard test failed' }
    }
    finally {
        Invoke-Compose start worker | Out-Host
    }
}
finally {
    Pop-Location
}
