$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    & (Join-Path $PSScriptRoot 'test-contracts.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Contract checks failed' }
    $env:GOPATH = Join-Path $root '.tools\gopath'
    $env:GOTOOLCHAIN = 'local'
    & (Join-Path $root '.tools\go\bin\go.exe') test -buildvcs=false ./worker/...
    if ($LASTEXITCODE -ne 0) { throw 'Worker unit tests failed' }
    & python (Join-Path $root 'tests\e2e.py')
    if ($LASTEXITCODE -ne 0) { throw 'E2E failed' }
}
finally { Pop-Location }