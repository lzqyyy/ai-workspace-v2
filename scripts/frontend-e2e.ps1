param([string]$BaseUrl = "http://localhost:8080", [string]$Screenshot = "docs/evidence/ui-project-first.png")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$env:BASE_URL = $BaseUrl
$env:FRONTEND_E2E_SCREENSHOT = Join-Path $root $Screenshot
node (Join-Path $root 'tests\frontend_e2e.js')
if ($LASTEXITCODE -ne 0) { throw 'Frontend browser E2E failed' }
