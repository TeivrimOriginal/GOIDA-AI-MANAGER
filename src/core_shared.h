// ===== GOIDA AI MANAGER - Shared Core v0.4 =====
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <thread>

#pragma comment(lib, "winhttp.lib")

#define WND_LAUNCHER L"GOIDALauncher_Master"
#define WND_CLIENT   L"GOIDAClient_Slave"
#define WND_TRAIN    L"GOIDATrain_Slave"
#define WM_GOIDA_IPC (WM_APP + 111)
#define WM_GOIDA_LOG (WM_APP + 112)

// ===== Config =====
struct GoConfig {
    std::wstring theme = L"dark";
    std::wstring lang = L"ru";
    std::wstring api_url = L"http://localhost:11434/api/generate";
    std::wstring model = L"qwen:14b";
    float temp = 0.7f;
    int max_tokens = 4096;
    std::wstring f_name = L"femboy";
    std::wstring f_voice = L"soft";
    std::wstring f_eyes = L"round";
    std::wstring f_hair = L"short";
    std::wstring f_top = L"tank";
    std::wstring f_bottom = L"briefs";
    std::wstring f_feet = L"sneakers";
    std::wstring f_ears = L"normal";
    std::wstring update_url = L"https://example.com/goida-latest.exe";

    bool Load(const wchar_t* path) {
        std::wifstream f(path);
        if (!f.is_open()) return false;
        std::wstring line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
            auto eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring k = line.substr(0, eq);
            std::wstring v = line.substr(eq + 1);
            if (k == L"theme") theme = v;
            else if (k == L"lang") lang = v;
            else if (k == L"api_url") api_url = v;
            else if (k == L"model") model = v;
            else if (k == L"temp") temp = (float)_wtof(v.c_str());
            else if (k == L"max_tokens") max_tokens = _wtoi(v.c_str());
            else if (k == L"f_name") f_name = v;
            else if (k == L"f_voice") f_voice = v;
            else if (k == L"f_eyes") f_eyes = v;
            else if (k == L"f_hair") f_hair = v;
            else if (k == L"f_top") f_top = v;
            else if (k == L"f_bottom") f_bottom = v;
            else if (k == L"f_feet") f_feet = v;
            else if (k == L"f_ears") f_ears = v;
            else if (k == L"update_url") update_url = v;
        }
        return true;
    }

    bool Save(const wchar_t* path) {
        std::wofstream f(path);
        if (!f.is_open()) return false;
        f << L"# GOIDA AI MANAGER Config\n";
        f << L"theme=" << theme << L"\nlang=" << lang << L"\napi_url=" << api_url << L"\n";
        f << L"model=" << model << L"\ntemp=" << temp << L"\nmax_tokens=" << max_tokens << L"\n";
        f << L"f_name=" << f_name << L"\nf_voice=" << f_voice << L"\nf_eyes=" << f_eyes << L"\n";
        f << L"f_hair=" << f_hair << L"\nf_top=" << f_top << L"\nf_bottom=" << f_bottom << L"\n";
        f << L"f_feet=" << f_feet << L"\nf_ears=" << f_ears << L"\nupdate_url=" << update_url << L"\n";
        return true;
    }
};

// ===== Logger =====
class GoLogger {
    std::wofstream m_file;
public:
    GoLogger() {}
    ~GoLogger() { if (m_file.is_open()) m_file.close(); }
    bool Open(const wchar_t* path) { m_file.open(path, std::ios::app); return m_file.is_open(); }
    void Log(const wchar_t* src, const wchar_t* msg) {
        time_t t = time(0); struct tm ltm; localtime_s(&ltm, &t);
        wchar_t buf[32]; wcsftime(buf, 32, L"%H:%M:%S", &ltm);
        std::wstring out = std::wstring(buf) + L" [" + src + L"] " + msg;
        OutputDebugStringW(out.c_str());
        if (m_file.is_open()) { m_file << out << L"\n"; m_file.flush(); }
    }
};

// ===== IPC =====
inline HWND IpcFindMaster() { return FindWindow(WND_LAUNCHER, NULL); }

inline void IpcSendLog(HWND master, const wchar_t* src, const wchar_t* msg) {
    if (!master) return;
    std::wstring data = std::wstring(L"LOG|") + src + L"|" + msg;
    COPYDATASTRUCT cds = { 0, (DWORD)((data.size()+1)*sizeof(wchar_t)), (PVOID)data.data() };
    SendMessage(master, WM_COPYDATA, 0, (LPARAM)&cds);
}

