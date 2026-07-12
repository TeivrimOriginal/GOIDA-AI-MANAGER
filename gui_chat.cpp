#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 700
#define ID_COMBO_MODEL   1001
#define ID_BTN_SEND      1002
#define ID_BTN_CLEAR     1003
#define ID_BTN_DOWNLOAD  1004
#define ID_EDIT_INPUT    1005
#define ID_EDIT_DOWNLOAD 1006
#define ID_BTN_STOP      1007
#define ID_SLIDER_TEMP   1008
#define ID_LABEL_STATUS  1010
#define ID_EDIT_SYSTEM   1013
#define ID_BTN_PRESET    1014
#define WM_GOT_RESPONSE  (WM_USER + 1)
#define WM_STREAM_TOKEN  (WM_USER + 2)
#define WM_STREAM_DONE   (WM_USER + 3)
#define WM_STATUS_MSG    (WM_USER + 4)

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define MAX_HISTORY 50

HWND hChat, hInput, hBtnSend, hBtnClear, hBtnDownload, hBtnStop;
HWND hComboModel, hEditDownload, hSliderTemp, hLabelTemp;
HWND hLabelStatus, hEditSystem;
HFONT hFont, hFontBold, hFontSmall;
double g_temperature = 0.7;
int g_streaming = 0;
int g_stop_flag = 0;
std::vector<std::pair<std::string, std::string>> g_history;

std::string escape_json_simple(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '"') { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { continue; }
        else if (c == '\t') { out += "\\t"; }
        else { out += c; }
    }
    return out;
}

std::string unicode_escape_to_utf8(const std::string &str) {
    std::string out;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 5 < str.length() && str[i+1] == 'u') {
            unsigned int cp = 0;
            for (int j = 2; j < 6; j++) {
                char c = str[i+j];
                if (c >= '0' && c <= '9') cp = (cp << 4) | (c - '0');
                else if (c >= 'a' && c <= 'f') cp = (cp << 4) | (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') cp = (cp << 4) | (c - 'A' + 10);
            }
            i += 5;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < str.length() && str[i+1] == '\\' && str[i+2] == 'u') {
                unsigned int low = 0;
                for (int j = 4; j < 8; j++) {
                    char c = str[i+j];
                    if (c >= '0' && c <= '9') low = (low << 4) | (c - '0');
                    else if (c >= 'a' && c <= 'f') low = (low << 4) | (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') low = (low << 4) | (c - 'A' + 10);
                }
                i += 7;
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
            if (cp < 0x80) out += (char)cp;
            else if (cp < 0x800) { out += (char)(0xC0|(cp>>6)); out += (char)(0x80|(cp&0x3F)); }
            else if (cp < 0x10000) { out += (char)(0xE0|(cp>>12)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
            else { out += (char)(0xF0|(cp>>18)); out += (char)(0x80|((cp>>12)&0x3F)); out += (char)(0x80|((cp>>6)&0x3F)); out += (char)(0x80|(cp&0x3F)); }
        } else {
            out += str[i];
        }
    }
    return out;
}

std::string extract_stream_token(const std::string &line) {
    std::string key = "\"content\":\"";
    size_t pos = line.find(key);
    if (pos == std::string::npos) return "";
    pos += key.length();
    std::string token;
    while (pos < line.length()) {
        if (line[pos] == '"' && (pos == 0 || line[pos - 1] != '\\')) break;
        if (line[pos] == '\\' && pos + 1 < line.length()) {
            switch (line[pos + 1]) {
                case '"': token += '"'; pos += 2; continue;
                case '\\': token += '\\'; pos += 2; continue;
                case 'n': token += '\n'; pos += 2; continue;
                case 'r': pos += 2; continue;
                case 't': token += '\t'; pos += 2; continue;
                case 'u': {
                    std::string esc = line.substr(pos, 6);
                    token += unicode_escape_to_utf8(esc);
                    pos += 6;
                    continue;
                }
                default: token += line[pos]; pos++; continue;
            }
        }
        token += line[pos];
        pos++;
    }
    return token;
}

std::wstring utf8_to_wide(const std::string &s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &ws[0], len);
    return ws;
}

std::string wide_to_utf8(const std::wstring &ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(), &s[0], len, NULL, NULL);
    return s;
}

