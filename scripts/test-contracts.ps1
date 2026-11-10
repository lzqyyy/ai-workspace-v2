$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$pydeps = Join-Path $root '.tools\pydeps'
if (Test-Path -LiteralPath $pydeps) { $env:PYTHONPATH = $pydeps }
python (Join-Path $root 'tests\validate_contracts.py')
if ($LASTEXITCODE -ne 0) { throw 'Contract validation failed' }
New-Item -ItemType Directory -Force (Join-Path $root 'build\contracts') | Out-Null; $protoc = Join-Path $root '.tools\msys64\ucrt64\bin\protoc.exe'; if (!(Test-Path -LiteralPath $protoc)) { $protoc = 'protoc' }; $protoDir = Join-Path $root 'proto'; $protoFile = Join-Path $protoDir 'workspace.proto'; $descriptor = Join-Path $root 'build\contracts\workspace_contract.pb'; & $protoc -I $protoDir "--descriptor_set_out=$descriptor" --include_imports $protoFile
if ($LASTEXITCODE -ne 0) { throw 'Protobuf compilation failed' }
Write-Host 'Contract checks passed.'