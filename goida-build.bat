@echo off
SetLocal EnableDelayedExpansion
echo === GOIDA AI MANAGER Build Script v2.1 ===

REM CMake primary
where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] cmake found
    if not exist "build" mkdir build
    cmake -S . -B build -G "MinGW Makefiles"
    if !errorlevel! neq 0 goto :fallback
    cmake --build build
    if !errorlevel! neq 0 goto :fallback
    if exist "build\GOIDA.exe" copy /Y "build\GOIDA.exe" "GOIDA.exe" >nul
    echo [OK] GOIDA.exe via CMake
    ctest --test-dir build --output-on-failure
    goto :legacy
)

:fallback
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] g++ not found
    pause
    exit /b 1
)
echo [INFO] Fallback g++ direct
if exist "GOIDA.exe" del "GOIDA.exe"
g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src\goida.cpp src\sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32
if %errorlevel% neq 0 (
    echo [ERROR] g++ failed
    pause
    exit /b 1
)
echo [OK] GOIDA.exe via g++

:legacy
REM Legacy multi-exe build (optional, for backward compat)
if exist "src\launcher.cpp" (
    echo [INFO] Building legacy launcher/client/train...
    g++ -std=c++17 -O2 -mwindows -municode src\launcher.cpp -o launcher.exe 2>nul
    g++ -std=c++17 -O2 -mwindows -municode src\client.cpp -o client.exe 2>nul
    g++ -std=c++17 -O2 -mwindows -municode src\train_prompt.cpp -o train_prompt.exe 2>nul
    echo [OK] legacy exes built (if sources present)
)

if not exist "global" mkdir global
if not exist "global\templates" mkdir global\templates

echo [INFO] ========== Build finished ==========
echo [INFO] GOIDA.exe ready
echo [INFO] Use: GOIDA.exe
pause
exit /b 0
