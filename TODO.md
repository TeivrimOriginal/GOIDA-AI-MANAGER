# GOIDA AI MANAGER TODO — источник правды v2.4

## Точка А (сейчас) — факт 2026-09-24
- goida.cpp 2439 строк монолит src/goida.cpp:1, 7 страниц Chat/Trainer/Profiles(Femboy)/Memory/Console/Models/About, навигация слева 7 кнопок Ctrl+1..7
- Ollama http://localhost:11434/api/* : generate/chat/tags/pull/delete/show/copy/create/ps/version/push/embed — 10+ потоков WinHttp + CreateThread, WM_HTTP_CHUNK/DONE/ERR src/goida.cpp:65
- Chat: streaming g_history:75, InputProc Enter/S Shift+Enter newline, Stop/Clear/Save/Load/Test/CtxClr/SaveAs, ConnCheckThr 30с, ModelListThr, RICHEDIT markdown >> User/<< AI src/goida.cpp:1038, accent-кнопки SecLbl
- Profiles: таблица profiles src/db_migration.h:42 keep_alive L1:5m L2:30m L3:1h src/goida.cpp:329, лимит Memory 100 MemoryCount:321
- Models: 2 поля + 7 кнопок + ListView Name|Size|Modified + Progress bar src/goida.cpp:1466 + Marketplace static mk[] src/goida.cpp:1489
- Build: CMake 3.20 primary src/CMakeLists.txt:1 (MinGW static, GOIDA.exe 1.88MB <2MB) + build_goida.bat/build.bat fallback g++ -municode -mwindows -lwinhttp -lcomctl32 -lole32 -luuid -lurlmon -lws2_32, ctest 5/5 passed
- DB: goida.db WAL/FK migrate v3 src/db_migration.h:29 config/chat_messages/prompts/memory/logs/femboy/profiles/marketplace_cache/chat_branches
- i18n ru/en lang/ru.json lang/en.json src/i18n.h:1, UTF-8 src/utf8.h:1, RAII src/raii.h:1, httplib.h опционально
- Риски: дубли DoSend/HTTP потоков, магические ID/RECT enum:48, new wstring утечки WM_HTTP_*, нет DPI/resize адаптива, нет пагинации истории, нет валидации API URL

## Точка Б (цель v2.4) — One-Click Privacy для чайника
- Один GOIDA.exe <2MB static, установка с нуля на VM <3 мин, Kill-Switch не нужен (для GOIDA: keep_alive переживает ребут через DB active_profile)
- Страницы: Launcher / Models (Local+Mkt ListView+Progress+поиск) / Download / Create Profile / Profiles / Profile Detail/Edit / Chat (RICHEDIT) / Memory (key elements в профиле) / Console (INFO/WARN/ERR фильтр, 3 дня) / About + Settings + Updates
- Сценарии: скачать модель → сделать профиль → поболтать (сырая переписка → ключевые элементы в память → отображение в профиле) — по умолчанию 100 запросов, branches DB не теряют переписку
- Ollama: chat дефолт + generate свитч, images base64 llava, tools/think ограниченно стандартизировано, embed для Memory, keep_alive 3 уровня + авто-переход по таймеру
- UI: тёмная + персонализация (theme dark/amoled/light, accent blue/purple/green/orange/pink, rounded, adaptive/dynamic), скругления, тёмные скроллы, шорткаты Ctrl+Enter/Esc/Ctrl+L/Ctrl+S/Ctrl+R/Ctrl+B/F5

## Этапы снизу-вверх (делать по порядку)
### Этап1 Фундамент — DONE
- [x] CMake + bat fallback, DB миграция v3, utf8/raii/i18n, ctest 5/5
- [ ] Вынести exec/peers/config модули (для GOIDA: ollama_client.h, profiles.h, db.h) — уменьшить goida.cpp с 2439 → ~1500+модули

### Этап2 Профили+Память
- [ ] Femboy→Profiles финал (переименовать UI), avatar/system/temp/tokens/keep_alive, лимит 100, активный профиль в config
- [ ] Ключевые элементы: после чата выявлять предпочтения/эмоции/факты → memory tag|value, показывать в Profile Detail
- [ ] История 100 по умолчанию + настройка автоудаления (время/кол-во) + branches не теряют

### Этап3 Ollama API — кнопки
- [ ] ListView Local Name|Size|Modified|Family, прогресс-бар Pull/Push total/completed, поиск/фильтр, Unload keep_alive:0
- [ ] Show Info таблицей (параметр raw/json), Marketplace fetch https://ollama.com/library через public API
- [ ] Параметры UI: temp/tokens/seed/json_mode + top_p/top_k/repeat_penalty/num_ctx/stop в Settings, keep_alive кнопки в Chat

### Этап4 Chat
- [ ] /api/chat дефолт + свитч generate, RICHEDIT оставить, Regenerate/Copy/Edit/Branch, пагинация/поиск/удаление одного
- [ ] System_prompt на профиль, sys_editor на Chat, CtxClr/Shift+Enter, Stop/Clear/Save/Load DB + Export json/md/txt (SaveAs), Test → GET /api/version

### Этап5 UI/UX
- [ ] Settings страница (API/Model/keep_alive thresholds L1/L2/L3), тёмная+персонализация, адаптивный по умолчанию + опция динамичный, скругления везде, шорткаты

### Этап6 Релиз
- [ ] docs/*.md + README ru/en человечно, GOIDA.exe Actions build зеленый, проверка на VM, трей minimize-to-tray оптимизация, Check Update app+models

## Не делать
VPS-сеть, Telegram-бот, монетизация, кроссплатформа сейчас (только Win10/11, WinHTTP; httplib для будущего), Electron, мобильное

## Критерий готовности релиза
cmake --build build зеленый, ctest 5/5, exe <2MB static, 9 страниц работают, Pull с прогрессом, Profile keep_alive L1/L2/L3, Chat RichEdit + branches, установка VM <3 мин

## Следующий шаг
Этап1 рефактор goida.cpp → модули src/ollama_client.h + src/profiles.h (вынести HttpThr/Pull/Delete/Show + Profile) — проверить cmake --build build
