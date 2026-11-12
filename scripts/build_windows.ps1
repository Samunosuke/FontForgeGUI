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
    [switch]$SkipPackage,
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

function Deploy-RuntimeDependencies {
    param(
        [string]$TargetDir
    )

    foreach ($dllDir in @($ffBinDir, $ffLibDir)) {
        if (Test-Path $dllDir) {
            Get-ChildItem -Path $dllDir -Filter "*.dll" | ForEach-Object {
                $destFile = Join-Path $TargetDir $_.Name
                if (-not (Test-Path $destFile) -or ($_.LastWriteTimeUtc -gt (Get-Item $destFile).LastWriteTimeUtc)) {
                    try {
                        Copy-Item -Path $_.FullName -Destination $destFile -Force
                    } catch {
                        Write-Warning "Could not overwrite '$destFile' (file in use by running process); keeping existing copy."
                    }
                }
            }
        }
    }

    $windeployqt = @(
        (Join-Path $UcrtBin "windeployqt6.exe"),
        (Join-Path $UcrtBin "windeployqt-qt6.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    $targetExe = Join-Path $TargetDir "FontForgeGUI.exe"
    if ($windeployqt) {
        Invoke-Checked "Deploying Qt6 runtime to '$TargetDir' via $(Split-Path $windeployqt -Leaf)" {
            & $windeployqt --no-translations --no-opengl-sw $targetExe
        }
    } else {
        Write-Warning "windeployqt6 not found in '$UcrtBin'; skipping automatic Qt plugin deployment."
    }

    # Copy all transitive UCRT64 DLL dependencies (libstdc++, libgcc, libwinpthread, freetype, libxml2, etc.)
    # Uses objdump -p (static PE import table inspection) instead of ldd to avoid spawning processes or modal error dialogs.
    $objdump = Join-Path $UcrtBin "objdump.exe"
    if (-not (Test-Path $objdump)) {
        throw "objdump.exe not found in '$UcrtBin'."
    }

    Write-Host "==> Collecting MSYS2 UCRT64 DLL dependencies into '$TargetDir'" -ForegroundColor Cyan
    $processedFiles = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $pendingFiles = [System.Collections.Generic.Queue[string]]::new()

    Get-ChildItem -Path $TargetDir -File | Where-Object {
        ($_.Extension -eq ".exe" -or $_.Extension -eq ".dll") -and $_.Name -ne "dxcompiler.dll"
    } | ForEach-Object {
        if ($processedFiles.Add($_.FullName)) {
            $pendingFiles.Enqueue($_.FullName)
        }
    }

    while ($pendingFiles.Count -gt 0) {
        $batch = New-Object System.Collections.Generic.List[string]
        while ($pendingFiles.Count -gt 0 -and $batch.Count -lt 32) {
            $batch.Add($pendingFiles.Dequeue())
        }

        $matchesList = & $objdump -p @batch 2>$null | Select-String -Pattern '^\s*DLL Name:\s*(\S+\.dll)\s*$'
        foreach ($m in $matchesList) {
            $dllName = $m.Matches[0].Groups[1].Value
            $destDll = Join-Path $TargetDir $dllName
            $srcDll = Join-Path $UcrtBin $dllName
            if (-not (Test-Path $destDll) -and (Test-Path $srcDll)) {
                Copy-Item -Path $srcDll -Destination $destDll -Force
                if ($processedFiles.Add($destDll)) {
                    $pendingFiles.Enqueue($destDll)
                }
            }
        }
    }
}

# Deploy DLLs directly into build-win so FontForgeGUI.exe and tests run out-of-the-box
Deploy-RuntimeDependencies -TargetDir $FullBuildDir

if (-not $SkipTests) {
    $env:QT_QPA_PLATFORM = "offscreen"
    Invoke-Checked "Running CTest suite" {
        & ctest --test-dir $FullBuildDir --output-on-failure
    }
}

if (-not $SkipPackage) {
    # Extract project version from src/CMakeLists.txt
    $cmakeContent = Get-Content -Path (Join-Path $SrcDir "CMakeLists.txt") -Raw
    $version = if ($cmakeContent -match 'project\s*\(\s*FontForgeGUI\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $matches[1]
    } else {
        "0.0.0"
    }
    $dateStamp = Get-Date -Format "yyyy-MM-dd"
    $archiveBaseName = "FontForgeGUI-v$version-$dateStamp-win64"

    $DistRoot = Join-Path $RepoRoot "dist-win"
    $StageDir = Join-Path $DistRoot $archiveBaseName
    $ZipPath = Join-Path $DistRoot "$archiveBaseName.zip"

    if (Test-Path $StageDir) {
        Remove-Item -Recurse -Force $StageDir
    }
    if (Test-Path $ZipPath) {
        Remove-Item -Force $ZipPath
    }
    New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

    $exePath = Join-Path $FullBuildDir "FontForgeGUI.exe"
    if (-not (Test-Path $exePath)) {
        throw "Expected executable not found at '$exePath'."
    }

    Write-Host "==> Staging standalone runtime files in '$StageDir'" -ForegroundColor Cyan
    Copy-Item -Path $exePath -Destination $StageDir -Force
    Get-ChildItem -Path $FullBuildDir -Filter "*.dll" -File | Copy-Item -Destination $StageDir -Force

    $qtPluginDirs = @("generic", "iconengines", "imageformats", "networkinformation", "platforms", "styles", "tls")
    foreach ($pluginDir in $qtPluginDirs) {
        $srcPluginDir = Join-Path $FullBuildDir $pluginDir
        if (Test-Path $srcPluginDir) {
            Copy-Item -Path $srcPluginDir -Destination (Join-Path $StageDir $pluginDir) -Recurse -Force
        }
    }

    Write-Host "==> Creating ZIP package: $ZipPath" -ForegroundColor Cyan
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $StageDir,
        $ZipPath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $true
    )

    # Also copy the ZIP into the build directory for convenience
    $BuildZipPath = Join-Path $FullBuildDir "$archiveBaseName.zip"
    Copy-Item -Path $ZipPath -Destination $BuildZipPath -Force

    Write-Host "==> Standalone ZIP created:" -ForegroundColor Green
    Write-Host "    - $ZipPath" -ForegroundColor Green
    Write-Host "    - $BuildZipPath" -ForegroundColor Green
}

Write-Host "==> Done! Run: `"$FullBuildDir\FontForgeGUI.exe`"" -ForegroundColor Green

