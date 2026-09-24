// ===== GOIDA AI MANAGER - Launcher v0.4 =====
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "core_shared.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "urlmon.lib")

#define GOIDA_VERSION L"0.5.0"
#define IDC_NAV_FEMBOY    1001
#define IDC_NAV_CONSOLE   1002
#define IDC_NAV_MEMORY    1004
#define IDC_NAV_ABOUT     1003
#define IDC_BTN_LAUNCH_CLIENT 1012
#define IDC_BTN_LAUNCH_TRAIN  1013
#define IDC_BTN_KILL_ALL      1014
#define IDC_BTN_UPDATE        1015
#define IDC_BTN_RELOAD        1016
#define IDC_FEMBOY_NAME   2001
#define IDC_FEMBOY_SAVE   2002
#define IDC_MEMORY_LIST   2010
#define IDC_MEMORY_ADD    2011
#define IDC_MEMORY_TEXT   2012
#define IDC_MEMORY_TAG    2013
#define IDC_MEMORY_DEL    2014
#define IDC_LOG_LIST      2020
#define IDC_LOG_CLEAR     2021
#define IDC_BTN_AUTOSTART 2022
#define ID_TRAY_EXIT      3001
#define ID_TRAY_SHOW      3002
#define WM_TRAYICON       (WM_APP + 200)

HINSTANCE g_hInst; HWND g_hWnd, g_page;
GoConfig g_cfg; GoLogger g_log;
std::vector<std::wstring> g_logs;
PROCESS_INFORMATION g_procClient = {0}, g_procTrain = {0};
NOTIFYICONDATAW g_nid = {};

void ClearPage() { if (g_page) { DestroyWindow(g_page); g_page = NULL; } }

