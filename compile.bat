REM ===== Компиляция GOIDA AI MANAGER =====

REM Установка переменной PATH, чтобы убедиться, что g++ доступен
if not defined GCC_PATH set GCC_PATH=C:\MinGW\bin
path %GCC_PATH%;%path%

REM Проверка наличия g++
g++ --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] g++ не установлен. Пожалуйста, установите MinGW (https://www.mingw-w64.org/) или Visual Studio.
    pause
    exit /b 1
)

REM Очистка предыдущих файлов сборки
if exist "launcher.exe" del "launcher.exe"
if exist "client.exe" del "client.exe"
if exist "train_prompt.exe" del "train_prompt.exe"
if exist "goida-manager.exe" del "goida-manager.exe"
if exist "build.log" del "build.log"
if exist "build_errors.log" del "build_errors.log"

REM Проверка наличия исходных файлов
if not exist "src\launcher.cpp" (
    echo [ERROR] src\launcher.cpp не найден
    pause
    exit /b 1
)
if not exist "src\client.cpp" (
    echo [ERROR] src\client.cpp не найден
    pause
    exit /b 1
)
if not exist "src\train_prompt.cpp" (
    echo [ERROR] src\train_prompt.cpp не найден
    pause
    exit /b 1
)

echo [INFO] Проверка файлов завершена

REM --- Компиляция launcher.cpp ---
g++ -std=c++17 -O2 -mwindows -municode src\launcher.cpp -o launcher.exe 2>>build.log
if %errorlevel% neq 0 (
    echo [ERROR] Ошибка компиляции launcher.cpp
    type build.log
    pause
    exit /b 1
)
echo [OK] launcher.cpp успешно скомпилирован -> launcher.exe

REM --- Компиляция client.cpp ---
g++ -std=c++17 -O2 -mwindows -municode src\client.cpp -o client.exe 2>>build.log
if %errorlevel% neq 0 (
    echo [ERROR] Ошибка компиляции client.cpp
    type build.log
    pause
    exit /b 1
)
echo [OK] client.cpp успешно скомпилирован -> client.exe

REM --- Компиляция train_prompt.cpp ---
g++ -std=c++17 -O2 -mwindows -municode src\train_prompt.cpp -o train_prompt.exe 2>>build.log
if %errorlevel% neq 0 (
    echo [ERROR] Ошибка компиляции train_prompt.cpp
    type build.log
    pause
    exit /b 1
)
echo [OK] train_prompt.cpp успешно скомпилирован -> train_prompt.exe

REM --- Объединение всех файлов в один goida-manager.exe ---
g++ -std=c++17 -O2 -mwindows -municode src\launcher.cpp src\client.cpp src\train_prompt.cpp -o goida-manager.exe 2>>build.log
if %errorlevel% neq 0 (
    echo [ERROR] Ошибка объединения goida-manager.exe
    type build.log
    pause
    exit /b 1
)
echo [OK] goida-manager.exe успешно создан

echo [INFO] =========================================
echo [INFO] GOIDA AI MANAGER Сборка успешно завершена!
echo [INFO] =========================================
echo [INFO] Создано:
)echo [INFO]   launcher.exe         - Лаунчер с настройками фембойчика
echo [INFO]   client.exe           - Клиент с темной темой и памятью
echo [INFO]   train_prompt.exe       - Генератор промптов
echo [INFO]   goida-manager.exe    - Основное приложение, объединяющее все возможности
echo [INFO] =========================================
echo [INFO] Конфигурационные файлы находятся в папке 'global/':
echo [INFO]   global\femboy.ini    - Настройки фембойчика
)echo [INFO]   global\memory.dat     - Файл памяти
echo [INFO]   global\templates\    - Каталог шаблонов (пример: example.ini)
echo [INFO] =========================================
echo [INFO] Для запуска двукрайно goida-manager.exe
echo [INFO] =========================================
