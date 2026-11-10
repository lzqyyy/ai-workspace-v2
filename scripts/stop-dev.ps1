$root = Split-Path -Parent $PSScriptRoot
foreach ($name in @('worker','server')) {
    $file = Join-Path $root "build\$name.pid"
    if (Test-Path -LiteralPath $file) {
        $processId = [int](Get-Content -LiteralPath $file -Raw)
        Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $file -Force
    }
}
$pgctl = Join-Path $root '.tools\msys64\ucrt64\bin\pg_ctl.exe'
$pgdata = Join-Path $root 'build\pgdata'
if ((Test-Path -LiteralPath $pgctl) -and (Test-Path -LiteralPath $pgdata)) {
    $env:PATH = "$(Join-Path $root '.tools\msys64\ucrt64\bin');$env:PATH"
    & $pgctl -D $pgdata stop -m fast | Out-Null
}
Write-Host 'Local services stopped.'