param([string]$BaseUrl = "http://localhost:8080", [string]$Screenshot = "docs/evidence/ui-2h2a.png")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$env:BASE_URL = $BaseUrl
$env:FRONTEND_2H2A_SCREENSHOT = Join-Path $root $Screenshot
node (Join-Path $root 'tests\frontend_2h2a_e2e.js')
if ($LASTEXITCODE -ne 0) { throw 'Frontend 2H+2A browser E2E failed' }
