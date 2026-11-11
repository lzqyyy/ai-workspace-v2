param([switch]$NoBrowser)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root '.tools\msys64\ucrt64\bin'
$pgdata = Join-Path $root 'build\pgdata'
$serverExe = Join-Path $root 'build\server\workspace_server.exe'
$workerExe = Join-Path $root 'build\workspace-worker.exe'

function Test-Port([int]$Port) {
    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $task = $client.ConnectAsync('127.0.0.1', $Port)
        if (!$task.Wait(500)) { return $false }
        return $client.Connected
    } catch { return $false }
    finally { $client.Dispose() }
}

if (!(Test-Path -LiteralPath $serverExe) -or !(Test-Path -LiteralPath $workerExe)) {
    Write-Host 'Build artifacts missing; running build.'
    & (Join-Path $PSScriptRoot 'build.ps1')
}
$env:PATH = "$bin;$env:PATH"
New-Item -ItemType Directory -Force (Join-Path $root 'build') | Out-Null
if (!(Test-Path -LiteralPath (Join-Path $pgdata 'PG_VERSION'))) {
    Write-Host 'Initializing local PostgreSQL data directory.'
    & (Join-Path $bin 'initdb.exe') -D $pgdata -U postgres --auth=trust --encoding=UTF8 --no-locale | Out-Null
}
if (!(Test-Port 55432)) {
    Write-Host 'Starting PostgreSQL on 127.0.0.1:55432.'
    & (Join-Path $bin 'pg_ctl.exe') -D $pgdata -o "-p 55432 -h 127.0.0.1" -l (Join-Path $root 'build\postgres.log') start
    Start-Sleep -Seconds 2
}
if (!(Test-Port 55432)) { throw 'PostgreSQL did not start on port 55432.' }
$exists = & (Join-Path $bin 'psql.exe') -h 127.0.0.1 -p 55432 -U postgres -d postgres -tAc "SELECT 1 FROM pg_database WHERE datname='workspace'"
if ($exists -ne '1') { & (Join-Path $bin 'createdb.exe') -h 127.0.0.1 -p 55432 -U postgres workspace }
$env:DATABASE_URL = 'host=127.0.0.1 port=55432 dbname=workspace user=postgres'
$env:HTTP_HOST = '127.0.0.1'; $env:HTTP_PORT = '8080'; $env:GRPC_HOST = '127.0.0.1'; $env:GRPC_PORT = '50051'
$env:FRONTEND_DIR = Join-Path $root 'frontend'; $env:MIGRATIONS_DIR = Join-Path $root 'db\migrations'
$env:APP_ORIGIN = 'http://localhost:8080,http://127.0.0.1:8080'; $env:COOKIE_SECURE = 'false'
if (!(Test-Port 8080)) {
    Write-Host 'Starting C++ Core on http://127.0.0.1:8080.'
    $server = Start-Process -FilePath $serverExe -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root 'build\server.log') -RedirectStandardError (Join-Path $root 'build\server.err') -PassThru
    $server.Id | Set-Content (Join-Path $root 'build\server.pid')
}
for ($i = 0; $i -lt 30; $i++) {
    try { $health = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/health' -TimeoutSec 2; if ($health.status -eq 'ok') { break } } catch {}
    Start-Sleep -Seconds 1
}
if ($health.status -ne 'ok') { & (Join-Path $PSScriptRoot 'stop-dev.ps1'); throw 'Core did not become healthy. See build/server.log and build/server.err.' }
$env:GRPC_ADDR = '127.0.0.1:50051'; $env:WORKER_ID = 'local-worker'; $env:MOCK_DELAY_MS = '250'
Write-Host 'Starting Go Worker.'
$worker = Start-Process -FilePath $workerExe -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput (Join-Path $root 'build\worker.log') -RedirectStandardError (Join-Path $root 'build\worker.err') -PassThru
$worker.Id | Set-Content (Join-Path $root 'build\worker.pid')
Start-Sleep -Seconds 1
Write-Host 'Workspace running at http://127.0.0.1:8080 (demo / demo1234)'
Write-Host 'Stop with ./scripts/stop-dev.ps1'
if (!$NoBrowser) { Start-Process 'http://127.0.0.1:8080' }