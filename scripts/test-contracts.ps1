param([switch]$Docker)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot

function Assert-Docker {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) {
        throw 'Docker CLI not found. Install Docker Desktop or provide Python 3 with PyYAML and protoc on PATH.'
    }
}

function Invoke-DockerContractChecks {
    Assert-Docker
    $image = 'ai-workspace-v2-contract-tools:local'
    & docker build --tag $image --file (Join-Path $root 'deploy\contract-tools.Dockerfile') (Join-Path $root 'deploy')
    if ($LASTEXITCODE -ne 0) { throw 'Failed to build the cached contract-tools image' }

    $script = 'set -eu; python3 tests/validate_contracts.py; mkdir -p build/contracts; protoc -I proto --descriptor_set_out=build/contracts/workspace_contract.pb --include_imports proto/workspace.proto'
    & docker run --rm -v "${root}:/src" -w /src $image bash -lc $script
    if ($LASTEXITCODE -ne 0) { throw 'Docker contract validation failed' }
    Write-Host 'Contract checks passed in Docker.'
}

if ($Docker) {
    Push-Location $root
    try { Invoke-DockerContractChecks }
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
    throw 'Python 3 is required for contract validation. Use Docker-first: ./scripts/test-contracts.ps1 -Docker (or ./scripts/setup-dev.ps1).'
}

$protoc = $env:PROTOC
if (!$protoc) {
    $candidate = Join-Path $root '.tools\msys64\ucrt64\bin\protoc.exe'
    if (Test-Path -LiteralPath $candidate) { $protoc = $candidate }
}
if (!$protoc) {
    $command = Get-Command protoc -ErrorAction SilentlyContinue
    if ($command) { $protoc = $command.Source }
}
if (!$protoc) {
    throw 'protoc is required for descriptor compilation. Use Docker-first: ./scripts/test-contracts.ps1 -Docker (or ./scripts/setup-dev.ps1).'
}

$pydeps = Join-Path $root '.tools\pydeps'
if (Test-Path -LiteralPath $pydeps) { $env:PYTHONPATH = $pydeps }
& $python @pythonArgs -c 'import yaml' 2>$null
if ($LASTEXITCODE -ne 0) {
    throw 'Python PyYAML is missing. Install it, or use Docker-first: ./scripts/test-contracts.ps1 -Docker.'
}

Push-Location $root
try {
    & $python @pythonArgs 'tests\validate_contracts.py'
    if ($LASTEXITCODE -ne 0) { throw 'Contract validation failed' }

    New-Item -ItemType Directory -Force (Join-Path $root 'build\contracts') | Out-Null
    $protoDir = Join-Path $root 'proto'
    $protoFile = Join-Path $protoDir 'workspace.proto'
    $descriptor = Join-Path $root 'build\contracts\workspace_contract.pb'
    & $protoc -I $protoDir "--descriptor_set_out=$descriptor" --include_imports $protoFile
    if ($LASTEXITCODE -ne 0) { throw 'Protobuf compilation failed' }
    Write-Host 'Contract checks passed.'
}
finally { Pop-Location }