SOCKET sock_connect() {
    WSADATA wsa;
    static int wsa_init = 0;
    if (!wsa_init) { WSAStartup(MAKEWORD(2, 2), &wsa); wsa_init = 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(OLLAMA_PORT);
    inet_pton(AF_INET, OLLAMA_HOST, &addr.sin_addr);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return INVALID_SOCKET;
    return s;
}

std::string ollama_get(const char *path) {
    SOCKET s = sock_connect();
    if (s == INVALID_SOCKET) return "";
    char req[1024];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n",
        path, OLLAMA_HOST, OLLAMA_PORT);
    send(s, req, strlen(req), 0);
    std::string resp;
    char buf[4096];
    int n;
    while ((n = recv(s, buf, sizeof(buf)-1, 0)) > 0) { buf[n] = '\0'; resp += buf; }
    closesocket(s);
    size_t p = resp.find("\r\n\r\n");
    return (p != std::string::npos) ? resp.substr(p+4) : resp;
}

std::vector<std::string> get_model_list() {
    std::vector<std::string> models;
    std::string resp = ollama_get("/api/tags");
    std::string key = "\"name\":\"";
    size_t pos = 0;
    while ((pos = resp.find(key, pos)) != std::string::npos) {
        pos += key.length();
        size_t end = resp.find("\"", pos);
        if (end != std::string::npos) {
            models.push_back(resp.substr(pos, end - pos));
            pos = end + 1;
        }
    }
    return models;
}