HWND MakeBtn(HWND p, int id, const wchar_t* t, int x, int y, int w, int h) {
    return CreateWindowEx(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND MakeEdit(HWND p, int id, int x, int y, int w, int h) {
    return CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND MakeList(HWND p, int id, int x, int y, int w, int h) {
    return CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
void MakeLabel(HWND p, const wchar_t* t, int x, int y, int w) {
    CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, w, 20, p, NULL, g_hInst, NULL);
}

void AddLog(const wchar_t* src, const wchar_t* msg) {
    std::wstring line = std::wstring(src) + L": " + msg;
    g_logs.push_back(line); g_log.Log(src, msg);
    if (g_page && GetDlgItem(g_page, IDC_LOG_LIST)) {
        HWND lb = GetDlgItem(g_page, IDC_LOG_LIST);
        SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        int c = (int)SendMessage(lb, LB_GETCOUNT, 0, 0);
        SendMessage(lb, LB_SETTOPINDEX, c - 1, 0);
    }
}

void TrayInit(HWND hWnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy(g_nid.szTip, L"GOIDA AI MANAGER");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

// ===== Pages =====
void ShowFemboyPage(HWND parent) {
    ClearPage();
    g_page = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 200, 10, 400, 480, parent, NULL, g_hInst, NULL);
    int x = 220, y = 30;
    MakeLabel(g_page, L"Femboy Settings", x, y, 300); y += 30;
    MakeLabel(g_page, L"Name:", x, y, 80);
    HWND eName = MakeEdit(g_page, IDC_FEMBOY_NAME, x + 80, y, 200, 24);
    SetWindowText(eName, g_cfg.f_name.c_str()); y += 35;
    MakeLabel(g_page, (L"Voice: " + g_cfg.f_voice).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Eyes: " + g_cfg.f_eyes).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Hair: " + g_cfg.f_hair).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Top: " + g_cfg.f_top).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Bottom: " + g_cfg.f_bottom).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Feet: " + g_cfg.f_feet).c_str(), x, y, 300); y += 22;
    MakeLabel(g_page, (L"Ears: " + g_cfg.f_ears).c_str(), x, y, 300); y += 30;
    MakeBtn(g_page, IDC_FEMBOY_SAVE, L"Save Config", x + 60, y, 140, 30);
}

void ShowMemoryPage(HWND parent) {
    ClearPage();
    g_page = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 200, 10, 400, 480, parent, NULL, g_hInst, NULL);
    int x = 220, y = 30;
    MakeLabel(g_page, L"Memory Store", x, y, 200); y += 28;
    MakeList(g_page, IDC_MEMORY_LIST, x, y, 360, 180); y += 190;
    MakeLabel(g_page, L"Tag:", x, y, 50);
    MakeEdit(g_page, IDC_MEMORY_TAG, x + 50, y, 140, 22); y += 30;
    MakeLabel(g_page, L"Value:", x, y, 50);
    MakeEdit(g_page, IDC_MEMORY_TEXT, x + 50, y, 300, 22); y += 35;
    MakeBtn(g_page, IDC_MEMORY_ADD, L"Add", x + 80, y, 90, 28);
    MakeBtn(g_page, IDC_MEMORY_DEL, L"Delete", x + 190, y, 90, 28);
    std::wifstream f(L"global/memory.dat");
    std::wstring line; while (std::getline(f, line)) { if (!line.empty()) SendMessage(GetDlgItem(g_page, IDC_MEMORY_LIST), LB_ADDSTRING, 0, (LPARAM)line.c_str()); }
}

void ShowConsolePage(HWND parent) {
    ClearPage();
    g_page = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 200, 10, 400, 480, parent, NULL, g_hInst, NULL);
    int x = 220, y = 25;
    MakeLabel(g_page, L"Console / Logs", x, y, 200); y += 24;
    MakeList(g_page, IDC_LOG_LIST, x, y, 360, 260); y += 270;
    wchar_t st[128]; swprintf(st, L"client: %s | train: %s",
        g_procClient.hProcess ? L"RUNNING" : L"stopped",
        g_procTrain.hProcess ? L"RUNNING" : L"stopped");
    MakeLabel(g_page, st, x, y, 300); y += 24;
    MakeBtn(g_page, IDC_BTN_LAUNCH_CLIENT, L"Launch Client", x, y, 120, 28);
    MakeBtn(g_page, IDC_BTN_LAUNCH_TRAIN, L"Launch Train", x + 130, y, 120, 28);
    MakeBtn(g_page, IDC_BTN_KILL_ALL, L"Kill All", x + 260, y, 90, 28); y += 36;
    MakeBtn(g_page, IDC_LOG_CLEAR, L"Clear Log", x, y, 100, 26);
    MakeBtn(g_page, IDC_BTN_RELOAD, L"Reload Config", x + 110, y, 110, 26);
    MakeBtn(g_page, IDC_BTN_UPDATE, L"Check Update", x + 230, y, 110, 26); y += 34;
    // Check current auto-start state
    HKEY hk; wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH);
    bool autoStart = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        wchar_t val[MAX_PATH]; DWORD sz = sizeof(val);
        if (RegQueryValueExW(hk, L"GOIDA_Launcher", NULL, NULL, (LPBYTE)val, &sz) == ERROR_SUCCESS) autoStart = (wcscmp(val, exePath) == 0);
        RegCloseKey(hk);
    }
    std::wstring btnTxt = autoStart ? L"Auto-start: ON" : L"Auto-start: OFF";
    MakeBtn(g_page, IDC_BTN_AUTOSTART, btnTxt.c_str(), x, y, 150, 26);
    for (const auto& s : g_logs) SendMessage(GetDlgItem(g_page, IDC_LOG_LIST), LB_ADDSTRING, 0, (LPARAM)s.c_str());
    int c = (int)SendMessage(GetDlgItem(g_page, IDC_LOG_LIST), LB_GETCOUNT, 0, 0);
    if (c > 0) SendMessage(GetDlgItem(g_page, IDC_LOG_LIST), LB_SETTOPINDEX, c - 1, 0);
}

