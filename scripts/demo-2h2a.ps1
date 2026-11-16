param([switch]$Docker, [string]$Report = "build/demo-2h2a-report.json")
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
$reportPath = Join-Path $root $Report
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $reportPath) | Out-Null

if ($Docker) {
    if (!(Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Docker CLI not found.' }
    & docker run --rm --network ai-workspace-v2_internal -v "${root}:/src" -w /src `
        -e BASE_URL=http://core:8080 -e DEMO_REPORT=/src/build/demo-2h2a-report.json `
        python:3.12-slim python tests/demo_2h2a.py
    if ($LASTEXITCODE -ne 0) { throw '2H+2A demo seed failed in Docker' }
    return
}

$python = Get-Command py -ErrorAction SilentlyContinue
if ($python) { & $python.Source -3 (Join-Path $root 'tests\demo_2h2a.py') }
else {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (!$python) { throw 'Python 3 is required. Use Docker-first: ./scripts/demo-2h2a.ps1 -Docker' }
    & $python.Source (Join-Path $root 'tests\demo_2h2a.py')
}
if ($LASTEXITCODE -ne 0) { throw '2H+2A demo seed failed' }
