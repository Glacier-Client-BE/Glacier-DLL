@echo off
setlocal
echo [Glacier] Configuring...
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 ( echo [Glacier] CMake configure FAILED & pause & exit /b 1 )

echo [Glacier] Building...
cmake --build build --config Release --parallel
if %errorlevel% neq 0 ( echo [Glacier] Build FAILED & pause & exit /b 1 )

echo.
echo [Glacier] Done!
echo   glacier.dll  : build\Release\glacier.dll
echo   FA font      : embedded in DLL (no external file needed)
pause
