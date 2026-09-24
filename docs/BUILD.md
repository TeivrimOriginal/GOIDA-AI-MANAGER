# Сборка GOIDA v2.1

## Требования
- Windows 10+
- MinGW-w64 GCC 14+ (ucrt, posix, seh) или MSVC с `/utf-8`
- CMake 3.20+

## CMake (рекомендуется)
```cmd
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
copy build\GOIDA.exe GOIDA.exe
```

`CMakeLists.txt`:
- `project(GOIDA 2.1 LANGUAGES CXX C)` `C++17` `C11`
- `add_executable(GOIDA WIN32 src/goida.cpp src/sqlite3.c)` `UNICODE _UNICODE _WIN32_WINNT=0x0600 NOMINMAX`
- `target_link_libraries comctl32 winhttp ole32 uuid urlmon ws2_32 wsock32`
- `-municode -mwindows -Wall` для MinGW
- `enable_testing()` + `test_utf8`, `test_i18n`, `test_db` (sqlite3.c), `test_json`, `unit_json`

## Bat fallback
`build.bat` / `build_goida.bat` / `goida-build.bat`:
1. `where cmake` → если найдено → `cmake -S . -B build` → `cmake --build build` → `ctest`
2. иначе `where g++` → `g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src/goida.cpp src/sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32`
3. Legacy `goida-build.bat` дополнительно собирает `launcher.exe`/`client.exe`/`train_prompt.exe` если есть `src/launcher.cpp` etc.

## Прямой g++
```cmd
g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src/goida.cpp src/sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32
```

## Тесты
- `tests/test_utf8.cpp` — кириллица roundtrip
- `tests/test_i18n.cpp` — `lang/ru.json` `lang/en.json` с BOM
- `tests/test_db.cpp` — `migrate` + `user_version` 3, таблицы `profiles`/`chat_branches`
- `tests/test_json.cpp` — `JEsc` / `JStr`

## Релиз
`release/`:
- `GOIDA.exe` — копия `build/GOIDA.exe`
- `start.bat` — `start "" "GOIDA.exe"`
- `global/goida.db` создаётся при первом запуске (WAL)

`GOIDA.exe` — GUI (`WIN32`), иконка `LoadIcon IDI_APPLICATION`, трей `Shell_NotifyIconW`, `AdjustWindowRect 840x580`.
