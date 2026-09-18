@echo off
setlocal EnableExtensions
REM Simple batch build helper (Windows). Powershell is recommended for more features.

:: Resolve repo top directory (one level up from this script)
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.." || (echo Failed to locate repo root & exit /b 1)
set TOP=%CD%

echo Checking for cmake...
where cmake >nul 2>&1 || (echo cmake not found in PATH. Please install cmake & exit /b 1)

echo Checking for ninja...
where ninja >nul 2>&1 || (echo ninja not found in PATH. You can install Ninja or change the generator in this script)

set "VCPKG_TOOLCHAIN="
set "VCPKG_EXE="

where vcpkg >nul 2>&1
if not errorlevel 1 (
  for /f "delims=" %%I in ('where vcpkg') do set "VCPKG_EXE=%%I"
)

if not defined VCPKG_EXE (
  where winget >nul 2>&1
  if not errorlevel 1 (
    echo vcpkg not found. Attempting to install vcpkg via winget...
    winget install --id Microsoft.Vcpkg --source winget -e
    if not errorlevel 1 (
      where vcpkg >nul 2>&1
      if not errorlevel 1 (
        for /f "delims=" %%I in ('where vcpkg') do set "VCPKG_EXE=%%I"
      )
    )
  )
)

if defined VCPKG_EXE (
  for %%I in ("%VCPKG_EXE%") do set "VCPKG_ROOT=%%~dpI"
  if exist "%VCPKG_ROOT%scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%scripts\buildsystems\vcpkg.cmake"
  )
)

call :configure_and_build Release || (echo Build failed for Release & exit /b 1)
call :configure_and_build Debug || (echo Build failed for Debug & exit /b 1)

echo.
echo Build finished. Binaries:
echo   Release: %TOP%\build\Release\TheLawOfPrey
echo   Debug:   %TOP%\build\Debug\TheLawOfPrey

popd
exit /b 0

:configure_and_build
set "CONFIG=%~1"
echo Configuring and building %CONFIG%...
set "BUILD_DIR=%TOP%\build\%CONFIG%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%" >nul 2>&1
cmake -S "%TOP%" -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_BUILD_TYPE=%CONFIG% %VCPKG_TOOLCHAIN% > "%TEMP%\law-of-prey-configure-%CONFIG%.log" 2>&1
if errorlevel 1 (
  findstr /C:"Could not find raylib" /C:"raylib.h" /C:"libraylib" "%TEMP%\law-of-prey-configure-%CONFIG%.log" >nul 2>&1
  if not errorlevel 1 (
    echo raylib was not found by CMake. Attempting to install it via vcpkg...
    if defined VCPKG_EXE (
      "%VCPKG_EXE%" install raylib:x64-windows --triplet x64-windows
      if errorlevel 1 (
        echo Automatic raylib installation failed. Please install raylib manually or set RAYLIB_DIR.
        exit /b 1
      )
      cmake -S "%TOP%" -B "%BUILD_DIR%" -G "Ninja" -DCMAKE_BUILD_TYPE=%CONFIG% %VCPKG_TOOLCHAIN% > "%TEMP%\law-of-prey-configure-%CONFIG%.log" 2>&1
      if errorlevel 1 (
        type "%TEMP%\law-of-prey-configure-%CONFIG%.log"
        exit /b 1
      )
    ) else (
      echo raylib could not be installed automatically because vcpkg is unavailable.
      exit /b 1
    )
  ) else (
    type "%TEMP%\law-of-prey-configure-%CONFIG%.log"
    exit /b 1
  )
)
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 exit /b 1
exit /b 0
