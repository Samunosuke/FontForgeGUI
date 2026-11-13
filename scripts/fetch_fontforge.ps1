<#
.SYNOPSIS
    Clones or updates the upstream FontForge repository into ..\FontForge.

.DESCRIPTION
    Fetches https://github.com/fontforge/fontforge.git into the sibling FontForge
    directory expected by CMake's default FONTFORGE_SOURCE_DIR (..\FontForge).

.EXAMPLE
    .\scripts\fetch_fontforge.ps1
    Shallow-clones FontForge into ..\FontForge.

.EXAMPLE
    .\scripts\fetch_fontforge.ps1 -Update
    Pulls latest fast-forward changes if ..\FontForge already exists.
#>
[CmdletBinding()]
param(
    [string]$TargetDir = "",
    [string]$Branch = "",
    [switch]$Full,
    [switch]$Update
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($TargetDir)) {
    $TargetDir = Join-Path (Split-Path $RepoRoot -Parent) "FontForge"
}
$RepoUrl = "https://github.com/fontforge/fontforge.git"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git is required but was not found on PATH."
}

if (Test-Path $TargetDir) {
    if (-not (Test-Path (Join-Path $TargetDir ".git"))) {
        throw "'$TargetDir' already exists and is not a git repository."
    }
    if (-not $Update) {
        Write-Host "FontForge already exists at '$TargetDir'. Pass -Update to pull latest changes." -ForegroundColor Yellow
        exit 0
    }
    $dirty = & git -C $TargetDir status --porcelain
    if ($dirty) {
        throw "'$TargetDir' has uncommitted local changes; skipping pull."
    }
    Write-Host "==> Updating FontForge in '$TargetDir'" -ForegroundColor Cyan
    & git -C $TargetDir pull --ff-only
    if ($LASTEXITCODE -ne 0) {
        throw "git pull failed with exit code $LASTEXITCODE."
    }
    exit 0
}

$parentDir = Split-Path $TargetDir -Parent
if (-not (Test-Path $parentDir)) {
    New-Item -ItemType Directory -Path $parentDir | Out-Null
}

$cloneArgs = @("clone")
if (-not $Full) {
    $cloneArgs += @("--depth", "1")
}
if (-not [string]::IsNullOrWhiteSpace($Branch)) {
    $cloneArgs += @("--branch", $Branch)
}
$cloneArgs += @($RepoUrl, $TargetDir)

Write-Host "==> Cloning FontForge from $RepoUrl into '$TargetDir'" -ForegroundColor Cyan
& git @cloneArgs
if ($LASTEXITCODE -ne 0) {
    throw "git clone failed with exit code $LASTEXITCODE."
}

Write-Host "==> Done! FontForge is ready at '$TargetDir'" -ForegroundColor Green

