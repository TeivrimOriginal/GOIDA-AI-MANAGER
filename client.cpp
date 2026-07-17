#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

enum {
    IDC_COMBO=100, IDC_INPUT, IDC_SEND, IDC_STOP, IDC_STATUS, IDC_CHAT,
    IDC_CLEAR, IDC_HISTORY, IDC_MODEL_REFRESH
};

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define WM_TOK   (WM_USER+100)
#define WM_DONE  (WM_USER+101)
#define WM_STAT  (WM_USER+102)
#define WM_TRAYICON (WM_USER+200)
#define TRAY_ID 1

// Dark theme colors (Catppuccin Mocha)
#define COL_BG        0x1E1E2E
#define COL_SURFACE   0x313244
#define COL_SURFACE1  0x45475A
#define COL_TEXT      0xCDD6F4
#define COL_SUBTEXT   0xA6ADC8
#define COL_ACCENT    0x89B4FA
#define COL_GREEN     0xA6E3A1
#define COL_RED       0xF38BA8

HFONT g_font, g_font_big, g_font_title;
HBRUSH g_bg_brush, g_surface_brush;
HWND g_chat, g_input, g_combo, g_status;
HICON g_hIconNormal, g_hIconActive;
int g_stop = 0;
wchar_t g_cfg_name[128] = L"";
wchar_t g_cfg_sysprompt[4096] = L"";
double g_cfg_temp = 0.7;
wchar_t g_cfg_model[256] = L"";
std::vector<std::pair<std::string,std::string>> g_hist;
NOTIFYICONDATAW g_nid = {};

enum MemTag { TAG_LIKES=0, TAG_DISLIKES, TAG_FACTS, TAG_PREFERENCES, TAG_MEMORIES };
struct MemoryEntry {
    int tag;
    std::wstring key;
    std::wstring value;
    time_t timestamp;
};
std::vector<MemoryEntry> g_memories;
std::wstring g_app_dir;

std::wstring utf8w(const std::string &s) {
    if (s.empty()) return L"";
    int l = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), NULL, 0);
    std::wstring w(l, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &w[0], l);
    return w;
}
std::string w8utf(const std::wstring &w) {
    if (w.empty()) return "";
    int l = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(), NULL, 0, NULL, NULL);
    std::string s(l, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(), &s[0], l, NULL, NULL);
    return s;
}

std::string esc_json(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else if (c == '\t') o += "\\t";
        else o += c;
    }
    return o;
}

std::string decode_unicode_escapes(const std::string &s) {
    std::string o;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == '\\' && i + 5 < s.length() && s[i+1] == 'u') {
            unsigned int cp = 0;
            for (int j = 2; j < 6; j++) {
                char c = s[i+j];
                cp <<= 4;
                if (c >= '0' && c <= '9') cp |= (c - '0');
                else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
            }
            i += 5;
            if (cp < 0x80) o += (char)cp;
            else if (cp < 0x800) { o += (char)(0xC0|(cp>>6)); o += (char)(0x80|(cp&0x3F)); }
            else { o += (char)(0xE0|(cp>>12)); o += (char)(0x80|((cp>>6)&0x3F)); o += (char)(0x80|(cp&0x3F)); }
        } else {
            o += s[i];
        }
    }
    return o;
}

std::string extract_token(const std::string &line) {
    std::string key = "\"content\":\"";
    size_t p = line.find(key);
    if (p == std::string::npos) return "";
    p += key.length();
    std::string tok;
    while (p < line.length()) {
        if (line[p] == '"' && (p == 0 || line[p-1] != '\\')) break;
        if (line[p] == '\\' && p + 1 < line.length()) {
            char nxt = line[p+1];
            if (nxt == '"') { tok += '"'; p += 2; continue; }
            if (nxt == '\\') { tok += '\\'; p += 2; continue; }
            if (nxt == 'n') { tok += '\n'; p += 2; continue; }
            if (nxt == 'r') { p += 2; continue; }
            if (nxt == 't') { tok += '\t'; p += 2; continue; }
            if (nxt == 'u' && p + 5 < line.length()) {
                std::string esc = line.substr(p, 6);
                tok += decode_unicode_escapes(esc);
                p += 6;
                continue;
            }
        }
        tok += line[p];
        p++;
    }
    return tok;
}

