$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try { & python (Join-Path $root 'tests\e2e.py') }
finally { Pop-Location }
exit $LASTEXITCODE