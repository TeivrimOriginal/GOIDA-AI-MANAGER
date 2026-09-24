# Ollama API — GOIDA

Базовый URL: `g_cfg.api_url` (по умолчанию `http://localhost:11434/api/chat`, legacy `/api/generate` поддерживается).

## Chat
```json
POST /api/chat
{
  "model": "qwen:14b",
  "messages": [{"role":"system","content":"..."},{"role":"user","content":"hi","images":["base64..."]}],
  "stream": true,
  "keep_alive": "5m",
  "tools": [{"type":"function","function":{"name":"calculator",...}}],
  "options": {"temperature":0.7,"num_predict":4096,"seed":42},
  "format": "json" // if json_mode
}
```
Поток: `WinHttpReadData` → строки с `"\n"` → извлечение `"response":"..."` / `"content":"..."` → `PostMessage WM_HTTP_CHUNK`; `done:true` → `WM_HTTP_DONE`.

KeepAlive: `GetActiveKeepAlive()` читает `profiles.keep_alive` активного профиля.

## Generate (compat)
```json
POST /api/generate
{"model":"qwen:14b","prompt":"user: hi\n","stream":true,"keep_alive":"5m","options":{...}}
```

## Embed
```json
POST /api/embed
{"model":"nomic-embed-text","input":"текст"}
```
`EmbedThr` → `WM_MODEL_EMBED_DONE`.

## Models
- `GET /api/tags` → `ModelListThr` парсит `"name":"...", "size":123, "modified_at":"..."`
- `POST /api/pull` `{"name":"qwen:14b","stream":true}` → строки с `"status"`, `"total"`, `"completed"` → `WM_MODEL_PROGRESS pct`
- `DELETE /api/delete` `{"name":"..."}`
- `POST /api/show` `{"name":"..."}`
- `POST /api/copy` `{"source":"...","destination":"..."}`
- `POST /api/create` `{"model":"...","from":"...", "stream":true}`
- `POST /api/push` `{"model":"...","stream":true}`
- `GET /api/ps` → список running
- `GET /api/version` → `{"version":"0.6.5"}`

## Errors
Все потоки постят `WM_HTTP_ERR` с `L"ERR:..."` → `AppendChat("\n[ERR] ")`.

## Tools / Images
- `tools` включается кнопкой `Tools demo` → `g_toolsJson` глобально, `ChatJson` добавляет `"tools":[...]"` top-level.
- `images` — выбор файла `GetOpenFileNameW` → `FileToBase64` → `g_pendingImages` (vector<string> base64) → `ChatJson` добавляет `"images":[base64]` к последнему user-сообщению, одноразово (очищается после `DoSend`).

## Таймауты
`WinHttpSetTimeouts(30000)` для generate/chat, `120000` для pull/create/push, `5000` для tags/ps/version, `10000` для delete/show/copy.
