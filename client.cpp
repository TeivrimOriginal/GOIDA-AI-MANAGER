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

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

enum { IDC_COMBO=100, IDC_INPUT, IDC_SEND, IDC_STOP, IDC_STATUS, IDC_CHAT };

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define WM_TOK   (WM_USER+100)
#define WM_DONE  (WM_USER+101)
#define WM_STAT  (WM_USER+102)
#define WM_TRAYICON (WM_USER+200)
#define TRAY_ID 1

HWND g_chat, g_input, g_combo, g_status;
HFONT g_font;
HICON g_hIconNormal, g_hIconActive;
int g_stop = 0;
wchar_t g_cfg_name[128] = L"";
wchar_t g_cfg_sysprompt[4096] = L"";
double g_cfg_temp = 0.7;
wchar_t g_cfg_model[256] = L"";
std::vector<std::pair<std::string,std::string>> g_hist;
NOTIFYICONDATAW g_nid = {};

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

void load_config() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring wp(path);
    size_t pos = wp.find_last_of(L'\\');
    std::wstring dir = wp.substr(0, pos + 1);
    std::wstring cfg_path = dir + L"config.ini";

    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, cfg_path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);

    char buf[8192] = {0};
    FILE *f = fopen(cpath, "r");
    if (!f) {
        wcscpy(g_cfg_name, L"");
        wcscpy(g_cfg_sysprompt, L"");
        g_cfg_temp = 0.7;
        wcscpy(g_cfg_model, L"");
        return;
    }
    fread(buf, 1, 8191, f);
    fclose(f);

    std::string content = buf;
    auto extract = [&](const std::string &key) -> std::string {
        size_t p = content.find(key + "=");
        if (p == std::string::npos) return "";
        p += key.length() + 1;
        size_t e = content.find_first_of("\r\n", p);
        if (e == std::string::npos) e = content.length();
        return content.substr(p, e - p);
    };

    std::string name = extract("name");
    std::string model = extract("model");
    std::string temp = extract("temperature");
    std::string sys = extract("system_prompt");

    if (!name.empty()) {
        std::wstring wn = utf8w(name);
        wcscpy(g_cfg_name, wn.c_str());
    }
    if (!model.empty()) {
        std::wstring wm = utf8w(model);
        wcscpy(g_cfg_model, wm.c_str());
    }
    if (!temp.empty()) g_cfg_temp = atof(temp.c_str());
    if (!sys.empty()) {
        std::string decoded;
        for (size_t i = 0; i < sys.length(); i++) {
            if (sys[i] == '\\' && i + 1 < sys.length()) {
                if (sys[i+1] == 'n') { decoded += '\n'; i++; }
                else if (sys[i+1] == 'r') { i++; }
                else if (sys[i+1] == 't') { decoded += '\t'; i++; }
                else if (sys[i+1] == '\\') { decoded += '\\'; i++; }
                else decoded += sys[i];
            } else {
                decoded += sys[i];
            }
        }
        std::wstring ws = utf8w(decoded);
        wcscpy(g_cfg_sysprompt, ws.c_str());
    }
}

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
    if (wcslen(g_cfg_name) == 0) return L"\u0424\u0435\u043c\u0431\u043e\u0439\u0447\u0438\u043a";
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

struct ChatData {
    std::string body;
    HWND hwnd;
};

