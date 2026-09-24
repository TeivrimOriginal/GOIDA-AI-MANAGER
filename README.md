# GOIDA AI MANAGER — v2.1

Русское AI-приложение для Ollama: профили, память, чат с markdown, модели, консоль. Один исполняемый файл, SQLite, темная тема.

## Точка Б (реализовано)

- **Launcher** — единый вход, трей, автозапуск
- **Models (Marketplace+Local)** — ListView (имя/размер/дата/семейство), прогресс-бар `PBM_SETPOS` для `pull` (парсинг `total/completed` → `WM_MODEL_PROGRESS`), Marketplace (8 популярных: `qwen:14b`, `llama3:8b`, `mistral:7b`, `gemma2:9b`, `phi3:mini`, `codellama:13b`, `nomic-embed-text`, `llava:13b` → Install)
- **Download** — `POST /api/pull` stream=true, статус + проценты
- **Create Profile** — Profiles (миграция Femboy→Profiles), лимит памяти 100, `keep_alive` L1=5m / L2=30m / L3=1h
- **Profiles / Profile Detail/Edit** — ListBox профилей, поля Name/Avatar/System/temp/tokens/keep_alive (Combo), Create/Save/Delete/Load, `active_profile` в `config`
- **Chat** — `POST /api/chat` (по умолчанию `http://localhost:11434/api/chat`), RichEdit50W markdown (заголовки `#`, списки `-`, цитаты `>`, ``code``, **bold**), Regenerate (сброс последнего assistant и resend), Branch (снапшот `g_history` → `chat_branches`), keep_alive из активного профиля, `images` (base64) и `tools` (function calling) в JSON
- **Settings** — в About: Theme (dark/amoled/light), Accent (blue/purple/green/orange/pink → `g_colAccent`), Round (CreateRoundRectRgn), Adaptive (WM_SIZE ресайз контента), Lang (ru/en → `lang/*.json`)
- **Updates** — `URLDownloadToFileW` + bat-свап

## Структура

```
GOIDA-AI-MANAGER/
├── src/
│   ├── goida.cpp            # ~2400 строк, 7 страниц (Chat/Trainer/Profiles/Memory/Console/Models/About)
│   ├── sqlite3.c/.h         # SQLite
│   ├── httplib.h            # cpp-httplib 0.58 (header-only) для embed/demo
│   ├── utf8.h               # w2utf8 / utf8_to_w, BOM strip, file I/O
│   ├── raii.h               # Handle, WinHttpHandle, Stmt, GdiHandle
│   ├── i18n.h               # I18n loader (ru/en JSON, UTF-8, fallback)
│   └── db_migration.h       # PRAGMA user_version, migrate 1..3 (profiles, marketplace_cache, chat_branches)
├── lang/
│   ├── ru.json              # 44 ключа, UTF-8 BOM
│   └── en.json              # 44 ключа
├── tests/
│   ├── test_utf8.cpp        # roundtrip Cyrillic
│   ├── test_i18n.cpp        # load ru/en, fallback
│   ├── test_db.cpp          # migrate + user_version
│   └── test_json.cpp        # JEsc/JStr
├── docs/
│   ├── ARCHITECTURE.md
│   ├── API.md
│   └── BUILD.md
├── .github/workflows/build.yml  # Actions: CMake + MinGW + ctest
├── CMakeLists.txt           # 3.20+, C++17, WIN32, comctl32/winhttp/ole32/urlmon/ws2_32
├── build.bat                # CMake primary, g++ fallback
├── build_goida.bat          # CMake + legacy launcher/client/train fallback
└── goida-build.bat          # объединённый скрипт
```

## Этапы (снизу-вверх)

