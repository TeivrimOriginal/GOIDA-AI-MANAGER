# GOIDA Architecture v2.1

## Монолит
`src/goida.cpp` ~2400 строк — единый Win32 процесс ( `WIN32` + `WIN32_LEAN_AND_MEAN` ), 7 страниц:
- Chat (RichEdit markdown)
- Trainer (prompt trainer)
- Profiles (миграция Femboy, keep_alive)
- Memory (лимит 100)
- Console (логи, авто-запуск)
- Models (ListView + Progress + Marketplace)
- About (personalization)

Вспомогательные:
- `sqlite3.c/.h` — WAL + FK + `PRAGMA user_version`
- `httplib.h` — header-only, демо `/api/embed`
- `utf8.h` — `w2utf8` / `utf8_to_w` + BOM strip
- `raii.h` — `WinHttpHandle`, `Stmt`, `FileHandle`
- `i18n.h` — `lang/ru.json` `lang/en.json` (UTF-8 BOM, 44 ключа)
- `db_migration.h` — миграции 1..3
- `json_helpers.h` — `goida::json::JEsc/JStr`, общие JSON-операции
- `ollama_client.h` — `goida::ollama::ParseUrl/ApiBase/Build*Url`, единая валидация endpoint Ollama
- `profiles.h` — `goida::profiles::Profile`, CRUD профилей, memory count, keep_alive L1/L2/L3
- `db.h` — `DBInit/DBExec/DBPrep/DBGet/DBSet/DBLog`, единый SQLite access layer
- `config.h` — `Config` и сохранение пользовательских настроек через `DBGet/DBSet`


## Навигация
Левая панель 198px (`COL_NAV`), кнопки `BS_OWNERDRAW`, `DrawBtn` (rounded `CreateRoundRectRgn` + `g_colAccent`), `WM_MOUSEMOVE` hover, `WM_TRAYICON` трей, `WM_SIZE` adaptive.

## База
`global/goida.db` — SQLite:
- `config(key,value)` — плоский KV
- `chat_messages`, `prompts`, `memory(tag,value)`, `logs(source,message)`, `femboy(key,value)` legacy
- v1: `profiles(name UNIQUE, avatar, system_prompt, temperature, max_tokens, keep_alive)` + индекс
- v2: `marketplace_cache`, `model_progress`
- v3: `chat_branches(parent_id FK, role, content)`
- `goida::db::migrate()` идемпотентна, `SELECT COUNT(*) FROM profiles` → `EnsureDefaultProfile()`

## Конфиг
`Config g_cfg` загружается `DBGet`/`DBSet`, хранит `api_url` (`/api/chat`), `model`, `temp`, `max_tokens`, `seed`, `sys_prompt`, `json_mode`, `lang`, `theme`, `accent`, `rounded`, `adaptive`, `f_*`, `update_url`, `active_profile`. `Save()` после каждого изменения UI.

## Хуки UI
- `WM_DRAWITEM` — rounded + accent
- `WM_SIZE` — если `adaptive`, ресайз контента `MoveWindow(newRight - x -16)`
- `WM_GETMINMAXINFO` 720x480
- `WM_TRAYICON` двойной клик → Show, правый клик → меню

## Потоки
WinHTTP (`winhttp.h`) `WinHttpOpen/Connect/OpenRequest/Send/Receive` с `WinHttpReadData` loop. URL для всех Ollama API проходит через `goida::ollama::ParseUrl`, endpoint строится через `ApiBase/Build*Url`. Каждое API — отдельный `CreateThread` + `PostMessage` (`WM_HTTP_CHUNK`, `WM_HTTP_DONE`, `WM_HTTP_ERR`, `WM_MODEL_PROGRESS`, `WM_CONN_RESULT`, `WM_MODEL_LIST`, `WM_MODEL_EMBED_DONE`).

## Отрисовка
`WM_PAINT` заливает `COL_BG`, рисует `COL_NAV` панель, separator `COL_BORDER`, title `Segoe UI` `g_fontTitle`, статус-точка `CONN_OK green / FAIL red`.

## Проверка
`cmake -S . -B build -G "MinGW Makefiles"` → `cmake --build build` → `ctest --test-dir build --output-on-failure`. Автотесты: `test_utf8`, `test_i18n`, `test_json`, `test_ollama_client`, `test_db`, `unit_json`.
