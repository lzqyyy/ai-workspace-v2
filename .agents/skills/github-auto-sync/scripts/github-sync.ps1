[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Message,
    [string]$Branch = "main"
)

$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-Git([string[]]$GitArgs) {
    & git @GitArgs
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

$root = (& git rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or !$root) { throw "Not inside a Git repository." }
Set-Location $root

$currentBranch = (& git branch --show-current)
if ($LASTEXITCODE -ne 0 -or $currentBranch -ne $Branch) {
    throw "Auto-sync requires branch '$Branch'; current branch is '$currentBranch'."
}
if (!(& git remote get-url origin 2>$null)) { throw "Git remote 'origin' is not configured." }

Invoke-Git @("add", "-A")
$staged = @(& git diff --cached --name-only)
if ($LASTEXITCODE -ne 0) { throw "Unable to inspect staged files." }

if ($staged.Count -eq 0) {
    Invoke-Git @("fetch", "origin", $Branch)
    $ahead = [int]((& git rev-list --count "origin/$Branch..HEAD").Trim())
    if ($ahead -gt 0) {
        Invoke-Git @("push", "origin", "HEAD:$Branch")
        $hash = (& git rev-parse --short HEAD).Trim()
        Write-Output "SYNCED_NO_COMMIT commit=$hash branch=$Branch remote=origin"
    } else {
        Write-Output "NO_CHANGES branch=$Branch remote=origin"
    }
    exit 0
}

$forbiddenPaths = $staged | Where-Object {
    $_ -match '(^|/)\.env($|\.)' -or
    $_ -match '^\.tools/' -or
    $_ -match '^build/' -or
    $_ -match '(^|/)pgdata/' -or
    $_ -match '\.(pem|key|pfx|p12)$' -or
    $_ -match '(^|/)id_(rsa|ed25519)$'
}
if ($forbiddenPaths) {
    throw "Refusing to commit sensitive or generated paths: $($forbiddenPaths -join ', ')"
}

foreach ($file in $staged) {
    if (Test-Path -LiteralPath $file -PathType Leaf) {
        $size = (Get-Item -LiteralPath $file).Length
        if ($size -gt 10MB) { throw "Refusing to commit file larger than 10MB: $file" }
    }
}

& git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw "Staged diff failed whitespace/error checks." }

$secretPatterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    'gh[pousr]_[A-Za-z0-9]{20,}',
    'github_pat_[A-Za-z0-9_]{20,}',
    'sk-[A-Za-z0-9]{20,}',
    'AKIA[0-9A-Z]{16}'
)
$scan = & git diff --cached -- . ':(exclude).agents/skills/github-auto-sync/**'
if ($LASTEXITCODE -ne 0) { throw "Unable to scan staged diff for secrets." }
foreach ($pattern in $secretPatterns) {
    if ($scan | Select-String -Pattern $pattern -Quiet) {
        throw "Possible secret detected in staged diff; synchronization stopped."
    }
}

Invoke-Git @("commit", "-m", $Message)
Invoke-Git @("fetch", "origin", $Branch)
$behind = [int]((& git rev-list --count "HEAD..origin/$Branch").Trim())
if ($behind -gt 0) {
    Invoke-Git @("rebase", "origin/$Branch")
}
Invoke-Git @("push", "origin", "HEAD:$Branch")
$hash = (& git rev-parse --short HEAD).Trim()
Write-Output "SYNCED commit=$hash branch=$Branch remote=origin"