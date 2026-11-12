<#
.SYNOPSIS
    Builds FontForgeGUI and headless libfontforge on Windows using MSYS2 (UCRT64).

.DESCRIPTION
    Configures and builds FontForgeGUI with CMake + Ninja + MinGW-w64 (UCRT64),
    optionally installs all required MSYS2 packages, runs the CTest suite,
    and can package a standalone dist-win folder with Qt6 & MinGW DLLs.

.EXAMPLE
    .\scripts\build_windows.ps1 -InstallDeps
    First-time setup: installs MSYS2 UCRT64 dependencies, builds, and runs tests.

.EXAMPLE
    .\scripts\build_windows.ps1 -Package
    Builds FontForgeGUI and creates a standalone dist-win\ bundle.
#>
[CmdletBinding()]
param(
    [string]$Msys2Root = $(if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }),
    [string]$BuildDir = "build-win",
    [string]$FontForgeDir = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "RelWithDebInfo",
    [switch]$InstallDeps,
    [switch]$SkipTests,
    [switch]$Package,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SrcDir = Join-Path $RepoRoot "src"
$FullBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $RepoRoot $BuildDir
}

$UcrtBin = Join-Path $Msys2Root "ucrt64\bin"
$MsysBash = Join-Path $Msys2Root "usr\bin\bash.exe"

if (-not (Test-Path $MsysBash)) {
    throw "MSYS2 not found at '$Msys2Root'. Install MSYS2 from https://www.msys2.org/ or pass -Msys2Root <path>."
}

function Invoke-Checked {
    param(
        [string]$Description,
        [scriptblock]$Command
    )
    Write-Host "==> $Description" -ForegroundColor Cyan
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

if ($InstallDeps) {
    $packages = @(
        "mingw-w64-ucrt-x86_64-toolchain"
        "mingw-w64-ucrt-x86_64-cmake"
        "mingw-w64-ucrt-x86_64-ninja"
        "mingw-w64-ucrt-x86_64-pkgconf"
        "mingw-w64-ucrt-x86_64-qt6-base"
        "mingw-w64-ucrt-x86_64-qt6-svg"
        "mingw-w64-ucrt-x86_64-freetype"
        "mingw-w64-ucrt-x86_64-libxml2"
        "mingw-w64-ucrt-x86_64-zlib"
        "mingw-w64-ucrt-x86_64-libiconv"
        "mingw-w64-ucrt-x86_64-libspiro"
        "mingw-w64-ucrt-x86_64-woff2"
        "mingw-w64-ucrt-x86_64-harfbuzz"
        "mingw-w64-ucrt-x86_64-libpng"
        "mingw-w64-ucrt-x86_64-libjpeg-turbo"
        "mingw-w64-ucrt-x86_64-libtiff"
        "mingw-w64-ucrt-x86_64-giflib"
        "mingw-w64-ucrt-x86_64-libunibreak"
    ) -join " "

    $env:MSYSTEM = "UCRT64"
    $env:CHERE_INVOKING = "1"
    Invoke-Checked "Installing MSYS2 UCRT64 dependencies via pacman" {
        & $MsysBash -lc "pacman -S --needed --noconfirm $packages"
    }
}

if (-not (Test-Path (Join-Path $UcrtBin "cmake.exe"))) {
    throw "UCRT64 CMake not found in '$UcrtBin'. Re-run with -InstallDeps."
}

# Prepend UCRT64 toolchain and Qt6 to PATH
$env:PATH = "$UcrtBin;$env:PATH"

if ($Clean -and (Test-Path $FullBuildDir)) {
    Write-Host "==> Removing build directory '$FullBuildDir'" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $FullBuildDir
}

$cmakeArgs = @(
    "-G", "Ninja"
    "-S", $SrcDir
    "-B", $FullBuildDir
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DCMAKE_C_COMPILER=$($UcrtBin -replace '\\', '/')/gcc.exe"
    "-DCMAKE_CXX_COMPILER=$($UcrtBin -replace '\\', '/')/g++.exe"
    "-DBUILD_TESTING=ON"
)

if (-not [string]::IsNullOrWhiteSpace($FontForgeDir)) {
    $resolvedFf = (Resolve-Path $FontForgeDir).Path -replace '\\', '/'
    $cmakeArgs += "-DFONTFORGE_SOURCE_DIR=$resolvedFf"
}

Invoke-Checked "Configuring CMake (Ninja / UCRT64)" {
    & cmake @cmakeArgs
}

Invoke-Checked "Building FontForgeGUI" {
    & cmake --build $FullBuildDir --parallel
}

# Ensure built libfontforge DLLs are discoverable on Windows PATH
$ffBinDir = Join-Path $FullBuildDir "fontforge-build\bin"
$ffLibDir = Join-Path $FullBuildDir "fontforge-build\lib"
$env:PATH = "$ffBinDir;$ffLibDir;$env:PATH"

if (-not $SkipTests) {
    $env:QT_QPA_PLATFORM = "offscreen"
    Invoke-Checked "Running CTest suite" {
        & ctest --test-dir $FullBuildDir --output-on-failure
    }
}

if ($Package) {
    $DistDir = Join-Path $RepoRoot "dist-win"
    if (Test-Path $DistDir) {
        Remove-Item -Recurse -Force $DistDir
    }
    New-Item -ItemType Directory -Path $DistDir | Out-Null

    $exePath = Join-Path $FullBuildDir "FontForgeGUI.exe"
    if (-not (Test-Path $exePath)) {
        throw "Expected executable not found at '$exePath'."
    }
    Copy-Item $exePath $DistDir

    foreach ($dllDir in @($ffBinDir, $ffLibDir)) {
        if (Test-Path $dllDir) {
            Get-ChildItem -Path $dllDir -Filter "*.dll" | Copy-Item -Destination $DistDir -Force
        }
    }

    $windeployqt = @(
        (Join-Path $UcrtBin "windeployqt6.exe"),
        (Join-Path $UcrtBin "windeployqt-qt6.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ($windeployqt) {
        Invoke-Checked "Deploying Qt6 runtime via $(Split-Path $windeployqt -Leaf)" {
            & $windeployqt --no-translations --no-opengl-sw (Join-Path $DistDir "FontForgeGUI.exe")
        }
    } else {
        Write-Warning "windeployqt6 not found in '$UcrtBin'; skipping automatic Qt plugin deployment."
    }

    # Copy remaining UCRT64 DLL dependencies (libstdc++, libgcc, freetype, libxml2, etc.)
    $distUnix = ($DistDir -replace '\\', '/') -replace '^([A-Za-z]):', { "/$($matches[1].ToLower())" }
    $ucrtUnix = ($UcrtBin -replace '\\', '/') -replace '^([A-Za-z]):', { "/$($matches[1].ToLower())" }
    $env:MSYSTEM = "UCRT64"
    Invoke-Checked "Collecting MSYS2 UCRT64 DLL dependencies into dist-win" {
        & $MsysBash -lc "ldd '$distUnix/FontForgeGUI.exe' '$distUnix/'*.dll 2>/dev/null | awk '/\/ucrt64\/bin\// {print \$3}' | sort -u | xargs -r -I{} cp -u '{}' '$distUnix/'"
    }

    Write-Host "==> Standalone Windows bundle created at: $DistDir" -ForegroundColor Green
}

Write-Host "==> Done! Run: `"$FullBuildDir\FontForgeGUI.exe`"" -ForegroundColor Green

