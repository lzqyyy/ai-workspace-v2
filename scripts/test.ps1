param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot

function Assert-Docker {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) {
        throw 'Docker CLI not found. Install Docker Desktop or provide a complete local toolchain.'
    }
}

function Invoke-Docker([string[]]$Arguments, [string]$Failure) {
    & docker @Arguments
    if ($LASTEXITCODE -ne 0) { throw $Failure }
}

if ($Docker) {
    Assert-Docker
    Push-Location $root
    try {
        & (Join-Path $PSScriptRoot 'test-contracts.ps1') -Docker
        if ($LASTEXITCODE -ne 0) { throw 'Contract checks failed' }

        Invoke-Docker @('run', '--rm', '-v', "${root}:/src", '-v', 'ai-workspace-v2-go-mod:/go/pkg/mod', '-w', '/src', '-e', 'GOTOOLCHAIN=local', '-e', 'GOFLAGS=-buildvcs=false', 'golang:1.25-bookworm', 'go', 'test', './worker/...') 'Worker unit tests failed in Docker'
        Invoke-Docker @('run', '--rm', '-v', "${root}:/src", '-v', 'ai-workspace-v2-go-mod:/go/pkg/mod', '-w', '/src', '-e', 'GOTOOLCHAIN=local', '-e', 'GOFLAGS=-buildvcs=false', 'golang:1.25-bookworm', 'go', 'vet', './worker/...') 'Worker go vet failed in Docker'
        Invoke-Docker @('run', '--rm', '-v', "${root}:/src", '-w', '/src', 'node:24-bookworm-slim', 'node', '--check', 'frontend/app.js') 'Frontend syntax check failed in Docker'
        Invoke-Docker @('run', '--rm', '-v', "${root}:/src", '-w', '/src', 'node:24-bookworm-slim', 'node', 'tests/frontend_state_test.js') 'Frontend state test failed in Docker'

        & (Join-Path $PSScriptRoot 'e2e.ps1') -Docker
        if ($LASTEXITCODE -ne 0) { throw 'E2E failed' }
        Write-Host 'All Docker-first checks passed.'
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
    throw 'Python 3 is required for E2E. Use Docker-first: ./scripts/test.ps1 -Docker.'
}

$go = $env:GO
if (!$go) {
    $candidate = Join-Path $root '.tools\go\bin\go.exe'
    if (Test-Path -LiteralPath $candidate) { $go = $candidate }
}
if (!$go) {
    $command = Get-Command go -ErrorAction SilentlyContinue
    if ($command) { $go = $command.Source }
}
if (!$go) {
    throw 'Go is required for Worker tests. Use Docker-first: ./scripts/test.ps1 -Docker.'
}

$node = $env:NODE
if (!$node) {
    $command = Get-Command node -ErrorAction SilentlyContinue
    if ($command) { $node = $command.Source }
}
if (!$node) {
    throw 'Node.js is required for frontend checks. Use Docker-first: ./scripts/test.ps1 -Docker.'
}

Push-Location $root
try {
    & (Join-Path $PSScriptRoot 'test-contracts.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Contract checks failed' }

    if ($go -like '*\.tools\go\*') {
        $env:GOPATH = Join-Path $root '.tools\gopath'
        $env:GOTOOLCHAIN = 'local'
    }
    & $go test -buildvcs=false ./worker/...
    if ($LASTEXITCODE -ne 0) { throw 'Worker unit tests failed' }

    & $node --check (Join-Path $root 'frontend\app.js')
    if ($LASTEXITCODE -ne 0) { throw 'Frontend syntax check failed' }
    & $node (Join-Path $root 'tests\frontend_state_test.js')
    if ($LASTEXITCODE -ne 0) { throw 'Frontend state test failed' }

    & $python @pythonArgs (Join-Path $root 'tests\e2e.py')
    if ($LASTEXITCODE -ne 0) { throw 'E2E failed' }
}
finally { Pop-Location }
