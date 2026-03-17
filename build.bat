@echo off
setlocal
echo [Glacier] Configuring...
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 ( echo [Glacier] CMake configure FAILED & pause & exit /b 1 )

echo [Glacier] Building...
cmake --build build --config Release --parallel
if %errorlevel% neq 0 ( echo [Glacier] Build FAILED & pause & exit /b 1 )

echo.
echo [Glacier] Done!  Output: build\Release\glacier.dll
pause
