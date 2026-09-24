// ===== GOIDA AI MANAGER - Train Prompt v0.3 =====
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "core_shared.h"

#pragma comment(lib, "comctl32.lib")

#define GOIDA_VERSION L"0.5.0"
#define IDC_PROMPT_IN   4001
#define IDC_PROMPT_GEN  4002
#define IDC_PROMPT_OUT  4003
#define IDC_PROMPT_SAVE 4004
#define IDC_PROMPT_LOAD 4005
#define IDC_PROMPT_LIST 4006
#define IDC_TEMP_SLIDER 4007
#define IDC_TEMP_VAL    4008
#define IDC_SYSTEM_IN   4009
#define IDC_TOKEN_CT    4010
#define IDC_BTN_REPORT  4011
#define IDC_BTN_CLEAR   4012
#define WM_GEN_DONE     (WM_APP + 60)

HINSTANCE g_hInst; HWND g_hWnd;
GoConfig g_cfg; GoLogger g_log;
HWND g_master = NULL;

void SetOut(HWND h, const wchar_t* t) { SetDlgItemText(h, IDC_PROMPT_OUT, t); }
void AppendOut(HWND h, const wchar_t* t) {
    wchar_t buf[8192]; GetDlgItemText(h, IDC_PROMPT_OUT, buf, 8192);
    wcscat(buf, t); SetDlgItemText(h, IDC_PROMPT_OUT, buf);
}

struct GenThreadData {
    std::wstring url, body; HWND hwnd;
};

