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
    [string]$Generator = 'Ninja'
)

$ErrorActionPreference = 'Stop'

function Check-Command([string]$name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Error "$name not found in PATH. Please install it and re-run."
        exit 1
    }
}

Check-Command cmake
# Ninja is recommended; if another generator is provided, skip checking ninja
if ($Generator -eq 'Ninja') { Check-Command ninja }

$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Definition
$topDir = Resolve-Path (Join-Path $scriptPath '..')

function Build($cfg) {
    Write-Host "Configuring and building $cfg..."
    $buildDir = Join-Path $topDir "build\$cfg"
    & cmake -S $topDir -B $buildDir -G $Generator -DCMAKE_BUILD_TYPE=$cfg
    & cmake --build $buildDir --config $cfg
}

if ($Configuration -eq 'Release' -or $Configuration -eq 'Both') { Build 'Release' }
if ($Configuration -eq 'Debug' -or $Configuration -eq 'Both') { Build 'Debug' }

Write-Host "\nBuild finished. Binaries:"
Write-Host "  Release: $topDir\build\Release\TheLawOfPrey"
Write-Host "  Debug:   $topDir\build\Debug\TheLawOfPrey"