### Этап 1 — Фундамент
- **CMake+bat fallback** — `build.bat`/`build_goida.bat`/`goida-build.bat`: `cmake -S . -B build -G "MinGW Makefiles"` → `cmake --build build` → `ctest`, если cmake нет → `g++ -municode -mwindows`
- **DB миграция** — `src/db_migration.h`: `PRAGMA user_version`, `profiles`, `marketplace_cache`, `model_progress`, `chat_branches`; `DBInit()` → `goida::db::migrate(g_db)`
- **cpp-httplib** — `src/httplib.h` (799k, header-only), демо `EmbedThr` + `httplib::Client` (закомментирован для лёгкой сборки)
- **RAII** — `src/raii.h`: `WinHttpHandle`, `FileHandle`, `RegHandle`, `Stmt`, `GdiHandle`
- **i18n ru/en** — `src/i18n.h` + `lang/*.json` (BOM, UTF-8, парсер `"key":"value"`), `wWinMain` → `goida::i18n::instance().load()`
- **UTF8** — `src/utf8.h`: централизовано `WideCharToMultiByte CP_UTF8` / `MultiByteToWideChar CP_UTF8`
- **Тесты** — `ctest --test-dir build` 5/5 pass (`test_utf8`, `test_i18n`, `test_db`, `test_json`+`unit_json`)

### Этап 2 — Профили+Память
- **Femboy→Profiles** — таблица `profiles(id, name UNIQUE, avatar, system_prompt, temperature, max_tokens, keep_alive)`; `EnsureDefaultProfile()` мигрирует `f_name/*` → профиль `default`; UI `ShowFemboy()` → Profiles ListBox + поля + `TBM_SETRANGE 0-150` + Combo `L1 5m / L2 30m / L3 1h / Off`
- **Ключевые элементы** — память `memory(tag,value)` + `MemoryCount()` индикатор `x/100`
- **Лимит 100** — `ID_MEM_ADD` проверяет `MemoryCount()>=100` → `MessageBox` warning, `SELECT ... LIMIT 100`
- **keep_alive L1/L2/L3** — `KeepAliveFromLevel` / `LevelFromKeepAlive` / `GetActiveKeepAlive()` → в `ChatJson()` добавляется `"keep_alive":"5m"` top-level

### Этап 3 — Ollama API
- **ListView** — `WC_LISTVIEW` `LVS_REPORT` с колонками Name/Size/Modified/Family; `WM_MODEL_LIST` → `ListView_InsertItem` + `SetItemText`; marketplace второй ListView
- **Прогресс-бары** — `PROGRESS_CLASS` `PBM_SETRANGE 0,100` + `WM_MODEL_PROGRESS` (парсинг `"total":`/`"completed":` в `PullModelThr`)
- **Marketplace** — статика 8 моделей в `ShowModels`, `ID_MODEL_MKT_INSTALL` → `PullModelThr`
- **embed/images/tools** — `POST /api/embed` (`EmbedThr` → `WM_MODEL_EMBED_DONE`), `images` ( `FileToBase64` + `g_pendingImages` → `images:[base64]` в `ChatJson` ), `tools` (`g_toolsJson` + `g_toolsEnabled` → `"tools":[...]` ), кнопка `Tools demo` / `Shift+Tools` → image picker `GetOpenFileNameW` + `Base64Encode`

### Этап 4 — Chat
- **/api/chat** — `ChatJson()` строит `{"model","messages":[...],"stream":bool,"keep_alive","tools","options":{"temperature","num_predict","seed"}}`; `/api/generate` ветка сохранена для совместимости
- **RICHEDIT markdown** — `Msftedit.dll` `MSFTEDIT_CLASS`, `EM_SETBKGNDCOLOR COL_EDIT_BG`, `CHARFORMAT2` default `Consolas 9pt`; `AppendChat` с `ApplyRichFormat`: префиксы `>> User:` синий `COL_MSG_USER` bold, `<< AI:` зелёный, markdown: `#` заголовки (size 13-9pt accent), `-` списки orange, `> ` цитаты italic dim, `**bold**`, `` `code` `` Consolas orange
- **Regenerate/Branch** — `ID_CHAT_REG` удаляет последний `assistant` и resend; `ID_CHAT_BRANCH` сохраняет `g_history` → `INSERT INTO chat_branches(role,content)` + маркер `branch_point`, `WM` + `MessageBox`

