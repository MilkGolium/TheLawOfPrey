@echo off
REM Simple batch build helper (Windows). Powershell is recommended for more features.

:: Resolve repo top directory (one level up from this script)
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%.." || (echo Failed to locate repo root & exit /b 1)
set TOP=%CD%

echo Checking for cmake...
where cmake >nul 2>&1 || (echo cmake not found in PATH. Please install cmake & exit /b 1)

echo Checking for ninja...
where ninja >nul 2>&1 || (echo ninja not found in PATH. You can install Ninja or change the generator in this script)

echo Configuring and building Release...
cmake -S "%TOP%" -B "%TOP%\build\Release" -G "Ninja" -DCMAKE_BUILD_TYPE=Release || (echo CMake configure failed & popd & exit /b 1)
cmake --build "%TOP%\build\Release" --config Release || (echo Build failed & popd & exit /b 1)

echo Configuring and building Debug...
cmake -S "%TOP%" -B "%TOP%\build\Debug" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug || (echo CMake configure failed & popd & exit /b 1)
cmake --build "%TOP%\build\Debug" --config Debug || (echo Build failed & popd & exit /b 1)

echo.
echo Build finished. Binaries:
echo   Release: %TOP%\build\Release\TheLawOfPrey
echo   Debug:   %TOP%\build\Debug\TheLawOfPrey

popd
exit /b 0