void ShowAboutPage(HWND parent) {
    ClearPage();
    g_page = CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 200, 10, 400, 480, parent, NULL, g_hInst, NULL);
    int y = 30;
    MakeLabel(g_page, L"GOIDA AI MANAGER", 260, y, 300); y += 30;
    MakeLabel(g_page, L"Version " GOIDA_VERSION, 270, y, 200); y += 26;
    MakeLabel(g_page, L"Master + Slaves | IPC: WM_COPYDATA", 230, y, 350); y += 22;
    MakeLabel(g_page, L"Log: global/logs.txt | Config: global/config.dat", 220, y, 400); y += 30;
    wchar_t buf[512]; swprintf(buf, L"model: %s | temp: %.2f", g_cfg.model.c_str(), g_cfg.temp);
    MakeLabel(g_page, buf, 220, y, 350); y += 20;
    swprintf(buf, L"api: %s", g_cfg.api_url.c_str());
    MakeLabel(g_page, buf, 220, y, 350); y += 20;
    swprintf(buf, L"femboy: %s", g_cfg.f_name.c_str());
    MakeLabel(g_page, buf, 220, y, 350); y += 20;
    MakeLabel(g_page, L"Minimize to tray to run in background.", 220, y, 350);
}

void ShowPage(HWND parent, int page) {
    switch (page) {
        case 0: ShowFemboyPage(parent); break;
        case 1: ShowConsolePage(parent); break;
        case 2: ShowMemoryPage(parent); break;
        case 3: ShowAboutPage(parent); break;
        default: ShowFemboyPage(parent); break;
    }
}

