@echo off
REM GOIDA AI MANAGER Build — CMake primary, g++ fallback
setlocal
echo [INFO] GOIDA build start...

REM Prefer CMake
where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] CMake found — configuring...
    if not exist "build" mkdir build
    cmake -S . -B build -G "MinGW Makefiles"
    if %errorlevel% neq 0 (
        echo [WARN] CMake configure failed, fallback to g++
        goto :fallback
    )
    cmake --build build --config Release
    if %errorlevel% neq 0 (
        echo [WARN] CMake build failed, fallback to g++
        goto :fallback
    )
    echo [OK] GOIDA.exe built via CMake -> build\GOIDA.exe
    if exist "build\GOIDA.exe" copy /Y "build\GOIDA.exe" "GOIDA.exe" >nul
    ctest --test-dir build --output-on-failure
    goto :end
)

:fallback
echo [INFO] Fallback: direct g++ compile
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] g++ not found. Install MinGW.
    exit /b 1
)
if exist "GOIDA.exe" del "GOIDA.exe"
g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src/goida.cpp src/sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32
if %errorlevel% equ 0 (
    echo [OK] GOIDA.exe built via g++
) else (
    echo [ERROR] g++ build failed
    exit /b 1
)

:end
echo [INFO] Done.
endlocal