inline void IpcSendStatus(HWND master, const wchar_t* status) {
    if (!master) return;
    std::wstring data = std::wstring(L"STATUS|") + status;
    COPYDATASTRUCT cds = { 0, (DWORD)((data.size()+1)*sizeof(wchar_t)), (PVOID)data.data() };
    SendMessage(master, WM_COPYDATA, 0, (LPARAM)&cds);
}

// ===== JSON =====
inline std::wstring JsonEscape(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'"') r += L"\\\"";
        else if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n";
        else if (c == L'\r') r += L"\\r";
        else if (c == L'\t') r += L"\\t";
        else r += c;
    }
    return r;
}

inline std::wstring JsonGetString(const std::wstring& json, const std::wstring& key) {
    std::wstring s = L"\"" + key + L"\":\"";
    auto pos = json.find(s);
    if (pos == std::wstring::npos) return L"";
    pos += s.size();
    std::wstring v;
    while (pos < json.size()) {
        if (json[pos] == L'"' && (pos == 0 || json[pos-1] != L'\\')) break;
        if (json[pos] == L'\\' && pos+1 < json.size()) {
            if (json[pos+1] == L'n') { v += L'\n'; pos += 2; }
            else if (json[pos+1] == L'r') { v += L'\r'; pos += 2; }
            else if (json[pos+1] == L't') { v += L'\t'; pos += 2; }
            else if (json[pos+1] == L'"') { v += L'"'; pos += 2; }
            else if (json[pos+1] == L'\\') { v += L'\\'; pos += 2; }
            else { v += json[pos]; pos++; }
        } else { v += json[pos]; pos++; }
    }
    return v;
}

// ===== WinHTTP =====
inline std::wstring HttpPost(const std::wstring& url, const std::wstring& body, int timeoutMs = 15000) {
    bool isHttps = (url.find(L"https://") == 0);
    std::wstring host, path; int port = isHttps ? 443 : 80;
    size_t s = url.find(L"://");
    if (s == std::wstring::npos) return L"ERR:bad_url";
    size_t start = s + 3;
    size_t slash = url.find(L'/', start);
    std::wstring hp = (slash == std::wstring::npos) ? url.substr(start) : url.substr(start, slash - start);
    path = (slash == std::wstring::npos) ? L"/" : url.substr(slash);
    size_t colon = hp.find(L':');
    if (colon != std::wstring::npos) { host = hp.substr(0, colon); port = _wtoi(hp.substr(colon+1).c_str()); }
    else { host = hp; }

    HINTERNET hSession = WinHttpOpen(L"GOIDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return L"ERR:session";
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return L"ERR:connect"; }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, NULL, NULL, flags);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return L"ERR:request"; }

    WinHttpSetTimeouts(hReq, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    std::string utf8Body;
    int len = WideCharToMultiByte(CP_UTF8, 0, body.c_str(), (int)body.size(), NULL, 0, NULL, NULL);
    if (len > 0) { utf8Body.resize(len); WideCharToMultiByte(CP_UTF8, 0, body.c_str(), (int)body.size(), &utf8Body[0], len, NULL, NULL); }

    LPCWSTR headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hReq, headers, (DWORD)wcslen(headers),
        (LPVOID)utf8Body.data(), (DWORD)utf8Body.size(), (DWORD)utf8Body.size(), 0);
    if (!sent) {
        DWORD err = GetLastError();
        wchar_t ebuf[64]; swprintf(ebuf, L"ERR:send(%lu)", err);
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return ebuf;
    }
    if (!WinHttpReceiveResponse(hReq, NULL)) {
        DWORD err = GetLastError();
        wchar_t ebuf[64]; swprintf(ebuf, L"ERR:recv(%lu)", err);
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return ebuf;
    }

    std::string resp;
    DWORD read = 0; char buf[4096];
    while (WinHttpReadData(hReq, buf, sizeof(buf)-1, &read) && read > 0) {
        buf[read] = 0; resp += buf;
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    if (resp.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), NULL, 0);
    if (wlen <= 0) return L"ERR:encoding";
    std::wstring wres; wres.resize(wlen);
    MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), &wres[0], wlen);
    return wres;
}
