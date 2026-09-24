// ===== GOIDA AI MANAGER - Client v0.5 =====
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "core_shared.h"

#pragma comment(lib, "comctl32.lib")

#define GOIDA_VERSION L"0.5.0"
#define IDC_CHAT_HIST   3001
#define IDC_CHAT_INPUT  3002
#define IDC_CHAT_SEND   3003
#define IDC_CHAT_CLEAR  3004
#define IDC_STATUS_LBL  3005
#define IDC_API_URL     3006
#define IDC_MODEL_SEL   3007
#define IDC_BTN_CONNECT 3008
#define IDC_BTN_SAVE    3009
#define IDC_BTN_LOAD    3010
#define WM_HTTP_CHUNK   (WM_APP + 50)
#define WM_HTTP_DONE    (WM_APP + 51)
#define WM_HTTP_ERR     (WM_APP + 52)

HINSTANCE g_hInst; HWND g_hWnd; HWND g_master = NULL;
GoConfig g_cfg; GoLogger g_log;
WNDPROC g_origInputProc = NULL;

std::vector<std::pair<std::wstring,std::wstring>> g_history;

void AppendChat(const wchar_t* t) {
    HWND h = GetDlgItem(g_hWnd, IDC_CHAT_HIST);
    if (!h) return;
    SendMessage(h, EM_REPLACESEL, 0, (LPARAM)t);
    int len = GetWindowTextLength(h);
    SendMessage(h, EM_SETSEL, len, len);
    SendMessage(h, EM_SCROLLCARET, 0, 0);
}
void SetSt(const wchar_t* t) { SetDlgItemText(g_hWnd, IDC_STATUS_LBL, t); }

std::wstring BuildJson(const wchar_t* model, const wchar_t* url, bool stream) {
    std::wstring msgs;
    for (auto& m : g_history)
        msgs += L"{\"role\":\"" + m.first + L"\",\"content\":\"" + JsonEscape(m.second) + L"\"},";

    std::wstring u(url);
    if (u.find(L"/api/generate") != std::wstring::npos) {
        std::wstring prompt;
        for (auto& m : g_history) prompt += m.first + L": " + m.second + L"\n";
        std::wstring s = stream ? L"true" : L"false";
        return L"{\"model\":\"" + JsonEscape(model) + L"\",\"prompt\":\"" + JsonEscape(prompt) +
            L"\",\"stream\":" + s + L",\"options\":{\"temperature\":" + std::to_wstring(g_cfg.temp) +
            L",\"num_predict\":" + std::to_wstring(g_cfg.max_tokens) + L"}}";
    } else {
        if (!msgs.empty()) msgs.pop_back();
        std::wstring s = stream ? L"true" : L"false";
        if (u.find(L"/v1/chat") != std::wstring::npos)
            return L"{\"model\":\"" + JsonEscape(model) + L"\",\"messages\":[" + msgs +
                L"],\"stream\":" + s + L",\"temperature\":" + std::to_wstring(g_cfg.temp) +
                L",\"max_tokens\":" + std::to_wstring(g_cfg.max_tokens) + L"}";
        return L"{\"model\":\"" + JsonEscape(model) + L"\",\"messages\":[" + msgs +
            L"],\"stream\":" + s + L",\"options\":{\"temperature\":" + std::to_wstring(g_cfg.temp) +
            L",\"num_predict\":" + std::to_wstring(g_cfg.max_tokens) + L"}}";
    }
}

// Streaming HTTP thread
struct HttpData { std::wstring url, body; HWND hwnd; int isStream; };