DWORD WINAPI GenThread(LPVOID lp) {
    GenThreadData* d = (GenThreadData*)lp;
    std::wstring result = HttpPost(d->url, d->body, 20000);
    PostMessage(d->hwnd, WM_GEN_DONE, 0, (LPARAM)new std::wstring(result));
    delete d;
    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_master = IpcFindMaster();

            int x = 12, y = 10;
            CreateWindowEx(0, L"STATIC", L"System prompt:", WS_CHILD | WS_VISIBLE,
                x, y, 120, 20, hWnd, NULL, g_hInst, NULL); y += 22;
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT",
                L"You are a helpful AI assistant. Answer concisely and accurately.",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                x, y, 560, 24, hWnd, (HMENU)IDC_SYSTEM_IN, g_hInst, NULL); y += 32;

            CreateWindowEx(0, L"STATIC", L"Prompt:", WS_CHILD | WS_VISIBLE,
                x, y, 120, 20, hWnd, NULL, g_hInst, NULL); y += 22;
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"Write a short story about...",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
                x, y, 560, 100, hWnd, (HMENU)IDC_PROMPT_IN, g_hInst, NULL); y += 108;

            // Buttons row
            CreateWindowEx(0, L"BUTTON", L"Generate",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x, y, 100, 28, hWnd, (HMENU)IDC_PROMPT_GEN, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Save",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x + 108, y, 80, 28, hWnd, (HMENU)IDC_PROMPT_SAVE, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Load",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x + 196, y, 80, 28, hWnd, (HMENU)IDC_PROMPT_LOAD, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Clear",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x + 284, y, 80, 28, hWnd, (HMENU)IDC_BTN_CLEAR, g_hInst, NULL);
            CreateWindowEx(0, L"BUTTON", L"Report",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x + 372, y, 80, 28, hWnd, (HMENU)IDC_BTN_REPORT, g_hInst, NULL);
            y += 36;

            // Temperature slider
            CreateWindowEx(0, L"STATIC", L"Temp: 0.70", WS_CHILD | WS_VISIBLE,
                x, y, 100, 22, hWnd, (HMENU)IDC_TEMP_VAL, g_hInst, NULL);
            CreateWindowEx(0, L"TRACKBAR", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_TOOLTIPS,
                x + 100, y, 200, 26, hWnd, (HMENU)IDC_TEMP_SLIDER, g_hInst, NULL);
            SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_SETRANGE, 1, MAKELPARAM(0, 100));
            SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_SETPOS, 1, 70);
            SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_SETPAGESIZE, 0, 10);
            SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_SETLINESIZE, 0, 5);

            // Tokens
            CreateWindowEx(0, L"STATIC", L"Tokens:", WS_CHILD | WS_VISIBLE,
                x + 320, y, 60, 22, hWnd, NULL, g_hInst, NULL);
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"4096",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                x + 380, y, 80, 24, hWnd, (HMENU)IDC_TOKEN_CT, g_hInst, NULL);
            y += 34;

            // Output
            CreateWindowEx(0, L"STATIC", L"Output:", WS_CHILD | WS_VISIBLE,
                x, y, 120, 20, hWnd, NULL, g_hInst, NULL); y += 20;
            CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                x, y, 560, 120, hWnd, (HMENU)IDC_PROMPT_OUT, g_hInst, NULL); y += 128;

            // Saved list
            CreateWindowEx(0, L"STATIC", L"Saved prompts:", WS_CHILD | WS_VISIBLE,
                x, y, 120, 20, hWnd, NULL, g_hInst, NULL); y += 18;
            HWND lb = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                x, y, 560, 70, hWnd, (HMENU)IDC_PROMPT_LIST, g_hInst, NULL);

            std::wifstream fl(L"global/prompts.dat");
            std::wstring line;
            while (std::getline(fl, line)) { if (!line.empty()) SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)line.c_str()); }

            if (g_master) IpcSendLog(g_master, L"TRAIN", L"Started");
            break;
        }
        case WM_HSCROLL: {
            HWND s = (HWND)lParam;
            if (s == GetDlgItem(hWnd, IDC_TEMP_SLIDER)) {
                int pos = (int)SendMessage(s, TBM_GETPOS, 0, 0);
                wchar_t buf[16]; swprintf(buf, L"Temp: %.2f", pos / 100.0f);
                SetDlgItemText(hWnd, IDC_TEMP_VAL, buf);
            }
            break;
        }
        case WM_GEN_DONE: {
            std::wstring* r = (std::wstring*)lParam;
            if (r) {
                if (r->find(L"ERR:") == 0) { SetOut(hWnd, (L"Error: " + *r).c_str()); }
                else {
                    std::wstring t = JsonGetString(*r, L"response");
                    if (t.empty()) t = JsonGetString(*r, L"content");
                    if (t.empty()) t = L"(empty response)\n" + *r;
                    SetOut(hWnd, t.c_str());
                }
                delete r;
            }
            EnableWindow(GetDlgItem(hWnd, IDC_PROMPT_GEN), TRUE);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_PROMPT_GEN) {
                wchar_t sys[1024], prompt[4096];
                GetDlgItemText(hWnd, IDC_SYSTEM_IN, sys, 1024);
                GetDlgItemText(hWnd, IDC_PROMPT_IN, prompt, 4096);
                int pos = (int)SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_GETPOS, 0, 0);
                wchar_t tokens[16]; GetDlgItemText(hWnd, IDC_TOKEN_CT, tokens, 16);
                int maxT = _wtoi(tokens); if (maxT <= 0) maxT = 4096;

                std::wstring fullPrompt = std::wstring(sys) + L"\n\n" + prompt;
                std::wstring jsonBody = L"{\"model\":\"" + JsonEscape(g_cfg.model) +
                    L"\",\"prompt\":\"" + JsonEscape(fullPrompt) +
                    L"\",\"stream\":false,\"options\":{\"temperature\":" +
                    std::to_wstring(pos / 100.0f) + L",\"num_predict\":" +
                    std::to_wstring(maxT) + L"}}";

                SetOut(hWnd, L"Generating...");
                EnableWindow(GetDlgItem(hWnd, IDC_PROMPT_GEN), FALSE);
                GenThreadData* d = new GenThreadData{ g_cfg.api_url, jsonBody, hWnd };
                CreateThread(NULL, 0, GenThread, d, 0, NULL);
            }
            else if (id == IDC_PROMPT_SAVE) {
                wchar_t buf[4096]; GetDlgItemText(hWnd, IDC_PROMPT_IN, buf, 4096);
                if (wcslen(buf) > 0) {
                    std::wofstream f(L"global/prompts.dat", std::ios::app);
                    if (f.is_open()) { f << buf << L"\n"; f.close(); }
                    SendMessage(GetDlgItem(hWnd, IDC_PROMPT_LIST), LB_ADDSTRING, 0, (LPARAM)buf);
                }
            }
            else if (id == IDC_PROMPT_LOAD) {
                HWND lb = GetDlgItem(hWnd, IDC_PROMPT_LIST);
                int sel = (int)SendMessage(lb, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    wchar_t buf[4096]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf);
                    SetDlgItemText(hWnd, IDC_PROMPT_IN, buf);
                }
            }
            else if (id == IDC_BTN_CLEAR) { SetOut(hWnd, L""); }
            else if (id == IDC_BTN_REPORT) {
                wchar_t sys[1024], prompt[4096];
                GetDlgItemText(hWnd, IDC_SYSTEM_IN, sys, 1024);
                GetDlgItemText(hWnd, IDC_PROMPT_IN, prompt, 4096);
                int pos = (int)SendMessage(GetDlgItem(hWnd, IDC_TEMP_SLIDER), TBM_GETPOS, 0, 0);
                wchar_t tokens[16]; GetDlgItemText(hWnd, IDC_TOKEN_CT, tokens, 16);
                wchar_t buf[8192]; swprintf(buf,
                    L"=== TRAINING REPORT ===\r\n"
                    L"System: %s\r\nPrompt: %s\r\nTemp: %.2f\r\nTokens: %s\r\n"
                    L"Model: %s\r\nAPI: %s\r\n======================",
                    sys, prompt, pos/100.0f, tokens, g_cfg.model.c_str(), g_cfg.api_url.c_str());
                SetOut(hWnd, buf);
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam; SetBkColor(hdc, RGB(30, 25, 35)); SetTextColor(hdc, RGB(200, 190, 210));
            return (LRESULT)CreateSolidBrush(RGB(30, 25, 35));
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam; SetBkColor(hdc, RGB(22, 20, 28)); SetTextColor(hdc, RGB(210, 200, 220));
            return (LRESULT)CreateSolidBrush(RGB(22, 20, 28));
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
            RECT r; GetClientRect(hWnd, &r);
            HBRUSH bg = CreateSolidBrush(RGB(30, 25, 35));
            FillRect(hdc, &r, bg); DeleteObject(bg);
            EndPaint(hWnd, &ps); break;
        }
        case WM_DESTROY: {
            if (g_master) IpcSendLog(g_master, L"TRAIN", L"Shutdown");
            PostQuitMessage(0); break;
        }
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst; InitCommonControls();
    CreateDirectoryW(L"global", NULL);
    g_cfg.Load(L"global/config.dat");
    g_log.Open(L"global/logs.txt");

    WNDCLASS wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = WND_TRAIN; wc.hbrBackground = CreateSolidBrush(RGB(30, 25, 35));
    RegisterClass(&wc);

    RECT wr = {0, 0, 588, 460}; AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(0, WND_TRAIN, L"GOIDA Prompt Trainer v" GOIDA_VERSION,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 200, 200,
        wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 0;
    ShowWindow(g_hWnd, nShow); UpdateWindow(g_hWnd);

    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