DWORD WINAPI chat_thread(LPVOID param) {
    ChatData *d = (ChatData *)param;
    g_stop = 0;

    SOCKET s = ollama_connect();
    if (s == INVALID_SOCKET) {
        PostMessageW(d->hwnd, WM_STAT, 0, (LPARAM)_wcsdup(L"\u041d\u0435\u0442 \u0441\u043e\u0435\u0434\u0438\u043d\u0435\u043d\u0438\u044f \u0441 Ollama!"));
        delete d;
        return 1;
    }

    char header[1024];
    int hlen = snprintf(header, 1024,
        "POST /api/chat HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        OLLAMA_HOST, OLLAMA_PORT, (int)d->body.size());

    send(s, header, hlen, 0);
    send(s, d->body.c_str(), (int)d->body.size(), 0);

    std::string full_reply;
    std::string buffer;
    char chunk[8192];
    int n;

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
    PostMessageW(d->hwnd, WM_STAT, 0, (LPARAM)_wcsdup(L"Ready"));
    delete d;
    return 0;
}

void send_to_ollama(HWND hwnd, const std::wstring &user_msg) {
    int sel = SendMessageW(g_combo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) { SetWindowTextW(g_status, L"\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u043c\u043e\u0434\u0435\u043b\u044c!"); return; }

    wchar_t model[256];
    SendMessageW(g_combo, CB_GETLBTEXT, sel, (LPARAM)model);

    std::string user_utf8 = w8utf(user_msg);
    g_hist.push_back({"user", user_utf8});
    if (g_hist.size() > 50) g_hist.erase(g_hist.begin());

    add_chat_line(L"You", user_msg.c_str());

    std::wstring cname = get_char_name();
    add_chat_line(cname.c_str(), L"");

    std::string messages = "[";
    if (wcslen(g_cfg_sysprompt) > 0) {
        messages += "{\"role\":\"system\",\"content\":\"" + esc_json(w8utf(g_cfg_sysprompt)) + "\"}";
    }
    for (size_t i = 0; i < g_hist.size(); i++) {
        if (i > 0 || wcslen(g_cfg_sysprompt) > 0) messages += ",";
        messages += "{\"role\":\"" + g_hist[i].first + "\",\"content\":\"" + esc_json(g_hist[i].second) + "\"}";
    }
    messages += "]";

    char temp_s[32];
    snprintf(temp_s, sizeof(temp_s), "%.2f", g_cfg_temp);

    std::string model_utf8 = w8utf(model);
    std::string body = "{\"model\":\"" + model_utf8
        + "\",\"messages\":" + messages
        + ",\"stream\":true,\"options\":{\"temperature\":" + temp_s
        + ",\"num_ctx\":4096,\"repeat_penalty\":1.1,\"num_predict\":512}}";

    EnableWindow(GetDlgItem(hwnd, IDC_SEND), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_STOP), TRUE);
    SetWindowTextW(g_status, L"\u0413\u0435\u043d\u0435\u0440\u0430\u0446\u0438\u044f...");

    ChatData *d = new ChatData;
    d->body = body;
    d->hwnd = hwnd;
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

LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        g_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HWND h;

        h = CreateWindowW(L"STATIC", L"Model:", WS_CHILD|WS_VISIBLE, 10, 10, 50, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_combo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            65, 7, 350, 200, w, (HMENU)IDC_COMBO, 0, 0);
        SendMessageW(g_combo, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"\u041e\u0431\u043d\u043e\u0432\u0438\u0442\u044c", WS_CHILD|WS_VISIBLE, 420, 7, 70, 25, w, (HMENU)1001, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        g_chat = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|
            ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_HSCROLL,
            10, 40, 585, 440, w, (HMENU)IDC_CHAT, 0, 0);
        SendMessageW(g_chat, WM_SETFONT, (WPARAM)g_font, 0);
        SendMessageW(g_chat, EM_SETLIMITTEXT, 4*1024*1024, 0);

        g_input = CreateWindowW(L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,
            10, 490, 500, 25, w, (HMENU)IDC_INPUT, 0, 0);
        SendMessageW(g_input, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"Send", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
            520, 488, 50, 28, w, (HMENU)IDC_SEND, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD|WS_VISIBLE,
            580, 488, 45, 28, w, (HMENU)IDC_STOP, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);

        g_status = CreateWindowW(L"STATIC", L"Ready", WS_CHILD|WS_VISIBLE,
            10, 525, 620, 20, w, (HMENU)IDC_STATUS, 0, 0);
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
        PostMessageW(w, WM_SETFOCUS, 0, 0);
        break;
    }

    case WM_SETFOCUS:
        SetFocus(g_input);
        break;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(w, SW_RESTORE);
            SetForegroundWindow(w);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SEND:
            do_send();
            break;
        case IDC_STOP:
            g_stop = 1;
            EnableWindow(GetDlgItem(w, IDC_SEND), TRUE);
            EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);
            SetWindowTextW(g_status, L"Stopped");
            break;
        case 1001:
            refresh_models();
            SetWindowTextW(g_status, L"\u041e\u0431\u043d\u043e\u0432\u043b\u0435\u043d\u043e");
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
        break;
    }

    case WM_STAT: {
        wchar_t *st = (wchar_t *)lp;
        if (st) { SetWindowTextW(g_status, st); free(st); }
        break;
    }

    case WM_SIZE: {
        int W = LOWORD(lp), H = HIWORD(lp);
        if (W < 200 || H < 200) break;
        MoveWindow(g_chat, 10, 40, W - 20, H - 115, TRUE);
        MoveWindow(g_input, 10, H - 65, W - 130, 25, TRUE);
        MoveWindow(GetDlgItem(w, IDC_SEND), W - 115, H - 67, 55, 28, TRUE);
        MoveWindow(GetDlgItem(w, IDC_STOP), W - 55, H - 67, 45, 28, TRUE);
        MoveWindow(g_status, 10, H - 30, W - 20, 20, TRUE);
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 450;
        mm->ptMinTrackSize.y = 350;
        break;
    }

    case WM_CLOSE:
        ShowWindow(w, SW_HIDE);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        DeleteObject(g_font);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE hP, LPWSTR cL, int sH) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    load_config();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GoidaAIClient";
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, L"GoidaAIClient", L"GOIDA AI",
        WS_OVERLAPPEDWINDOW, (sx - 650) / 2, (sy - 580) / 2, 650, 580,
        NULL, NULL, hI, NULL);
    ShowWindow(hwnd, sH);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == g_input) {
            do_send();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