DWORD WINAPI HttpThread(LPVOID lp) {
    HttpData* d = (HttpData*)lp;
    bool isHttps = (d->url.find(L"https://") == 0);
    std::wstring host, path; int port = isHttps ? 443 : 80;
    size_t s = d->url.find(L"://"); if (s == std::wstring::npos) { PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:bad_url")); delete d; return 0; }
    size_t start = s + 3, slash = d->url.find(L'/', start);
    std::wstring hp = (slash == std::wstring::npos) ? d->url.substr(start) : d->url.substr(start, slash - start);
    path = (slash == std::wstring::npos) ? L"/" : d->url.substr(slash);
    size_t colon = hp.find(L':');
    if (colon != std::wstring::npos) { host = hp.substr(0, colon); port = _wtoi(hp.substr(colon+1).c_str()); }
    else { host = hp; }

    HINTERNET hSession = WinHttpOpen(L"GOIDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:session")); delete d; return 0; }
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:connect")); delete d; return 0; }

    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, NULL, NULL, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:request")); delete d; return 0; }

    WinHttpSetTimeouts(hReq, 30000, 30000, 30000, 30000);

    std::string utf8Body;
    int len = WideCharToMultiByte(CP_UTF8, 0, d->body.c_str(), (int)d->body.size(), NULL, 0, NULL, NULL);
    if (len > 0) { utf8Body.resize(len); WideCharToMultiByte(CP_UTF8, 0, d->body.c_str(), (int)d->body.size(), &utf8Body[0], len, NULL, NULL); }

    LPCWSTR headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hReq, headers, (DWORD)wcslen(headers),
        (LPVOID)utf8Body.data(), (DWORD)utf8Body.size(), (DWORD)utf8Body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:send")); delete d; return 0; }
    if (!WinHttpReceiveResponse(hReq, NULL)) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:recv")); delete d; return 0; }

    // Read with buffer for streaming
    std::string buf;
    std::string fullResp;
    DWORD read = 0; char tmp[4096];
    while (WinHttpReadData(hReq, tmp, sizeof(tmp)-1, &read) && read > 0) {
        tmp[read] = 0;
        if (d->isStream) {
            buf += tmp;
            // Process complete lines
            size_t nl;
            while ((nl = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, nl);
                buf.erase(0, nl + 1);
                if (!line.empty()) {
                    // Extract "response":"..." from the JSON line
                    auto rp = line.find("\"response\":\"");
                    if (rp == std::string::npos) rp = line.find("\"content\":\"");
                    if (rp != std::string::npos) {
                        rp = line.find('"', rp + 11);
                        if (rp != std::string::npos) {
                            rp++;
                            std::string token;
                            while (rp < line.size() && !(line[rp] == '"' && (rp == 0 || line[rp-1] != '\\'))) {
                                if (line[rp] == '\\' && rp+1 < line.size()) {
                                    if (line[rp+1] == 'n') token += '\n';
                                    else if (line[rp+1] == 'r') token += '\r';
                                    else if (line[rp+1] == '"') token += '"';
                                    else if (line[rp+1] == '\\') token += '\\';
                                    else token += line[rp];
                                    rp += 2;
                                } else { token += line[rp]; rp++; }
                            }
                            if (!token.empty()) {
                                int wlen = MultiByteToWideChar(CP_UTF8, 0, token.c_str(), (int)token.size(), NULL, 0);
                                if (wlen > 0) {
                                    std::wstring wtoken; wtoken.resize(wlen);
                                    MultiByteToWideChar(CP_UTF8, 0, token.c_str(), (int)token.size(), &wtoken[0], wlen);
                                    PostMessage(d->hwnd, WM_HTTP_CHUNK, 0, (LPARAM)new std::wstring(wtoken));
                                }
                            }
                            // Check if done
                            if (line.find("\"done\":true") != std::string::npos || line.find("\"done\": true") != std::string::npos) {
                                goto done;
                            }
                        }
                    }
                }
            }
        } else {
            fullResp += tmp;
        }
    }

done:
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);

    if (!d->isStream) {
        // Non-streaming: return full response
        if (fullResp.empty()) { PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:empty")); }
        else {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, fullResp.c_str(), (int)fullResp.size(), NULL, 0);
            if (wlen > 0) {
                std::wstring wres; wres.resize(wlen);
                MultiByteToWideChar(CP_UTF8, 0, fullResp.c_str(), (int)fullResp.size(), &wres[0], wlen);
                PostMessage(d->hwnd, WM_HTTP_DONE, 0, (LPARAM)new std::wstring(wres));
            }
        }
    } else {
        PostMessage(d->hwnd, WM_HTTP_DONE, 0, (LPARAM)0); // signal end
    }
    delete d;
    return 0;
}

// Accumulate streaming response
std::wstring g_streamResp;

void DoSend(HWND hWnd) {
    wchar_t buf[8192]; GetDlgItemText(hWnd, IDC_CHAT_INPUT, buf, 8192);
    if (wcslen(buf) == 0) return;

    g_history.push_back({L"user", buf});
    AppendChat(L">>> "); AppendChat(buf); AppendChat(L"\r\n<<< ");
    SetDlgItemText(hWnd, IDC_CHAT_INPUT, L"");
    g_streamResp.clear();

    wchar_t model[256], url[512];
    GetDlgItemText(hWnd, IDC_MODEL_SEL, model, 256);
    GetDlgItemText(hWnd, IDC_API_URL, url, 512);

    std::wstring jsonBody = BuildJson(model, url, true); // streaming enabled
    SetSt(L"Status: streaming...");
    EnableWindow(GetDlgItem(hWnd, IDC_CHAT_SEND), FALSE);
    EnableWindow(GetDlgItem(hWnd, IDC_CHAT_INPUT), FALSE);

    CreateThread(NULL, 0, HttpThread, new HttpData{url, jsonBody, hWnd, 1}, 0, NULL);
}