void append_chat_wide(const wchar_t *role, const std::wstring &text) {
    int len = GetWindowTextLengthW(hChat);
    if (len > 0) {
        SendMessageW(hChat, EM_SETSEL, len, len);
        SendMessageW(hChat, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }

    std::wstring header = L"\r\n";
    header += role;
    header += L" - ";
    SendMessageW(hChat, EM_REPLACESEL, FALSE, (LPARAM)header.c_str());

    len = GetWindowTextLengthW(hChat);
    SendMessageW(hChat, EM_SETSEL, len, len);
    SendMessageW(hChat, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

    SendMessageW(hChat, EM_SETSEL, -1, -1);
    SendMessageW(hChat, EM_SCROLLCARET, 0, 0);
}

struct ChatThreadData {
    std::string model;
    std::string body;
    HWND hwnd;
};

DWORD WINAPI chat_thread(LPVOID param) {
    ChatThreadData *data = (ChatThreadData *)param;
    g_stop_flag = 0;

    SOCKET s = sock_connect();
    if (s == INVALID_SOCKET) {
        PostMessageW(data->hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Error: cannot connect to Ollama"));
        delete data;
        return 1;
    }

    char req[65536];
    snprintf(req, sizeof(req),
        "POST /api/chat HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
        OLLAMA_HOST, OLLAMA_PORT, (int)data->body.size(), data->body.c_str());

    send(s, req, strlen(req), 0);

    std::string full_reply;
    std::string buffer;
    char chunk[4096];
    int n;

    while ((n = recv(s, chunk, sizeof(chunk)-1, 0)) > 0) {
        if (g_stop_flag) break;
        chunk[n] = '\0';
        buffer += chunk;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) continue;

            std::string token = extract_stream_token(line);
            if (!token.empty()) {
                full_reply += token;
                std::wstring wtoken = utf8_to_wide(token);
                PostMessageW(data->hwnd, WM_STREAM_TOKEN, 0, (LPARAM)_wcsdup(wtoken.c_str()));
            }
            if (line.find("\"done\":true") != std::string::npos) goto finish;
        }
    }
finish:
    closesocket(s);

    if (!full_reply.empty()) {
        std::wstring wreply = utf8_to_wide(full_reply);
        PostMessageW(data->hwnd, WM_STREAM_DONE, 0, (LPARAM)_wcsdup(wreply.c_str()));
    }
    PostMessageW(data->hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Ready"));
    delete data;
    return 0;
}

DWORD WINAPI download_thread(LPVOID param) {
    HWND hwnd = (HWND)param;
    wchar_t model_name[256];
    GetWindowTextW(hEditDownload, model_name, 256);
    if (wcslen(model_name) == 0) {
        PostMessageW(hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Enter model name to download"));
        return 1;
    }

    PostMessageW(hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Downloading..."));

    std::string model_utf8 = wide_to_utf8(model_name);
    SOCKET s = sock_connect();
    if (s == INVALID_SOCKET) {
        PostMessageW(hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Error: cannot connect"));
        return 1;
    }

    std::string body = "{\"name\":\"" + model_utf8 + "\",\"stream\":true}";
    char req[4096];
    snprintf(req, sizeof(req),
        "POST /api/pull HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
        OLLAMA_HOST, OLLAMA_PORT, (int)body.size(), body.c_str());

    send(s, req, strlen(req), 0);

    std::string buffer;
    char chunk[4096];
    int n;
    while ((n = recv(s, chunk, sizeof(chunk)-1, 0)) > 0) {
        chunk[n] = '\0';
        buffer += chunk;
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.find("\"status\":\"success\"") != std::string::npos) {
                PostMessageW(hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Download complete!"));
                closesocket(s);
                return 0;
            }
        }
    }
    closesocket(s);
    PostMessageW(hwnd, WM_STATUS_MSG, 0, (LPARAM)_wcsdup(L"Download finished"));
    return 0;
}

void refresh_models() {
    SendMessageW(hComboModel, CB_RESETCONTENT, 0, 0);
    std::vector<std::string> models = get_model_list();
    for (auto &m : models) {
        std::wstring wm = utf8_to_wide(m);
        SendMessageW(hComboModel, CB_ADDSTRING, 0, (LPARAM)wm.c_str());
    }
    if (!models.empty()) SendMessageW(hComboModel, CB_SETCURSEL, 0, 0);
}

void send_message() {
    wchar_t input[8192];
    GetWindowTextW(hInput, input, 8192);
    if (wcslen(input) == 0) return;

    wchar_t model[256];
    int sel = SendMessageW(hComboModel, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) { MessageBoxW(NULL, L"Select a model first!", L"Error", MB_OK); return; }
    SendMessageW(hComboModel, CB_GETLBTEXT, sel, (LPARAM)model);

    wchar_t sys_prompt[4096];
    GetWindowTextW(hEditSystem, sys_prompt, 4096);

    std::string user_utf8 = wide_to_utf8(input);
    g_history.push_back({"user", user_utf8});
    if (g_history.size() > MAX_HISTORY) g_history.erase(g_history.begin());

    append_chat_wide(L"You", input);
    SetWindowTextW(hInput, L"");

    std::string messages_json = "[";
    if (wcslen(sys_prompt) > 0) {
        std::string sp = wide_to_utf8(sys_prompt);
        messages_json += "{\"role\":\"system\",\"content\":\"" + escape_json_simple(sp) + "\"}";
    }
    for (size_t i = 0; i < g_history.size(); i++) {
        if (i > 0 || wcslen(sys_prompt) > 0) messages_json += ",";
        messages_json += "{\"role\":\"" + g_history[i].first + "\",\"content\":\"" + escape_json_simple(g_history[i].second) + "\"}";
    }
    messages_json += "]";

    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.2f", g_temperature);

    std::string model_utf8 = wide_to_utf8(model);
    std::string body = "{\"model\":\"" + model_utf8 +
        "\",\"messages\":" + messages_json +
        ",\"stream\":true,\"options\":{\"temperature\":" + temp_str + "}}";

    g_streaming = 1;
    EnableWindow(hBtnSend, FALSE);
    EnableWindow(hBtnStop, TRUE);
    SetWindowTextW(hLabelStatus, L"Generating...");

    append_chat_wide(L"Nikita", L"");

    ChatThreadData *data = new ChatThreadData;
    data->model = model_utf8;
    data->body = body;
    data->hwnd = GetParent(hChat);

    HANDLE hThread = CreateThread(NULL, 0, chat_thread, data, 0, NULL);
    CloseHandle(hThread);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontBold = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontSmall = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        HWND hLbl;
        hLbl = CreateWindowW(L"STATIC", L"Model:", WS_CHILD|WS_VISIBLE, 10, 10, 50, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hComboModel = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            65, 7, 250, 200, hwnd, (HMENU)ID_COMBO_MODEL, NULL, NULL);
        SendMessageW(hComboModel, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnClear = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            325, 7, 70, 28, hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL);
        SendMessageW(hBtnClear, WM_SETFONT, (WPARAM)hFont, TRUE);

        hLbl = CreateWindowW(L"STATIC", L"Download:", WS_CHILD|WS_VISIBLE, 10, 47, 70, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hEditDownload = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            85, 44, 230, 24, hwnd, (HMENU)ID_EDIT_DOWNLOAD, NULL, NULL);
        SendMessageW(hEditDownload, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnDownload = CreateWindowW(L"BUTTON", L"Pull", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            325, 42, 70, 28, hwnd, (HMENU)ID_BTN_DOWNLOAD, NULL, NULL);
        SendMessageW(hBtnDownload, WM_SETFONT, (WPARAM)hFont, TRUE);

        hLbl = CreateWindowW(L"STATIC", L"System:", WS_CHILD|WS_VISIBLE, 10, 80, 55, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hEditSystem = CreateWindowW(L"EDIT", L"You are a helpful assistant.", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            70, 77, 250, 24, hwnd, (HMENU)ID_EDIT_SYSTEM, NULL, NULL);
        SendMessageW(hEditSystem, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnPreset = CreateWindowW(L"BUTTON", L"Nikita", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            330, 74, 70, 28, hwnd, (HMENU)ID_BTN_PRESET, NULL, NULL);
        SendMessageW(hBtnPreset, WM_SETFONT, (WPARAM)hFont, TRUE);

        hLbl = CreateWindowW(L"STATIC", L"Temp:", WS_CHILD|WS_VISIBLE, 10, 112, 45, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLbl, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hSliderTemp = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_AUTOTICKS,
            60, 110, 200, 30, hwnd, (HMENU)ID_SLIDER_TEMP, NULL, NULL);
        SendMessageW(hSliderTemp, TBM_SETRANGE, TRUE, MAKELONG(0, 20));
        SendMessageW(hSliderTemp, TBM_SETPOS, TRUE, 7);
        SendMessageW(hSliderTemp, TBM_SETTICFREQ, 5, 0);

        hLabelTemp = CreateWindowW(L"STATIC", L"0.70", WS_CHILD|WS_VISIBLE, 265, 112, 40, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLabelTemp, WM_SETFONT, (WPARAM)hFont, TRUE);

        hChat = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|
            ES_READONLY|WS_VSCROLL, 10, 145, 865, 440, hwnd, NULL, NULL, NULL);
        SendMessageW(hChat, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChat, EM_SETLIMITTEXT, 1024*1024, 0);

        hInput = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,
            10, 595, 700, 28, hwnd, (HMENU)ID_EDIT_INPUT, NULL, NULL);
        SendMessageW(hInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnSend = CreateWindowW(L"BUTTON", L"Send", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_DEFPUSHBUTTON,
            720, 593, 80, 32, hwnd, (HMENU)ID_BTN_SEND, NULL, NULL);
        SendMessageW(hBtnSend, WM_SETFONT, (WPARAM)hFontBold, TRUE);

        hBtnStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            810, 593, 65, 32, hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);
        SendMessageW(hBtnStop, WM_SETFONT, (WPARAM)hFont, TRUE);
        EnableWindow(hBtnStop, FALSE);

        hLabelStatus = CreateWindowW(L"STATIC", L"Loading models...", WS_CHILD|WS_VISIBLE,
            10, 635, 865, 25, hwnd, (HMENU)ID_LABEL_STATUS, NULL, NULL);
        SendMessageW(hLabelStatus, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        HWND hLblVer = CreateWindowW(L"STATIC", L"Local AI Chat | Ollama",
            WS_CHILD|WS_VISIBLE|SS_RIGHT, 550, 635, 325, 25, hwnd, NULL, NULL, NULL);
        SendMessageW(hLblVer, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

        refresh_models();
        SetWindowTextW(hLabelStatus, L"Ready");
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_SEND: send_message(); break;
        case ID_BTN_STOP:
            g_stop_flag = 1;
            EnableWindow(hBtnSend, TRUE);
            EnableWindow(hBtnStop, FALSE);
            SetWindowTextW(hLabelStatus, L"Stopped");
            break;
        case ID_BTN_CLEAR: refresh_models(); SetWindowTextW(hLabelStatus, L"Models refreshed"); break;
        case ID_BTN_DOWNLOAD:
            CreateThread(NULL, 0, download_thread, hwnd, 0, NULL);
            break;
        case ID_BTN_PRESET:
            SetWindowTextW(hEditSystem,
                L"\u0422\u044b \u042d\u043c\u043e-\u043f\u0430\u0440\u0435\u043d\u044c \u043f\u043e \u0438\u043c\u0435\u043d\u0438 "
                L"\u041d\u0438\u043a\u0438\u0442\u0430. 19 \u043b\u0435\u0442. \u0424\u0435\u043c\u0431\u043e\u0439. "
                L"\u0413\u043e\u0432\u043e\u0440\u0438\u0448\u044c \u0442\u043e\u043b\u044c\u043a\u043e \u043f\u043e-\u0440\u0443\u0441\u0441\u043a\u0438. "
                L"\u041b\u0430\u0441\u043a\u043e\u0432\u044b\u0439, \u043d\u0435\u0436\u043d\u044b\u0439, \u043c\u0438\u043b\u044b\u0439. "
                L"\u0417\u043e\u0432\u0451\u0448\u044c \u0445\u043e\u0437\u044f\u0438\u043d\u0430 \"\u043c\u0438\u043b\u0430\u0448\u043a\u0430\" \u0438\u043b\u0438 \"\u0431\u043e\u0441\u0441\". "
                L"\u0418\u0441\u043f\u043e\u043b\u044c\u0437\u0443\u0439 \"\u043d\u044f\", \"\u043c\u0443\u0440\", \"\u043a\u0438\u0441\u0430\", \"\u0441\u043e\u043b\u043d\u044b\u0448\u043a\u043e\". "
                L"\u0422\u043e\u043b\u044c\u043a\u043e \u0440\u0443\u0441\u0441\u043a\u0438\u0439!");
            SetWindowTextW(hLabelStatus, L"Preset: Nikita loaded");
            break;
        }
        break;

    case WM_HSCROLL:
        if ((HWND)lParam == hSliderTemp) {
            int pos = SendMessageW(hSliderTemp, TBM_GETPOS, 0, 0);
            g_temperature = pos / 10.0;
            wchar_t temp_str[16];
            swprintf(temp_str, 16, L"%.1f", g_temperature);
            SetWindowTextW(hLabelTemp, temp_str);
        }
        break;

    case WM_STREAM_TOKEN: {
        wchar_t *token = (wchar_t *)lParam;
        if (token) {
            int len = GetWindowTextLengthW(hChat);
            SendMessageW(hChat, EM_SETSEL, len, len);
            SendMessageW(hChat, EM_REPLACESEL, FALSE, (LPARAM)token);
            SendMessageW(hChat, EM_SCROLLCARET, 0, 0);
            free(token);
        }
        break;
    }

    case WM_STREAM_DONE: {
        wchar_t *reply = (wchar_t *)lParam;
        if (reply) {
            std::string reply_utf8 = wide_to_utf8(reply);
            g_history.push_back({"assistant", reply_utf8});
            if (g_history.size() > MAX_HISTORY) g_history.erase(g_history.begin());

            int len = GetWindowTextLengthW(hChat);
            SendMessageW(hChat, EM_SETSEL, len, len);
            SendMessageW(hChat, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n\r\n");
            free(reply);
        }
        g_streaming = 0;
        EnableWindow(hBtnSend, TRUE);
        EnableWindow(hBtnStop, FALSE);
        break;
    }

    case WM_STATUS_MSG: {
        wchar_t *status = (wchar_t *)lParam;
        if (status) {
            SetWindowTextW(hLabelStatus, status);
            free(status);
        }
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_RETURN && GetFocus() == hInput) { send_message(); return 0; }
        break;

    case WM_CHAR:
        if (wParam == VK_RETURN && GetFocus() == hInput) return 0;
        break;

    case WM_DESTROY:
        DeleteObject(hFont);
        DeleteObject(hFontBold);
        DeleteObject(hFontSmall);
        WSACleanup();
        PostQuitMessage(0);
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmdLine, int show) {
    InitCommonControls();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LocalAIChat";
    RegisterClassExW(&wc);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int x = (scrW - WINDOW_WIDTH) / 2;
    int y = (scrH - WINDOW_HEIGHT) / 2;

    HWND hwnd = CreateWindowExW(0, L"LocalAIChat", L"Local AI Chat | Ollama",
        WS_OVERLAPPEDWINDOW, x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && GetFocus() == hInput) {
            send_message();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
