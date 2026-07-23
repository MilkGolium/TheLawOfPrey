<#
Simple PowerShell build helper for Windows.
Usage examples:
  ./scripts/build.ps1            # build both Release and Debug (default)
  ./scripts/build.ps1 -Configuration Release
  ./scripts/build.ps1 -Configuration Debug
  ./scripts/build.ps1 -Configuration Both -Generator "NMake Makefiles"

Requires: cmake, ninja (or another CMake generator you pass via -Generator), and raylib installed or RAYLIB_DIR pointing to installation.
#>
param(
    [ValidateSet('Release','Debug','Both')]
    [string]$Configuration = 'Both',
    [string]$Generator = 'Ninja',
    [switch]$AutoInstallRaylib = $true
)

$ErrorActionPreference = 'Stop'

function Check-Command([string]$name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Error "$name not found in PATH. Please install it and re-run."
        exit 1
    }
}

function Get-CommandPath([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    return $null
}

function Test-RaylibConfigured([string]$buildDir, [string]$cfg) {
    $cmakeArgs = @('-S', $topDir, '-B', $buildDir, '-G', $Generator, '-DCMAKE_BUILD_TYPE=' + $cfg)
    if ($script:VcpkgToolchainFile) {
        $cmakeArgs += @('-DCMAKE_TOOLCHAIN_FILE=' + $script:VcpkgToolchainFile)
    }
    if ($script:VcpkgTriplet) {
        $cmakeArgs += @('-DVCPKG_TARGET_TRIPLET=' + $script:VcpkgTriplet)
    }

    $configureOutput = & cmake @cmakeArgs 2>&1 | Tee-Object -Variable configureLog
    $exitCode = $LASTEXITCODE
    $text = ($configureLog | Out-String)

    if ($exitCode -ne 0) {
        if ($text -match 'Could not find raylib|raylib\.h|libraylib') {
            return $false
        }
        throw "CMake configure failed for $cfg (exit code $exitCode).`n$text"
    }

    return $true
}

function Ensure-Raylib() {
    if ($env:RAYLIB_DIR) {
        Write-Host "Using RAYLIB_DIR=$env:RAYLIB_DIR"
        return $true
    }

    if (-not $AutoInstallRaylib) {
        Write-Host "raylib was not found and auto-install is disabled."
        return $false
    }

    $vcpkgExe = Get-CommandPath 'vcpkg'
    if (-not $vcpkgExe) {
        $wingetExe = Get-CommandPath 'winget'
        if ($wingetExe) {
            Write-Host 'vcpkg not found. Attempting to install vcpkg via winget...'
            & $wingetExe install --id Microsoft.Vcpkg --source winget -e
            if ($LASTEXITCODE -ne 0) {
                Write-Host 'Could not install vcpkg automatically. Please install it manually or set RAYLIB_DIR.'
                return $false
            }
            $vcpkgExe = Get-CommandPath 'vcpkg'
        }
    }

    if (-not $vcpkgExe) {
        Write-Host 'Neither vcpkg nor winget was available. Please install vcpkg or raylib manually.'
        return $false
    }

    $script:VcpkgToolchainFile = $null
    $script:VcpkgTriplet = $null
    $vcpkgDir = Split-Path -Parent $vcpkgExe
    $toolchainFile = Join-Path $vcpkgDir 'scripts/buildsystems/vcpkg.cmake'
    if (Test-Path $toolchainFile) {
        $script:VcpkgToolchainFile = $toolchainFile
        $script:VcpkgTriplet = 'x64-windows'
    }

    Write-Host 'Installing raylib with vcpkg...'
    & $vcpkgExe install raylib:x64-windows --triplet x64-windows
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Automatic raylib installation failed. Please install raylib manually or set RAYLIB_DIR.'
        return $false
    }

    return $true
}

Check-Command cmake
# Ninja is recommended; if another generator is provided, skip checking ninja
if ($Generator -eq 'Ninja') { Check-Command ninja }

$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Definition
$topDir = (Resolve-Path (Join-Path $scriptPath '..')).Path

function Build($cfg) {
    Write-Host "Configuring and building $cfg..."
    $buildDir = Join-Path $topDir "build\$cfg"

    if (-not (Test-RaylibConfigured -buildDir $buildDir -cfg $cfg)) {
        Write-Host "raylib was not found by CMake. Attempting to install it automatically..."
        if (-not (Ensure-Raylib)) {
            throw "Could not configure $cfg because raylib is not available. Install raylib manually or set RAYLIB_DIR."
        }
        if (-not (Test-RaylibConfigured -buildDir $buildDir -cfg $cfg)) {
            throw "CMake still could not find raylib after the install attempt."
        }
    }

    & cmake --build $buildDir --config $cfg
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $cfg."
    }
}

if ($Configuration -eq 'Release' -or $Configuration -eq 'Both') { Build 'Release' }
if ($Configuration -eq 'Debug' -or $Configuration -eq 'Both') { Build 'Debug' }

Write-Host "`nBuild finished. Binaries:"
Write-Host "  Release: $topDir\build\Release\TheLawOfPrey"
Write-Host "  Debug:   $topDir\build\Debug\TheLawOfPrey"