void HandleIpc(COPYDATASTRUCT* cds) {
    if (!cds || !cds->lpData) return;
    std::wstring data((wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
    while (!data.empty() && data.back() == L'\0') data.pop_back();
    if (data.find(L"LOG|") == 0) {
        auto p1 = data.find(L'|', 4);
        if (p1 != std::wstring::npos) AddLog(data.substr(4, p1 - 4).c_str(), data.substr(p1 + 1).c_str());
    } else if (data.find(L"STATUS|") == 0) AddLog(L"STATUS", data.substr(7).c_str());
}

void LaunchApp(const wchar_t* exe, PROCESS_INFORMATION& pi) {
    if (pi.hProcess) { DWORD ec; if (GetExitCodeProcess(pi.hProcess, &ec) && ec == STILL_ACTIVE) return; CloseHandle(pi.hProcess); CloseHandle(pi.hThread); ZeroMemory(&pi, sizeof(pi)); }
    ZeroMemory(&pi, sizeof(pi));
    STARTUPINFOW si = { sizeof(si) }; wchar_t cmd[MAX_PATH]; wcsncpy(cmd, exe, MAX_PATH);
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) AddLog(L"LAUNCHER", (std::wstring(L"Started ") + exe).c_str());
    else AddLog(L"LAUNCHER", (std::wstring(L"Failed: ") + exe).c_str());
}

void KillProc(PROCESS_INFORMATION& pi) {
    if (pi.hProcess) { TerminateProcess(pi.hProcess, 0); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); ZeroMemory(&pi, sizeof(pi)); }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            MakeBtn(hWnd, IDC_NAV_FEMBOY, L"Femboy", 12, 30, 176, 40);
            MakeBtn(hWnd, IDC_NAV_CONSOLE, L"Console", 12, 78, 176, 40);
            MakeBtn(hWnd, IDC_NAV_MEMORY, L"Memory", 12, 126, 176, 40);
            MakeBtn(hWnd, IDC_NAV_ABOUT, L"About", 12, 174, 176, 40);
            ShowPage(hWnd, 0);
            AddLog(L"LAUNCHER", L"Started v" GOIDA_VERSION);
            break;
        }
        case WM_COPYDATA: { HandleIpc((COPYDATASTRUCT*)lParam); return 0; }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd);
            } else if (lParam == WM_RBUTTONDOWN) {
                HMENU m = CreatePopupMenu();
                AppendMenu(m, MF_STRING, ID_TRAY_SHOW, L"Show / Hide");
                AppendMenu(m, MF_SEPARATOR, 0, NULL);
                AppendMenu(m, MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hWnd);
                POINT pt; GetCursorPos(&pt);
                int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(m);
                if (cmd == ID_TRAY_EXIT) PostMessage(hWnd, WM_CLOSE, 0, 0);
                else if (cmd == ID_TRAY_SHOW) ShowWindow(hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
            }
            return 0;
        }
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) { ShowWindow(hWnd, SW_HIDE); } // minimize to tray
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_NAV_FEMBOY) ShowPage(hWnd, 0);
            else if (id == IDC_NAV_CONSOLE) ShowPage(hWnd, 1);
            else if (id == IDC_NAV_MEMORY) ShowPage(hWnd, 2);
            else if (id == IDC_NAV_ABOUT) ShowPage(hWnd, 3);
            else if (id == IDC_BTN_LAUNCH_CLIENT) LaunchApp(L"client.exe", g_procClient);
            else if (id == IDC_BTN_LAUNCH_TRAIN) LaunchApp(L"train_prompt.exe", g_procTrain);
            else if (id == IDC_BTN_KILL_ALL) { KillProc(g_procClient); KillProc(g_procTrain); AddLog(L"LAUNCHER", L"Killed all"); }
            else if (id == IDC_BTN_RELOAD) { g_cfg.Load(L"global/config.dat"); AddLog(L"LAUNCHER", L"Config reloaded"); }
            else if (id == IDC_LOG_CLEAR) { g_logs.clear(); if (g_page) SetDlgItemText(g_page, IDC_LOG_LIST, L""); }
            else if (id == IDC_BTN_UPDATE) {
                std::wstring m = L"Update GOIDA?\nDownload from: " + g_cfg.update_url;
                if (MessageBox(hWnd, m.c_str(), L"GOIDA", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
                AddLog(L"LAUNCHER", L"Downloading...");
                wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
                std::wstring t = std::wstring(tmp) + L"goida_new.exe";
                HRESULT hr = URLDownloadToFileW(NULL, g_cfg.update_url.c_str(), t.c_str(), 0, NULL);
                if (FAILED(hr)) { MessageBox(hWnd, L"Download failed!", L"GOIDA", MB_OK | MB_ICONERROR); AddLog(L"LAUNCHER", L"Update failed"); }
                else {
                    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
                    wchar_t bt[MAX_PATH]; GetTempPathW(MAX_PATH, bt); wcscat(bt, L"goida_up.bat");
                    wchar_t bc[4096]; swprintf(bc, L"@timeout /t 2 /nobreak >nul\ncopy /Y \"%s\" \"%s\" >nul\ndel \"%s\"\nstart \"\" \"%s\"\ndel \"%~f0\"\n", t.c_str(), exe, t.c_str(), exe);
                    std::wofstream bf(bt); if (bf.is_open()) { bf << bc; bf.close(); }
                    ShellExecuteW(NULL, L"open", bt, NULL, NULL, SW_HIDE);
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                }
            }
            else if (id == IDC_BTN_AUTOSTART) {
                HKEY hk; wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
                if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
                    wchar_t cur[MAX_PATH]; DWORD sz = sizeof(cur);
                    bool on = false;
                    if (RegQueryValueExW(hk, L"GOIDA_Launcher", NULL, NULL, (LPBYTE)cur, &sz) == ERROR_SUCCESS && wcscmp(cur, exe) == 0) on = true;
                    if (on) RegDeleteValueW(hk, L"GOIDA_Launcher");
                    else RegSetValueExW(hk, L"GOIDA_Launcher", 0, REG_SZ, (BYTE*)exe, (DWORD)((wcslen(exe)+1)*sizeof(wchar_t)));
                    RegCloseKey(hk);
                    AddLog(L"LAUNCHER", on ? L"Auto-start disabled" : L"Auto-start enabled");
                    ShowPage(hWnd, 1); // refresh
                }
            }
            else if (id == IDC_FEMBOY_SAVE) {
                wchar_t name[256]; GetDlgItemText(g_page, IDC_FEMBOY_NAME, name, 256);
                g_cfg.f_name = name; g_cfg.Save(L"global/config.dat"); AddLog(L"LAUNCHER", L"Config saved");
                MessageBox(hWnd, L"Saved!", L"GOIDA", MB_OK | MB_ICONINFORMATION);
            }
            else if (id == IDC_MEMORY_ADD) {
                wchar_t tag[128], val[256]; GetDlgItemText(g_page, IDC_MEMORY_TAG, tag, 128);
                GetDlgItemText(g_page, IDC_MEMORY_TEXT, val, 256);
                if (wcslen(tag) > 0 && wcslen(val) > 0) {
                    wchar_t buf[512]; swprintf(buf, L"%s|%s", tag, val);
                    std::wofstream f(L"global/memory.dat", std::ios::app);
                    if (f.is_open()) { f << buf << L"\n"; f.close(); }
                    SendMessage(GetDlgItem(g_page, IDC_MEMORY_LIST), LB_ADDSTRING, 0, (LPARAM)buf);
                    SetDlgItemText(g_page, IDC_MEMORY_TAG, L""); SetDlgItemText(g_page, IDC_MEMORY_TEXT, L"");
                }
            }
            else if (id == IDC_MEMORY_DEL) {
                HWND lb = GetDlgItem(g_page, IDC_MEMORY_LIST);
                int sel = (int)SendMessage(lb, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) SendMessage(lb, LB_DELETESTRING, sel, 0);
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            static HBRUSH br = CreateSolidBrush(RGB(35, 35, 40));
            SetBkColor((HDC)wParam, RGB(35, 35, 40)); SetTextColor((HDC)wParam, RGB(200, 200, 210));
            return (LRESULT)br;
        }
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
            static HBRUSH br = CreateSolidBrush(RGB(25, 25, 30));
            SetBkColor((HDC)wParam, RGB(25, 25, 30)); SetTextColor((HDC)wParam, RGB(210, 210, 220));
            return (LRESULT)br;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
            RECT r; GetClientRect(hWnd, &r);
            HBRUSH bg = CreateSolidBrush(RGB(35, 35, 40));
            FillRect(hdc, &r, bg); DeleteObject(bg);
            HBRUSH nav = CreateSolidBrush(RGB(45, 45, 50));
            RECT nr = {0, 0, 200, r.bottom}; FillRect(hdc, &nr, nav); DeleteObject(nav);
            EndPaint(hWnd, &ps); break;
        }
        case WM_DESTROY: {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            KillProc(g_procClient); KillProc(g_procTrain);
            AddLog(L"LAUNCHER", L"Shutdown");
            PostQuitMessage(0); break;
        }
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst; InitCommonControls();
    CreateDirectoryW(L"global", NULL);
    g_cfg.Load(L"global/config.dat"); g_cfg.Save(L"global/config.dat");
    g_log.Open(L"global/logs.txt");

    WNDCLASS wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = WND_LAUNCHER; wc.hbrBackground = CreateSolidBrush(RGB(35, 35, 40));
    RegisterClass(&wc);

    RECT wr = {0, 0, 630, 520}; AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(0, WND_LAUNCHER, L"GOIDA AI MANAGER v" GOIDA_VERSION,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 50, 50,
        wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 0;

    TrayInit(g_hWnd);
    ShowWindow(g_hWnd, nShow); UpdateWindow(g_hWnd);

    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