SOCKET ollama_connect() {
    static int init = 0;
    if (!init) { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); init = 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(OLLAMA_PORT);
    inet_pton(AF_INET, OLLAMA_HOST, &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return INVALID_SOCKET; }
    return s;
}

std::string http_get(const char *path) {
    SOCKET s = ollama_connect();
    if (s == INVALID_SOCKET) return "";
    char rq[512];
    int len = snprintf(rq, 512, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n", path, OLLAMA_HOST, OLLAMA_PORT);
    send(s, rq, len, 0);
    std::string resp; char buf[4096]; int n;
    while ((n = recv(s, buf, 4095, 0)) > 0) { buf[n] = 0; resp += buf; }
    closesocket(s);
    size_t p = resp.find("\r\n\r\n");
    return (p != std::string::npos) ? resp.substr(p + 4) : "";
}

std::string http_post(const char *path, const std::string &body) {
    SOCKET s = ollama_connect();
    if (s == INVALID_SOCKET) return "";
    char rq[4096];
    int rql = snprintf(rq, 4096,
        "POST %s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
        path, OLLAMA_HOST, OLLAMA_PORT, (int)body.size(), body.c_str());
    send(s, rq, rql, 0);
    std::string resp; char buf[4096]; int n;
    while ((n = recv(s, buf, 4095, 0)) > 0) { buf[n] = 0; resp += buf; }
    closesocket(s);
    size_t p = resp.find("\r\n\r\n");
    return (p != std::string::npos) ? resp.substr(p + 4) : "";
}

std::wstring get_app_dir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring wp(path);
    return wp.substr(0, wp.find_last_of(L'\\') + 1);
}

std::string read_file(const std::wstring &path) {
    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);
    FILE *f = fopen(cpath, "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf(sz, 0);
    fread(&buf[0], 1, sz, f);
    fclose(f);
    return buf;
}

void write_file(const std::wstring &path, const std::string &content) {
    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);
    FILE *f = fopen(cpath, "w");
    if (!f) return;
    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
}

std::string extract_field(const std::string &content, const std::string &key) {
    size_t p = content.find(key + "=");
    if (p == std::string::npos) return "";
    p += key.length() + 1;
    size_t e = content.find_first_of("\r\n", p);
    if (e == std::string::npos) e = content.length();
    return content.substr(p, e - p);
}

// === Memory ===
void load_memories() {
    g_memories.clear();
    std::wstring mpath = get_app_dir() + L"global\\memory.dat";
    std::string content = read_file(mpath);
    if (content.empty()) return;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        int tag = atoi(line.substr(0, p1).c_str());
        size_t p2 = line.find('|', p1+1);
        if (p2 == std::string::npos) continue;
        std::string key = line.substr(p1+1, p2-p1-1);
        size_t p3 = line.find('|', p2+1);
        if (p3 == std::string::npos) continue;
        std::string val = line.substr(p2+1, p3-p2-1);
        std::string ts = line.substr(p3+1);
        MemoryEntry m;
        m.tag = tag;
        m.key = utf8w(key);
        m.value = utf8w(val);
        m.timestamp = (time_t)atoll(ts.c_str());
        g_memories.push_back(m);
    }
}

