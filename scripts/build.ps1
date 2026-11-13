$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$msys = Join-Path $root '.tools\msys64\usr\bin\bash.exe'
$go = Join-Path $root '.tools\go\bin\go.exe'
if (!(Test-Path -LiteralPath $msys)) { throw 'Local MSYS2 toolchain missing under .tools/. Use Docker-first: ./scripts/setup-dev.ps1 -Up; or prepare the no-Docker toolchain described in docs/runbook/A14-operations.md.' }
if (!(Test-Path -LiteralPath $go)) { throw 'Local Go toolchain missing under .tools/go. Use Docker-first: ./scripts/setup-dev.ps1 -Up.' }
Push-Location $root
try {
    $env:GOPATH = Join-Path $root '.tools\gopath'
    $env:GOTOOLCHAIN = 'local'
    $rootUnix = $root.Replace('\', '/')
    if ($rootUnix -match '^([A-Za-z]):') { $rootUnix = '/' + $matches[1].ToLower() + $rootUnix.Substring(2) }
    $command = "cd '$rootUnix'; export PATH=/ucrt64/bin:/usr/bin:`$PATH; export PKG_CONFIG_PATH=/ucrt64/lib/pkgconfig; cmake -S . -B build/server -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDrogon_DIR=/ucrt64/lib/cmake/Drogon-shared -DTrantor_DIR=/ucrt64/lib/cmake/Trantor-shared; cmake --build build/server -j 2"
    & $msys -lc $command
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
    New-Item -ItemType Directory -Force (Join-Path $root 'build') | Out-Null
    & $go test -buildvcs=false ./worker/...
    if ($LASTEXITCODE -ne 0) { throw 'Go tests failed' }
    & $go build -buildvcs=false -o (Join-Path $root 'build\workspace-worker.exe') ./worker/cmd/worker
    if ($LASTEXITCODE -ne 0) { throw 'Worker build failed' }
    & $go build -buildvcs=false -o (Join-Path $root 'build\duplicate-callback.exe') ./tests/duplicate_callback
    if ($LASTEXITCODE -ne 0) { throw 'E2E helper build failed' }
    Write-Host 'Build complete: build/server/workspace_server.exe, build/workspace-worker.exe'
}
finally { Pop-Location }