### Этап 5 — UI
- **Темная тема+персонализация** — `COL_BG=28,28,30` etc, `g_colAccent = AccentFromName(g_cfg.accent)`, `DrawBtn` + `WM_DRAWITEM` используют `g_colAccent` + hover `+20` / down `-20`; `ShowAbout` секция Personalization: Theme Combo, Accent Combo, Lang Combo, Checkboxes Round/Adaptive, Shortcuts legend
- **Адаптивный** — `WM_SIZE` + `g_cfg.adaptive`: перечисление детей `id>=ID_CHAT_HIST` → `MoveWindow` с `newRight - x -16`; `WM_GETMINMAXINFO 720x480`
- **Скругления** — `g_cfg.rounded`: `CreateRoundRectRgn(...,10,10)` + `FillRgn` / `RoundRect` border в `DrawBtn` и `WM_DRAWITEM`
- **Шорткаты** — `CreateAcceleratorTableW` 14 акселлераторов: `Ctrl+1..7` страницы, `Ctrl+R` Regenerate, `Ctrl+B` Branch, `Ctrl+L` Clear, `F5` Refresh, `Esc` Stop, `Ctrl+W` Exit, `TranslateAcceleratorW` в цикле сообщений

### Этап 6 — Релиз
- **docs+README** — `docs/ARCHITECTURE.md`, `API.md`, `BUILD.md`, README обновлён
- **GOIDA.exe** — `build/GOIDA.exe` (Win32 GUI, `-municode -mwindows`, `comctl32 winhttp ole32 uuid urlmon ws2_32`) → `release/GOIDA.exe`
- **Actions** — `.github/workflows/build.yml`: `windows-latest` + `msys2` MinGW, `cmake -S . -B build` + `cmake --build build` + `ctest`, artifact `GOIDA.exe`

## Ollama endpoints ( `http://localhost:11434/api/*` )

| Метод | Путь | Использование |
|-------|------|---------------|
| POST | `/api/generate` | legacy `prompt` |
| POST | `/api/chat` | **основной** `messages` + `tools` + `images` |
| GET  | `/api/tags` | `ModelListThr` |
| POST | `/api/pull` | stream, `WM_MODEL_PROGRESS` |
| DELETE | `/api/delete` | `DeleteModelThr` |
| POST | `/api/show` | `ShowModelThr` |
| POST | `/api/copy` | `CopyModelThr` |
| POST | `/api/create` | `CreateModelThr` stream |
| POST | `/api/push` | `PushModelThr` stream |
| GET  | `/api/ps` | `RunningThr` |
| GET  | `/api/version` | `VersionThr` |
| POST | `/api/embed` | `EmbedThr` |

## Сборка

### CMake (рекомендуется)
```cmd
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
copy build\GOIDA.exe GOIDA.exe
```

### Bat fallback
```cmd
build.bat              # CMake → fallback g++
build_goida.bat        # + legacy launcher/client/train
goida-build.bat        # объединённый
```

Прямой `g++`:
```cmd
g++ -std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE src/goida.cpp src/sqlite3.c -o GOIDA.exe -lcomctl32 -lwinhttp -lole32 -luuid -lurlmon -lws2_32 -lwsock32
```

## Запуск

1. Запустите `GOIDA.exe` (потребует `Ollama` на `http://localhost:11434`)
2. `Models` → `Refresh` → выберите модель → `Pull` (прогресс)
3. `Profiles` → Create → Save → Load (keep_alive L1/L2/L3)
4. `Chat` → введите сообщение (Enter — send, Shift+Enter — newline, `Tools demo` / `Shift+Tools` → image, `Reg`/`Branch`)
5. `Memory` — Tag/Value → Add (лимит 100)
6. `Console` — логи, `Update`, `Auto-start` (HKCU\...\Run)
7. `About` — Personalization (Theme/Accent/Round/Adaptive/Lang)

## Горячие клавиши
`Ctrl+1..7` страницы, `Ctrl+Enter` Send, `Ctrl+R` Regenerate, `Ctrl+B` Branch, `Ctrl+L` Clear, `F5` Refresh, `Esc` Stop

## Конфигурация (SQLite `global/goida.db`)
- `config` — `api_url`, `model`, `temp`, `max_tokens`, `seed`, `sys_prompt`, `json_mode`, `lang`, `theme`, `accent`, `rounded`, `adaptive`, `f_*`, `update_url`, `active_profile`
- `chat_messages`, `prompts`, `memory`, `logs`, `femboy`, `profiles`, `marketplace_cache`, `model_progress`, `chat_branches`
- `PRAGMA user_version` 1..3, `journal_mode=WAL`, `foreign_keys=ON`

## Лицензия
MIT. Подготовлено для GOIDA AI MANAGER v2.1.