// === Config ===
void load_config() {
    std::wstring cfg_path = get_app_dir() + L"config.ini";
    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, cfg_path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);
    char buf[8192] = {0};
    FILE *f = fopen(cpath, "r");
    if (!f) { wcscpy(g_cfg_name, L""); wcscpy(g_cfg_sysprompt, L""); g_cfg_temp = 0.7; wcscpy(g_cfg_model, L""); return; }
    fread(buf, 1, 8191, f);
    fclose(f);
    std::string content = buf;
    auto ex = [&](const std::string &key) -> std::string {
        size_t p = content.find(key + "=");
        if (p == std::string::npos) return "";
        p += key.length() + 1;
        size_t e = content.find_first_of("\r\n", p);
        if (e == std::string::npos) e = content.length();
        return content.substr(p, e - p);
    };
    std::string n = ex("name"), m = ex("model"), t = ex("temperature"), s = ex("system_prompt");
    if (!n.empty()) { std::wstring wn = utf8w(n); wcscpy(g_cfg_name, wn.c_str()); }
    if (!m.empty()) { std::wstring wm = utf8w(m); wcscpy(g_cfg_model, wm.c_str()); }
    if (!t.empty()) g_cfg_temp = atof(t.c_str());
    if (!s.empty()) {
        std::string dec;
        for (size_t i = 0; i < s.length(); i++) {
            if (s[i] == '\\' && i + 1 < s.length()) {
                if (s[i+1] == 'n') { dec += '\n'; i++; }
                else if (s[i+1] == 'r') { i++; }
                else if (s[i+1] == 't') { dec += '\t'; i++; }
                else if (s[i+1] == '\\') { dec += '\\'; i++; }
                else dec += s[i];
            } else dec += s[i];
        }
        std::wstring ws = utf8w(dec);
        wcscpy(g_cfg_sysprompt, ws.c_str());
    }
}

// === Chat History ===
void save_chat_history() {
    std::string content;
    for (auto &p : g_hist) {
        content += p.first + "|" + p.second + "\n";
    }
    write_file(get_app_dir() + L"chat_history.dat", content);
}

void load_chat_history() {
    g_hist.clear();
    std::string content = read_file(get_app_dir() + L"chat_history.dat");
    if (content.empty()) return;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty()) continue;
        size_t p = line.find('|');
        if (p == std::string::npos) continue;
        g_hist.push_back({line.substr(0, p), line.substr(p + 1)});
    }
}

// === Build system prompt with memory ===
std::wstring build_system_prompt() {
    std::wstring sys = g_cfg_sysprompt;
    if (sys.empty()) {
        sys = L"Ты — фембой-парень, 19 лет. Живой человек, не ИИ. Нежный, ласковый, заботливый. Пиши ~, ♡, ня, мур. Люби и обнимай.";
    }
    if (!g_memories.empty()) {
        sys += L"\n\n=== Память о пользователе ===";
        for (auto &m : g_memories) {
            const wchar_t* tag_names[] = {L"❤ Нравится", L"💔 Не нравится", L"📝 Факт", L"⚙ Предпочтение", L"💭 Память"};
            sys += L"\n[" + std::wstring(tag_names[m.tag]) + L"] " + m.key + L": " + m.value;
        }
    }
    return sys;
}