// Subclass edit to detect Enter key
LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        DoSend(GetParent(hwnd)); return 0;
    }
    return CallWindowProc(g_origInputProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_master = IpcFindMaster();

            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
                12, 12, 560, 300, hWnd, (HMENU)IDC_CHAT_HIST, g_hInst, NULL);

            HWND input = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 322, 440, 28, hWnd, (HMENU)IDC_CHAT_INPUT, g_hInst, NULL);
            g_origInputProc = (WNDPROC)SetWindowLongPtr(input, GWLP_WNDPROC, (LONG_PTR)InputProc);

            CreateWindowEx(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                460, 322, 112, 28, hWnd, (HMENU)IDC_CHAT_SEND, g_hInst, NULL);

            CreateWindowEx(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, 360, 80, 26, hWnd, (HMENU)IDC_CHAT_CLEAR, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                98, 360, 80, 26, hWnd, (HMENU)IDC_BTN_SAVE, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                184, 360, 80, 26, hWnd, (HMENU)IDC_BTN_LOAD, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                270, 360, 80, 26, hWnd, (HMENU)4000, g_hInst, NULL);

            CreateWindowEx(0, L"STATIC", L"API:", WS_CHILD | WS_VISIBLE,
                12, 396, 30, 20, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", g_cfg.api_url.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 42, 396, 200, 24, hWnd, (HMENU)IDC_API_URL, g_hInst, NULL);
            CreateWindowEx(0, L"STATIC", L"Model:", WS_CHILD | WS_VISIBLE,
                252, 396, 40, 20, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", g_cfg.model.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 292, 396, 130, 24, hWnd, (HMENU)IDC_MODEL_SEL, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                430, 396, 58, 24, hWnd, (HMENU)IDC_BTN_CONNECT, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"|<-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                492, 396, 36, 24, hWnd, (HMENU)4001, g_hInst, NULL);

            CreateWindowEx(0, L"STATIC", L"Status: ready",
                WS_CHILD | WS_VISIBLE, 12, 428, 560, 18, hWnd, (HMENU)IDC_STATUS_LBL, g_hInst, NULL);

            AppendChat(L"=== GOIDA AI CLIENT v" GOIDA_VERSION L" ===\r\n");
            AppendChat(L"API: "); AppendChat(g_cfg.api_url.c_str()); AppendChat(L"\r\n");
            AppendChat(L"Model: "); AppendChat(g_cfg.model.c_str()); AppendChat(L"\r\n");
            AppendChat(L"Enter to send (Shift+Enter = newline).\r\n\r\n");

            g_history.push_back({L"system", L"You are a helpful AI assistant."});
            if (g_master) { SetSt(L"Status: ready | master"); IpcSendLog(g_master, L"CLIENT", L"Started"); }
            break;
        }
        case WM_HTTP_CHUNK: {
            std::wstring* t = (std::wstring*)lParam;
            if (t) { AppendChat(t->c_str()); g_streamResp += *t; delete t; }
            break;
        }
        case WM_HTTP_DONE: {
            std::wstring* r = (std::wstring*)lParam;
            if (r) {
                std::wstring text = JsonGetString(*r, L"response");
                if (text.empty()) text = JsonGetString(*r, L"content");
                if (text.empty()) { // Try OpenAI choices[0].message.content
                    auto cp = r->find(L"\"content\":\"");
                    if (cp != std::wstring::npos) {
                        cp += 11; text.clear();
                        while (cp < r->size() && !((*r)[cp] == L'"' && (*r)[cp-1] != L'\\')) { text += (*r)[cp]; cp++; }
                    }
                }
                if (text.empty()) text = L"(empty)";
                AppendChat(text.c_str()); AppendChat(L"\r\n");
                g_streamResp += text;
                g_history.push_back({L"assistant", g_streamResp});
                g_streamResp.clear();
                delete r;
            } else {
                // End of streaming - save accumulated
                if (!g_streamResp.empty()) {
                    g_history.push_back({L"assistant", g_streamResp});
                    g_streamResp.clear();
                }
                AppendChat(L"\r\n");
            }
            SetSt(L"Status: ready");
            EnableWindow(GetDlgItem(hWnd, IDC_CHAT_SEND), TRUE);
            EnableWindow(GetDlgItem(hWnd, IDC_CHAT_INPUT), TRUE);
            SetFocus(GetDlgItem(hWnd, IDC_CHAT_INPUT));
            break;
        }
        case WM_HTTP_ERR: {
            std::wstring* e = (std::wstring*)lParam;
            if (e) { AppendChat(L"\r\n<<< [error] "); AppendChat(e->c_str()); AppendChat(L"\r\n"); SetSt((L"Status: error " + *e).c_str()); delete e; }
            EnableWindow(GetDlgItem(hWnd, IDC_CHAT_SEND), TRUE);
            EnableWindow(GetDlgItem(hWnd, IDC_CHAT_INPUT), TRUE);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_CHAT_SEND) DoSend(hWnd);
            else if (id == IDC_CHAT_CLEAR) {
                SetDlgItemText(hWnd, IDC_CHAT_HIST, L"");
                g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."});
                AppendChat(L"=== Cleared ===\r\n");
            }
            else if (id == IDC_BTN_SAVE) {
                std::wofstream f(L"global/chat_last.txt");
                if (f.is_open()) { for (auto& m : g_history) f << m.first << L": " << m.second << L"\n"; f.close(); AppendChat(L"--- Saved ---\r\n"); }
            }
            else if (id == IDC_BTN_LOAD) {
                std::wifstream f(L"global/chat_last.txt");
                if (f.is_open()) { SetDlgItemText(hWnd, IDC_CHAT_HIST, L""); g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."}); std::wstring line; while (std::getline(f, line)) { if (!line.empty()) { AppendChat(line.c_str()); AppendChat(L"\r\n"); } } f.close(); }
                else AppendChat(L"--- No saved chat ---\r\n");
            }
            else if (id == IDC_BTN_CONNECT) {
                wchar_t url[512], model[128]; GetDlgItemText(hWnd, IDC_API_URL, url, 512); GetDlgItemText(hWnd, IDC_MODEL_SEL, model, 128);
                g_cfg.api_url = url; g_cfg.model = model; g_cfg.Save(L"global/config.dat");
                SetSt((L"Status: configured - " + std::wstring(url)).c_str());
                AppendChat(L"--- Connected "); AppendChat(url); AppendChat(L" ---\r\n");
            }
            else if (id == 4000) {
                AppendChat(L"--- Testing API...\r\n"); SetSt(L"Status: testing...");
                EnableWindow(GetDlgItem(hWnd, IDC_CHAT_SEND), FALSE);
                wchar_t url[512]; GetDlgItemText(hWnd, IDC_API_URL, url, 512);
                std::wstring j = L"{\"model\":\"" + JsonEscape(g_cfg.model) + L"\",\"prompt\":\"ping\",\"stream\":false}";
                CreateThread(NULL, 0, HttpThread, new HttpData{url, j, hWnd, 0}, 0, NULL);
            }
            else if (id == 4001) {
                g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."});
                AppendChat(L"--- Context cleared ---\r\n");
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            static HBRUSH br = CreateSolidBrush(RGB(30, 30, 35));
            SetBkColor((HDC)wParam, RGB(30, 30, 35)); SetTextColor((HDC)wParam, RGB(200, 200, 210));
            return (LRESULT)br;
        }
        case WM_CTLCOLOREDIT: {
            static HBRUSH br = CreateSolidBrush(RGB(22, 22, 26));
            SetBkColor((HDC)wParam, RGB(22, 22, 26)); SetTextColor((HDC)wParam, RGB(210, 210, 220));
            return (LRESULT)br;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
            RECT r; GetClientRect(hWnd, &r);
            HBRUSH bg = CreateSolidBrush(RGB(30, 30, 35)); FillRect(hdc, &r, bg); DeleteObject(bg);
            EndPaint(hWnd, &ps); break;
        }
        case WM_DESTROY: { if (g_master) IpcSendLog(g_master, L"CLIENT", L"Shutdown"); PostQuitMessage(0); break; }
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst; InitCommonControls();
    CreateDirectoryW(L"global", NULL);
    g_cfg.Load(L"global/config.dat"); g_log.Open(L"global/logs.txt");

    WNDCLASS wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = WND_CLIENT; wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 35));
    RegisterClass(&wc);

    RECT wr = {0, 0, 588, 462}; AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(0, WND_CLIENT, L"GOIDA AI Client v" GOIDA_VERSION,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 150, 150,
        wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 0;
    ShowWindow(g_hWnd, nShow); UpdateWindow(g_hWnd);

    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
