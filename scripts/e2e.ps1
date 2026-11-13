param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot

if ($Docker) {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker CLI not found.' }
    Push-Location $root
    try {
        & docker run --rm -v "${root}:/src" -v ai-workspace-v2-go-mod:/go/pkg/mod -w /src -e GOTOOLCHAIN=local golang:1.25-bookworm go build -buildvcs=false -o build/duplicate-callback-linux ./tests/duplicate_callback
        if ($LASTEXITCODE -ne 0) { throw 'Failed to build the Linux callback helper in Docker' }

        & docker run --rm --network ai-workspace-v2_internal -v "${root}:/src" -w /src `
            -e BASE_URL=http://core:8080 -e DUPLICATE_HELPER=/src/build/duplicate-callback-linux -e GRPC_ADDR=core:50051 `
            python:3.12-slim python tests/e2e.py
        if ($LASTEXITCODE -ne 0) { throw 'Docker E2E failed' }
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
}
finally { Pop-Location }