// === UI ===
void refresh_models() {
    SendMessageW(g_combo, CB_RESETCONTENT, 0, 0);
    std::string resp = http_get("/api/tags");
    std::string key = "\"name\":\"";
    size_t p = 0;
    int saved_sel = -1;
    while ((p = resp.find(key, p)) != std::string::npos) {
        p += key.length();
        size_t e = resp.find("\"", p);
        if (e != std::string::npos) {
            std::string name = resp.substr(p, e - p);
            SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)utf8w(name).c_str());
            if (g_cfg_model[0] && _wcsicmp(utf8w(name).c_str(), g_cfg_model) == 0)
                saved_sel = (int)SendMessageW(g_combo, CB_GETCOUNT, 0, 0) - 1;
            p = e + 1;
        }
    }
    if (saved_sel >= 0)
        SendMessageW(g_combo, CB_SETCURSEL, saved_sel, 0);
    else if (SendMessageW(g_combo, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(g_combo, CB_SETCURSEL, 0, 0);
}

std::wstring get_char_name() {
    if (wcslen(g_cfg_name) == 0) return L"Фембойчик";
    return g_cfg_name;
}

void add_chat_line(const wchar_t *role, const std::wstring &text) {
    int len = GetWindowTextLengthW(g_chat);
    if (len > 0) {
        SendMessageW(g_chat, EM_SETSEL, len, len);
        SendMessageW(g_chat, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
    std::wstring hdr = L"\r\n";
    hdr += role;
    hdr += L" - ";
    SendMessageW(g_chat, EM_REPLACESEL, FALSE, (LPARAM)hdr.c_str());
    len = GetWindowTextLengthW(g_chat);
    SendMessageW(g_chat, EM_SETSEL, len, len);
    SendMessageW(g_chat, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageW(g_chat, EM_SETSEL, -1, -1);
    SendMessageW(g_chat, EM_SCROLLCARET, 0, 0);
}

void tray_notify(HWND hwnd, const wchar_t *title, const wchar_t *text) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ID;
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_INFO;
    wcsncpy(g_nid.szInfoTitle, title, 64);
    wcsncpy(g_nid.szInfo, text, 256);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void tray_set_icon(HWND hwnd, HICON icon) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = icon;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

struct ChatData { std::string body; HWND hwnd; };

DWORD WINAPI chat_thread(LPVOID param) {
    ChatData *d = (ChatData *)param;
    g_stop = 0;
    SOCKET s = ollama_connect();
    if (s == INVALID_SOCKET) {
        PostMessageW(d->hwnd, WM_STAT, 0, (LPARAM)_wcsdup(L"Нет соединения с Ollama!"));
        delete d; return 1;
    }
    char header[1024];
    int hlen = snprintf(header, 1024,
        "POST /api/chat HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        OLLAMA_HOST, OLLAMA_PORT, (int)d->body.size());
    send(s, header, hlen, 0);
    send(s, d->body.c_str(), (int)d->body.size(), 0);
    std::string full_reply, buffer;
    char chunk[8192]; int n;
    while ((n = recv(s, chunk, sizeof(chunk)-1, 0)) > 0) {
        if (g_stop) break;
        chunk[n] = '\0';
        buffer += chunk;
        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (line.empty()) continue;
            std::string tok = extract_token(line);
            if (!tok.empty()) {
                full_reply += tok;
                wchar_t *wtok = _wcsdup(utf8w(tok).c_str());
                PostMessageW(d->hwnd, WM_TOK, 0, (LPARAM)wtok);
            }
            if (line.find("\"done\":true") != std::string::npos) goto FINISH;
        }
    }
FINISH:
    closesocket(s);
    if (!full_reply.empty()) {
        wchar_t *wreply = _wcsdup(utf8w(full_reply).c_str());
        PostMessageW(d->hwnd, WM_DONE, 0, (LPARAM)wreply);
    }
    PostMessageW(d->hwnd, WM_STAT, 0, (LPARAM)_wcsdup(L"Готово"));
    delete d;
    return 0;
}

void send_to_ollama(HWND hwnd, const std::wstring &user_msg) {
    int sel = SendMessageW(g_combo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) { SetWindowTextW(g_status, L"Выберите модель!"); return; }
    wchar_t model[256];
    SendMessageW(g_combo, CB_GETLBTEXT, sel, (LPARAM)model);
    std::string user_utf8 = w8utf(user_msg);
    g_hist.push_back({"user", user_utf8});
    if (g_hist.size() > 50) g_hist.erase(g_hist.begin());
    add_chat_line(L"Вы", user_msg.c_str());
    std::wstring cname = get_char_name();
    add_chat_line(cname.c_str(), L"");
    std::string messages = "[";
    std::wstring sys_prompt = build_system_prompt();
    if (!sys_prompt.empty()) {
        messages += "{\"role\":\"system\",\"content\":\"" + esc_json(w8utf(sys_prompt)) + "\"}";
    }
    for (size_t i = 0; i < g_hist.size(); i++) {
        if (i > 0 || !sys_prompt.empty()) messages += ",";
        messages += "{\"role\":\"" + g_hist[i].first + "\",\"content\":\"" + esc_json(g_hist[i].second) + "\"}";
    }
    messages += "]";
    char temp_s[32];
    snprintf(temp_s, sizeof(temp_s), "%.2f", g_cfg_temp);
    std::string model_utf8 = w8utf(model);
    std::string body = "{\"model\":\"" + model_utf8 + "\",\"messages\":" + messages + ",\"stream\":true,\"options\":{\"temperature\":" + temp_s + ",\"num_ctx\":4096,\"repeat_penalty\":1.1,\"num_predict\":512}}";
    EnableWindow(GetDlgItem(hwnd, IDC_SEND), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_STOP), TRUE);
    SetWindowTextW(g_status, L"Генерация...");
    ChatData *d = new ChatData; d->body = body; d->hwnd = hwnd;
    HANDLE h = CreateThread(NULL, 0, chat_thread, d, 0, NULL);
    CloseHandle(h);
}

void do_send() {
    wchar_t inp[8192];
    GetWindowTextW(g_input, inp, 8192);
    if (wcslen(inp) == 0) return;
    HWND hwnd = GetParent(g_chat);
    SetWindowTextW(g_input, L"");
    send_to_ollama(hwnd, inp);
}

// === Drawing ===
LRESULT OnCtlColor(HWND w, WPARAM wp, LPARAM lp, int type) {
    HDC hdc = (HDC)wp;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT);
    if (type == CTLCOLOR_STATIC || type == CTLCOLOR_DLG) {
        SetBkColor(hdc, COL_BG);
        return (LRESULT)g_bg_brush;
    }
    if (type == CTLCOLOR_EDIT || type == CTLCOLOR_LISTBOX) {
        SetBkColor(hdc, COL_SURFACE);
        return (LRESULT)g_surface_brush;
    }
    if (type == CTLCOLOR_BTN) {
        SetBkColor(hdc, COL_SURFACE1);
        return (LRESULT)g_surface_brush;
    }
    return DefWindowProcW(w, WM_CTLCOLORMSGBOX + type - 1, wp, lp);
}

LRESULT OnEraseBkgnd(HWND w, WPARAM wp) {
    HDC hdc = (HDC)wp;
    RECT rc; GetClientRect(w, &rc);
    FillRect(hdc, &rc, g_bg_brush);
    RECT nav = {0, 0, rc.right, 45};
    FillRect(hdc, &nav, g_surface_brush);
    return 1;
}

// === WndProc ===
LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        g_app_dir = get_app_dir();
        g_font = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_font_big = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_font_title = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_bg_brush = CreateSolidBrush(COL_BG);
        g_surface_brush = CreateSolidBrush(COL_SURFACE);
        load_config();
        load_memories();
        load_chat_history();

        // Nav bar
        HWND h = CreateWindowW(L"STATIC", L"GOIDA AI", WS_CHILD|WS_VISIBLE|SS_LEFT, 20, 10, 300, 30, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font_title, 0);
        h = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE|SS_RIGHT, 500, 10, 200, 30, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        wchar_t user[256];
        swprintf(user, 256, L"👤 %s | 🤖 %s", g_cfg_name[0] ? g_cfg_name : L"Аноним", get_char_name().c_str());
        SetWindowTextW(h, user);

        // Model combo
        h = CreateWindowW(L"STATIC", L"Модель:", WS_CHILD|WS_VISIBLE, 20, 55, 70, 25, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_combo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, 95, 52, 200, 200, w, (HMENU)IDC_COMBO, 0, 0);
        SendMessageW(g_combo, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"⟳", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 300, 50, 30, 30, w, (HMENU)IDC_MODEL_REFRESH, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font_big, 0);
        h = CreateWindowW(L"BUTTON", L"🗑 Очистить", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 340, 50, 100, 30, w, (HMENU)IDC_CLEAR, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"📜 История", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 450, 50, 100, 30, w, (HMENU)IDC_HISTORY, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        g_chat = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_HSCROLL,
            20, 95, 640, 380, w, (HMENU)IDC_CHAT, 0, 0);
        SendMessageW(g_chat, WM_SETFONT, (WPARAM)g_font, 0);
        SendMessageW(g_chat, EM_SETLIMITTEXT, 8*1024*1024, 0);

        g_input = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,
            20, 490, 450, 30, w, (HMENU)IDC_INPUT, 0, 0);
        SendMessageW(g_input, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"Отправить", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON, 480, 488, 90, 35, w, (HMENU)IDC_SEND, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Стоп", WS_CHILD|WS_VISIBLE, 580, 488, 80, 35, w, (HMENU)IDC_STOP, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);

        g_status = CreateWindowW(L"STATIC", L"Готово", WS_CHILD|WS_VISIBLE, 20, 535, 640, 25, w, (HMENU)IDC_STATUS, 0, 0);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)g_font, 0);

        g_hIconNormal = LoadIcon(NULL, IDI_APPLICATION);
        g_hIconActive = LoadIcon(NULL, IDI_INFORMATION);
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = w;
        g_nid.uID = TRAY_ID;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hIconNormal;
        wcscpy(g_nid.szTip, L"GOIDA AI");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        refresh_models();
        SetFocus(g_input);
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
        return OnCtlColor(w, wp, lp, m - WM_CTLCOLORMSGBOX);

    case WM_ERASEBKGND:
        return OnEraseBkgnd(w, wp);

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SEND: do_send(); break;
        case IDC_STOP:
            g_stop = 1;
            EnableWindow(GetDlgItem(w, IDC_SEND), TRUE);
            EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);
            SetWindowTextW(g_status, L"Остановлено");
            break;
        case IDC_CLEAR:
            SetWindowTextW(g_chat, L"");
            g_hist.clear();
            save_chat_history();
            SetWindowTextW(g_status, L"Чат очищен");
            break;
        case IDC_HISTORY: {
            std::wstring hist;
            for (auto &p : g_hist) {
                hist += utf8w(p.first) + L": " + utf8w(p.second) + L"\n\n";
            }
            if (hist.empty()) hist = L"История пуста";
            MessageBoxW(w, hist.c_str(), L"История чата", MB_OK | MB_ICONINFORMATION);
            break;
        }
        case IDC_MODEL_REFRESH:
            refresh_models();
            SetWindowTextW(g_status, L"Модели обновлены");
            break;
        }
        break;

    case WM_TOK: {
        wchar_t *tok = (wchar_t *)lp;
        if (tok) {
            int len = GetWindowTextLengthW(g_chat);
            SendMessageW(g_chat, EM_SETSEL, len, len);
            SendMessageW(g_chat, EM_REPLACESEL, FALSE, (LPARAM)tok);
            SendMessageW(g_chat, EM_SCROLLCARET, 0, 0);
            free(tok);
        }
        break;
    }
    case WM_DONE: {
        wchar_t *reply = (wchar_t *)lp;
        if (reply) {
            g_hist.push_back({"assistant", w8utf(reply)});
            if (g_hist.size() > 50) g_hist.erase(g_hist.begin());
            free(reply);
        }
        EnableWindow(GetDlgItem(w, IDC_SEND), TRUE);
        EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);
        tray_set_icon(w, g_hIconNormal);
        save_chat_history();
        break;
    }
    case WM_STAT: {
        wchar_t *st = (wchar_t *)lp;
        if (st) { SetWindowTextW(g_status, st); free(st); }
        break;
    }
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(w, SW_RESTORE);
            SetForegroundWindow(w);
        }
        break;

    case WM_SIZE: {
        int W = LOWORD(lp), H = HIWORD(lp);
        if (W < 200 || H < 200) break;
        MoveWindow(g_chat, 20, 95, W - 40, H - 165, TRUE);
        MoveWindow(g_input, 20, H - 50, W - 150, 30, TRUE);
        MoveWindow(GetDlgItem(w, IDC_SEND), W - 120, H - 52, 100, 35, TRUE);
        MoveWindow(GetDlgItem(w, IDC_STOP), W - 15, H - 52, 80, 35, TRUE);
        MoveWindow(g_status, 20, H - 25, W - 40, 20, TRUE);
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 500;
        mm->ptMinTrackSize.y = 400;
        break;
    }

    case WM_CLOSE:
        ShowWindow(w, SW_HIDE);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        DeleteObject(g_font);
        DeleteObject(g_font_big);
        DeleteObject(g_font_title);
        DeleteObject(g_bg_brush);
        DeleteObject(g_surface_brush);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE hP, LPWSTR cL, int sH) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"GoidaAIClient";
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, L"GoidaAIClient", L"GOIDA AI",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, (sx - 700) / 2, (sy - 600) / 2, 700, 600,
        NULL, NULL, hI, NULL);
    ShowWindow(hwnd, sH);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == g_input) {
            do_send(); continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}