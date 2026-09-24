@echo off
SetLocal EnableDelayedExpansion
echo [INFO] GOIDA AI MANAGER - Сборка (CMake + fallback) начата

where cmake >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] cmake найден
    if not exist "build" mkdir build
    cmake -S . -B build -G "MinGW Makefiles"
    if !errorlevel! neq 0 (
        echo [WARN] cmake configure failed — fallback g++
        goto :fallback
    )
    cmake --build build
    if !errorlevel! neq 0 (
        echo [WARN] cmake build failed — fallback g++
        goto :fallback
    )
    if exist "build\GOIDA.exe" copy /Y "build\GOIDA.exe" "GOIDA.exe" >nul
    echo [OK] GOIDA.exe собран через CMake
    ctest --test-dir build --output-on-failure
    goto :success
)

:fallback
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] g++ не установлен. Установите MinGW.
    pause
    exit /b 1
)
echo [OK] g++ найден
if exist "GOIDA.exe" del "GOIDA.exe"
g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src\goida.cpp src\sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32
if %errorlevel% neq 0 (
    echo [ERROR] Ошибка компиляции GOIDA.exe
    pause
    exit /b 1
)
echo [OK] GOIDA.exe скомпилирован fallback -> GOIDA.exe

:success
echo [INFO] ========== Сборка успешно завершена ===========
echo [INFO] Сгенерированные файлы:
echo [INFO]   GOIDA.exe           - Основное приложение
echo [INFO] Конфиг: global/goida.db (SQLite, миграции v3)
echo [INFO] Языки: lang/ru.json, lang/en.json (UTF-8)
echo [INFO] Тесты: ctest --test-dir build
echo [INFO] Для запуска: GOIDA.exe
pause
exit /b 0
