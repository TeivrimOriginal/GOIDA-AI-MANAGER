// ===== GOIDA AI MANAGER v2.1 — SQLite + Clean UI + Foundation =====
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <ctime>
#include "sqlite3.h"
#include <commdlg.h>
#include <urlmon.h>
#include <richedit.h>
#include "utf8.h"
#include "raii.h"
#include "i18n.h"
#include "db_migration.h"
// httplib available as src/httplib.h вЂ” used via separate demo/build, keep lightweight include guard
// #include "httplib.h" // uncomment to enable full httplib client (requires ws2_32 linkage)

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")

#define APPNAME L"GOIDA AI MANAGER"

// ===== Colors =====
#define COL_BG        RGB(28, 28, 30)
#define COL_NAV       RGB(36, 36, 38)
#define COL_PANEL     RGB(40, 40, 42)
#define COL_ACCENT    RGB(0, 122, 204)
#define COL_ACTIVE    RGB(70, 70, 75)
#define COL_HOVER     RGB(55, 55, 60)
#define COL_TEXT      RGB(215, 215, 220)
#define COL_TEXT_DIM  RGB(140, 140, 145)
#define COL_EDIT_BG   RGB(22, 22, 26)
#define COL_BORDER    RGB(50, 50, 55)
#define COL_MSG_USER  RGB(0, 140, 255)
#define COL_MSG_AI    RGB(60, 210, 80)
#define COL_MSG_SYS   RGB(140, 140, 145)
#define COL_ACCENT2   RGB(200, 100, 220)
#define COL_SUCCESS   RGB(50, 200, 80)
#define COL_WARN      RGB(255, 180, 40)

// ===== IDs =====
enum {
    ID_NAV_CHAT = 1001, ID_NAV_TRAIN, ID_NAV_FEMBOY, ID_NAV_MEMORY, ID_NAV_CONSOLE, ID_NAV_MODELS, ID_NAV_ABOUT,
    ID_CHAT_HIST = 2001, ID_CHAT_INPUT, ID_CHAT_SEND, ID_CHAT_CLEAR, ID_CHAT_SAVE, ID_CHAT_LOAD,
    ID_CHAT_TEST, ID_CHAT_CTXCLR, ID_CHAT_API, ID_CHAT_MODEL, ID_CHAT_CONN, ID_CHAT_STAT, ID_CHAT_REFRESH, ID_CHAT_STOP, ID_CHAT_SAVEAS,
    ID_CHAT_PULL = 2020, ID_CHAT_DEL, ID_CHAT_INFO, ID_CHAT_TEMP, ID_CHAT_SEED, ID_CHAT_TEMPV, ID_CHAT_TOKENS,
    ID_CHAT_SYS = 2040, ID_CHAT_REG, ID_CHAT_JSON, ID_CHAT_COPYLAST, ID_CHAT_BRANCH = 2044,
    ID_MODEL_NAME = 2100, ID_MODEL_DST, ID_MODEL_PULL, ID_MODEL_DEL, ID_MODEL_INFO, ID_MODEL_COPY, ID_MODEL_CREATE, ID_MODEL_RUNNING, ID_MODEL_LIST, ID_MODEL_STAT, ID_MODEL_REFRESH, ID_MODEL_PUSH,
    ID_MODEL_PROGRESS = 2113, ID_MODEL_MKT_LIST = 2114, ID_MODEL_MKT_INSTALL = 2115, ID_MODEL_EMBED = 2116, ID_MODEL_EMBED_INPUT = 2117, ID_MODEL_TOOLS = 2118,
    ID_TRAIN_SYS = 3001, ID_TRAIN_PROMPT, ID_TRAIN_GEN, ID_TRAIN_SAVE, ID_TRAIN_LOAD,
    ID_TRAIN_OUT, ID_TRAIN_LIST, ID_TRAIN_TEMP, ID_TRAIN_TEMPV, ID_TRAIN_TOKENS, ID_TRAIN_REPORT, ID_TRAIN_COPY,
    ID_FEMBOY_NAME = 4001, ID_FEMBOY_SAVE = 4010,
    ID_FEMBOY_VOICE = 4101, ID_FEMBOY_EYES = 4102, ID_FEMBOY_HAIR = 4103, ID_FEMBOY_TOP = 4104, ID_FEMBOY_BOTTOM = 4105, ID_FEMBOY_FEET = 4106, ID_FEMBOY_EARS = 4107,
    // Profiles (Stage2)
    ID_PROF_LIST = 4200, ID_PROF_NAME, ID_PROF_SYS, ID_PROF_TEMP, ID_PROF_TEMPV, ID_PROF_TOKENS, ID_PROF_KEEP, ID_PROF_CREATE, ID_PROF_SAVE, ID_PROF_DEL, ID_PROF_LOAD, ID_PROF_AVATAR,
    ID_MEM_LIST = 5001, ID_MEM_TAG, ID_MEM_VAL, ID_MEM_ADD, ID_MEM_DEL, ID_MEM_COUNT = 5006,
    ID_CONS_LOG = 6001, ID_CONS_CLR, ID_CONS_RELOAD, ID_CONS_UPDATE, ID_CONS_AUTO,
    ID_SET_THEME = 7001, ID_SET_ACCENT, ID_SET_ROUND, ID_SET_ADAPTIVE, ID_SET_LANG,
    WM_HTTP_DONE = WM_APP + 50, WM_HTTP_CHUNK = WM_APP + 51, WM_HTTP_ERR = WM_APP + 52,
    WM_CONN_RESULT = WM_APP + 60, WM_MODEL_LIST = WM_APP + 61, WM_MODEL_PROGRESS = WM_APP + 62, WM_MODEL_EMBED_DONE = WM_APP + 63,
    ID_TRAY_EXIT = 9001, ID_TRAY_SHOW = 9002, WM_TRAYICON = WM_APP + 200, IDT_HOVER = 1,
    IDT_CONN_CHECK = 2
};

// ===== Globals =====
HINSTANCE g_hInst; HWND g_hWnd; int g_curPage = 0;
int g_hoverBtn = -1, g_downBtn = -1; bool g_tracking = false;
std::vector<std::wstring> g_logCache;
std::vector<std::pair<std::wstring,std::wstring>> g_history;
std::wstring g_streamBuf;
enum ConnStatus { CONN_UNKNOWN, CONN_OK, CONN_FAIL };
ConnStatus g_connStatus = CONN_UNKNOWN;
HFONT g_fontUI = NULL, g_fontBold = NULL, g_fontTitle = NULL, g_fontSmall = NULL;
HBRUSH g_brBg = NULL, g_brNav = NULL, g_brPanel = NULL, g_brEdit = NULL;
WNDPROC g_origInputProc = NULL;
volatile bool g_cancelStream = false;
__int64 g_startTick = 0;
NOTIFYICONDATAW g_nid = {};
sqlite3* g_db = NULL;
// Stage3: embed/images/tools
std::vector<std::string> g_pendingImages; // base64 strings for next message
std::wstring g_toolsJson; // if not empty, include tools in chat
bool g_toolsEnabled = false;
// Stage5: personalization
COLORREF g_colAccent = COL_ACCENT;
inline COLORREF AccentFromName(const std::wstring& n) {
    if (n==L"purple") return RGB(150,90,255);
    if (n==L"green") return RGB(40,180,90);
    if (n==L"orange") return RGB(255,140,0);
    if (n==L"pink") return RGB(255,80,140);
    return COL_ACCENT;
}
void UpdateAccent();

// ===== SQLite =====
bool DBExec(const wchar_t* sql) {
    char buf[4096]; int n = WideCharToMultiByte(CP_UTF8, 0, sql, -1, buf, sizeof(buf), NULL, NULL);
    if (n <= 0) return false;
    char* err = NULL;
    if (sqlite3_exec(g_db, buf, NULL, NULL, &err) != SQLITE_OK) {
        OutputDebugStringA(err); sqlite3_free(err); return false;
    }
    return true;
}

bool DBExec(const char* sql) {
    char* err = NULL;
    if (sqlite3_exec(g_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        OutputDebugStringA(err); sqlite3_free(err); return false;
    }
    return true;
}

sqlite3_stmt* DBPrep(const char* sql) {
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    return stmt;
}

void DBInit() {
    CreateDirectoryW(L"global", NULL);
    // Use UTF-8 helper for path conversion
    std::string dbPath = goida::utf8::w2utf8(L"global/goida.db");
    sqlite3_open(dbPath.c_str(), &g_db);
    // Versioned migration (WAL, FK, profiles, marketplace, branches)
    goida::db::migrate(g_db);
}

std::wstring DBGet(const wchar_t* key, const wchar_t* def = L"") {
    goida::raii::Stmt st(DBPrep("SELECT value FROM config WHERE key=?1"));
    if (!st) return def;
    std::string k = goida::utf8::w2utf8(key);
    sqlite3_bind_text(st.get(), 1, k.c_str(), -1, SQLITE_TRANSIENT);
    std::wstring r = def;
    if (sqlite3_step(st.get()) == SQLITE_ROW) {
        const char* v = (const char*)sqlite3_column_text(st.get(), 0);
        if (v) r = goida::utf8::utf8_to_w(std::string(v));
    }
    return r;
}

void DBSet(const wchar_t* key, const wchar_t* val) {
    goida::raii::Stmt st(DBPrep("INSERT OR REPLACE INTO config(key,value) VALUES(?1,?2)"));
    if (!st) return;
    std::string k = goida::utf8::w2utf8(key);
    std::string v = goida::utf8::w2utf8(val);
    sqlite3_bind_text(st.get(), 1, k.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.get(), 2, v.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st.get());
}

void DBLog(const wchar_t* src, const wchar_t* msg) {
    goida::raii::Stmt st(DBPrep("INSERT INTO logs(source,message) VALUES(?1,?2)"));
    if (!st) return;
    std::string a = goida::utf8::w2utf8(src);
    std::string b = goida::utf8::w2utf8(msg);
    sqlite3_bind_text(st.get(), 1, a.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.get(), 2, b.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st.get());
}

// Example httplib fallback helper (demonstrates cpp-httplib usage вЂ” src/httplib.h present)
// To enable, uncomment #include "httplib.h" and use httplib::Client in this fn.
inline std::wstring HttpGetViaHttplib(const std::wstring& url_w) {
    std::string url = goida::utf8::w2utf8(url_w);
    (void)url;
    // httplib usage example (kept as comment for compile-light build):
    // httplib::Client cli("localhost", 11434);
    // auto res = cli.Get("/api/tags");
    // if (res && res->status==200) return goida::utf8::utf8_to_w(res->body);
    return L"";
}

// ===== Config =====
struct Config {
    std::wstring api_url = L"http://localhost:11434/api/chat";
    std::wstring model = L"qwen:14b"; float temp = 0.7f; int max_tokens = 4096; long long seed = -1;
    std::wstring sys_prompt = L"You are a helpful AI assistant."; bool json_mode = false;
    std::wstring lang = L"ru";
    std::wstring theme = L"dark"; // dark, amoled, light
    std::wstring accent = L"blue"; // blue, purple, green, orange
    bool rounded = true;
    bool adaptive = true;
    std::wstring f_name = L"femboy", f_voice = L"soft", f_eyes = L"round", f_hair = L"short";
    std::wstring f_top = L"tank", f_bottom = L"briefs", f_feet = L"sneakers", f_ears = L"normal";
    std::wstring update_url = L"https://example.com/goida-latest.exe";
    void Load() {
        api_url = DBGet(L"api_url", api_url.c_str());
        model = DBGet(L"model", model.c_str());
        std::wstring t = DBGet(L"temp"); if (!t.empty()) temp = (float)_wtof(t.c_str());
        std::wstring m = DBGet(L"max_tokens"); if (!m.empty()) max_tokens = _wtoi(m.c_str());
        std::wstring sd = DBGet(L"seed"); if (!sd.empty()) seed = _wtoi64(sd.c_str());
        sys_prompt = DBGet(L"sys_prompt", sys_prompt.c_str());
        std::wstring jm = DBGet(L"json_mode"); if (!jm.empty()) json_mode = (_wtoi(jm.c_str()) != 0);
        lang = DBGet(L"lang", lang.c_str());
        theme = DBGet(L"theme", theme.c_str());
        accent = DBGet(L"accent", accent.c_str());
        std::wstring rd = DBGet(L"rounded"); if (!rd.empty()) rounded = (_wtoi(rd.c_str())!=0);
        std::wstring ad = DBGet(L"adaptive"); if (!ad.empty()) adaptive = (_wtoi(ad.c_str())!=0);
        f_name = DBGet(L"f_name", f_name.c_str()); f_voice = DBGet(L"f_voice", f_voice.c_str());
        f_eyes = DBGet(L"f_eyes", f_eyes.c_str()); f_hair = DBGet(L"f_hair", f_hair.c_str());
        f_top = DBGet(L"f_top", f_top.c_str()); f_bottom = DBGet(L"f_bottom", f_bottom.c_str());
        f_feet = DBGet(L"f_feet", f_feet.c_str()); f_ears = DBGet(L"f_ears", f_ears.c_str());
        update_url = DBGet(L"update_url", update_url.c_str());
    }
    void Save() {
        DBSet(L"api_url", api_url.c_str()); DBSet(L"model", model.c_str());
        wchar_t b[64]; swprintf(b, L"%.2f", temp); DBSet(L"temp", b);
        swprintf(b, L"%d", max_tokens); DBSet(L"max_tokens", b);
        swprintf(b, L"%lld", seed); DBSet(L"seed", b);
        DBSet(L"sys_prompt", sys_prompt.c_str());
        DBSet(L"json_mode", json_mode ? L"1" : L"0");
        DBSet(L"lang", lang.c_str());
        DBSet(L"theme", theme.c_str());
        DBSet(L"accent", accent.c_str());
        DBSet(L"rounded", rounded?L"1":L"0");
        DBSet(L"adaptive", adaptive?L"1":L"0");
        DBSet(L"f_name", f_name.c_str()); DBSet(L"f_voice", f_voice.c_str());
        DBSet(L"f_eyes", f_eyes.c_str()); DBSet(L"f_hair", f_hair.c_str());
        DBSet(L"f_top", f_top.c_str()); DBSet(L"f_bottom", f_bottom.c_str());
        DBSet(L"f_feet", f_feet.c_str()); DBSet(L"f_ears", f_ears.c_str());
        DBSet(L"update_url", update_url.c_str());
    }
};
Config g_cfg;
void UpdateAccent() { g_colAccent = AccentFromName(g_cfg.accent); }

// ===== Profiles (Stage2) =====
struct Profile {
    std::wstring name;
    std::wstring avatar;
    std::wstring system_prompt;
    float temp = 0.7f;
    int max_tokens = 4096;
    std::wstring keep_alive = L"5m"; // L1=5m, L2=30m, L3=1h/0
};

inline bool EnsureDefaultProfile() {
    // if profiles empty, create default from g_cfg femboy settings
    goida::raii::Stmt cnt(DBPrep("SELECT COUNT(*) FROM profiles"));
    if (!cnt) return false;
    int c=0;
    if (sqlite3_step(cnt.get())==SQLITE_ROW) c=sqlite3_column_int(cnt.get(),0);
    if (c>0) return true;
    // Create default profile "FemboyDefault" migrating femboy fields into system_prompt
    std::wstring sys = g_cfg.sys_prompt;
    if (sys.empty()) sys = L"You are a helpful AI assistant.";
    // Embed femboy personality into system prompt for migration
    std::wstring fem = L"Name: " + g_cfg.f_name + L", Voice: " + g_cfg.f_voice + L", Eyes: " + g_cfg.f_eyes;
    std::wstring merged = sys + L" [" + fem + L"]";
    goida::raii::Stmt ins(DBPrep("INSERT INTO profiles(name, avatar, system_prompt, temperature, max_tokens, keep_alive) VALUES(?1,?2,?3,?4,?5,?6)"));
    if (!ins) return false;
    std::string n = goida::utf8::w2utf8(g_cfg.f_name.empty()? L"default" : g_cfg.f_name);
    std::string av = goida::utf8::w2utf8(L"");
    std::string sp = goida::utf8::w2utf8(merged);
    std::string ka = "5m";
    sqlite3_bind_text(ins.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins.get(),2,av.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins.get(),3,sp.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(ins.get(),4,g_cfg.temp);
    sqlite3_bind_int(ins.get(),5,g_cfg.max_tokens);
    sqlite3_bind_text(ins.get(),6,ka.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(ins.get());
    // Also ensure DB config active profile
    DBSet(L"active_profile", g_cfg.f_name.empty()? L"default" : g_cfg.f_name.c_str());
    return true;
}

inline std::vector<Profile> LoadAllProfiles() {
    std::vector<Profile> out;
    goida::raii::Stmt s(DBPrep("SELECT name, avatar, system_prompt, temperature, max_tokens, keep_alive FROM profiles ORDER BY created_at ASC"));
    if (!s) return out;
    while (sqlite3_step(s.get())==SQLITE_ROW) {
        Profile p;
        const char* n = (const char*)sqlite3_column_text(s.get(),0);
        const char* av = (const char*)sqlite3_column_text(s.get(),1);
        const char* sp = (const char*)sqlite3_column_text(s.get(),2);
        p.temp = (float)sqlite3_column_double(s.get(),3);
        p.max_tokens = sqlite3_column_int(s.get(),4);
        const char* ka = (const char*)sqlite3_column_text(s.get(),5);
        if (n) p.name = goida::utf8::utf8_to_w(n);
        if (av) p.avatar = goida::utf8::utf8_to_w(av);
        if (sp) p.system_prompt = goida::utf8::utf8_to_w(sp);
        if (ka) p.keep_alive = goida::utf8::utf8_to_w(ka);
        out.push_back(p);
    }
    return out;
}

inline bool SaveProfile(const Profile& p) {
    goida::raii::Stmt s(DBPrep("INSERT OR REPLACE INTO profiles(name, avatar, system_prompt, temperature, max_tokens, keep_alive, updated_at) VALUES(?1,?2,?3,?4,?5,?6, datetime('now','localtime'))"));
    if (!s) return false;
    std::string n = goida::utf8::w2utf8(p.name);
    std::string av = goida::utf8::w2utf8(p.avatar);
    std::string sp = goida::utf8::w2utf8(p.system_prompt);
    std::string ka = goida::utf8::w2utf8(p.keep_alive);
    sqlite3_bind_text(s.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s.get(),2,av.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s.get(),3,sp.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(s.get(),4,p.temp);
    sqlite3_bind_int(s.get(),5,p.max_tokens);
    sqlite3_bind_text(s.get(),6,ka.c_str(),-1,SQLITE_TRANSIENT);
    return sqlite3_step(s.get())==SQLITE_DONE;
}

inline bool DeleteProfile(const std::wstring& name) {
    goida::raii::Stmt s(DBPrep("DELETE FROM profiles WHERE name=?1"));
    if (!s) return false;
    std::string n = goida::utf8::w2utf8(name);
    sqlite3_bind_text(s.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    return sqlite3_step(s.get())==SQLITE_DONE;
}

inline int MemoryCount() {
    goida::raii::Stmt s(DBPrep("SELECT COUNT(*) FROM memory"));
    if (!s) return 0;
    if (sqlite3_step(s.get())==SQLITE_ROW) return sqlite3_column_int(s.get(),0);
    return 0;
}

// keep_alive L1/L2/L3 mapping helpers
inline std::wstring KeepAliveFromLevel(int lvl) {
    if (lvl==1) return L"5m";
    if (lvl==2) return L"30m";
    if (lvl==3) return L"1h";
    return L"5m";
}
inline int LevelFromKeepAlive(const std::wstring& ka) {
    if (ka==L"5m") return 1;
    if (ka==L"30m") return 2;
    if (ka==L"1h" || ka==L"60m") return 3;
    if (ka==L"0" || ka==L"none") return 0;
    return 1;
}
inline std::wstring GetActiveKeepAlive() {
    std::wstring active = DBGet(L"active_profile", L"");
    if (active.empty()) return L"5m";
    goida::raii::Stmt s(DBPrep("SELECT keep_alive FROM profiles WHERE name=?1"));
    if (!s) return L"5m";
    std::string n = goida::utf8::w2utf8(active);
    sqlite3_bind_text(s.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s.get())==SQLITE_ROW) {
        const char* ka = (const char*)sqlite3_column_text(s.get(),0);
        if (ka) return goida::utf8::utf8_to_w(ka);
    }
    return L"5m";
}

// ===== JSON helpers =====
std::wstring JEsc(const std::wstring& s) {
    std::wstring r; for (wchar_t c : s) {
        if (c == L'"') r += L"\\\""; else if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n"; else if (c == L'\r') r += L"\\r";
        else if (c == L'\t') r += L"\\t"; else r += c; }
    return r;
}
std::wstring JStr(const std::wstring& j, const std::wstring& k) {
    std::wstring s = L"\"" + k + L"\":\""; auto p = j.find(s); if (p == std::wstring::npos) return L"";
    p += s.size(); std::wstring v;
    while (p < j.size()) { if (j[p] == L'"' && (p == 0 || j[p-1] != L'\\')) break;
        if (j[p] == L'\\' && p+1 < j.size()) {
            if (j[p+1] == L'n') { v += L'\n'; p += 2; } else if (j[p+1] == L'r') { v += L'\r'; p += 2; }
            else if (j[p+1] == L't') { v += L'\t'; p += 2; } else if (j[p+1] == L'"') { v += L'"'; p += 2; }
            else if (j[p+1] == L'\\') { v += L'\\'; p += 2; } else { v += j[p]; p++; }
        } else { v += j[p]; p++; } }
    return v;
}

struct HttpD { std::wstring url, body; HWND hwnd; int stream; };

DWORD WINAPI HttpThr(LPVOID lp) {
    HttpD* d = (HttpD*)lp; bool https = (d->url.find(L"https://") == 0);
    std::wstring host, path; int port = https ? 443 : 80;
    size_t s = d->url.find(L"://"); if (s == std::wstring::npos) { PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:url")); delete d; return 0; }
    size_t st = s + 3, sl = d->url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? d->url.substr(st) : d->url.substr(st, sl - st);
    path = (sl == std::wstring::npos) ? L"/" : d->url.substr(sl); size_t co = hp.find(L':');
    if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) { PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:session")); delete d; return 0; }
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:connect")); delete d; return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"POST", path.c_str(), NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:req")); delete d; return 0; }
    WinHttpSetTimeouts(hr, 30000, 30000, 30000, 30000);
    std::string utf8; int l = WideCharToMultiByte(CP_UTF8, 0, d->body.c_str(), (int)d->body.size(), NULL, 0, NULL, NULL);
    if (l > 0) { utf8.resize(l); WideCharToMultiByte(CP_UTF8, 0, d->body.c_str(), (int)d->body.size(), &utf8[0], l, NULL, NULL); }
    LPCWSTR hdrs = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hr, hdrs, (DWORD)wcslen(hdrs), (LPVOID)utf8.data(), (DWORD)utf8.size(), (DWORD)utf8.size(), 0)) {
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:send")); delete d; return 0; }
    if (!WinHttpReceiveResponse(hr, NULL)) { WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:recv")); delete d; return 0; }
    g_cancelStream = false;
    std::string buf, fullResp; DWORD r = 0; char tmp[4096];
    while (!g_cancelStream && WinHttpReadData(hr, tmp, sizeof(tmp)-1, &r) && r > 0) {
        tmp[r] = 0;
        if (d->stream) {
            buf += tmp; size_t nl;
            while ((nl = buf.find('\n')) != std::string::npos) {
                std::string line = buf.substr(0, nl); buf.erase(0, nl + 1);
                if (!line.empty()) {
                    auto rp = line.find("\"response\":\""); if (rp == std::string::npos) rp = line.find("\"content\":\"");
                    if (rp != std::string::npos) {
                        rp = line.find('"', rp + 11); if (rp != std::string::npos) {
                            rp++; std::string tok;
                            while (rp < line.size() && !(line[rp] == '"' && (rp == 0 || line[rp-1] != '\\'))) {
                                if (line[rp] == '\\' && rp+1 < line.size()) {
                                    if (line[rp+1] == 'n') tok += '\n'; else if (line[rp+1] == 'r') tok += '\r';
                                    else if (line[rp+1] == '"') tok += '"'; else if (line[rp+1] == '\\') tok += '\\';
                                    else tok += line[rp]; rp += 2; } else { tok += line[rp]; rp++; } }
                            if (!tok.empty()) {
                                int wl = MultiByteToWideChar(CP_UTF8, 0, tok.c_str(), (int)tok.size(), NULL, 0);
                                if (wl > 0) { std::wstring wt; wt.resize(wl); MultiByteToWideChar(CP_UTF8, 0, tok.c_str(), (int)tok.size(), &wt[0], wl);
                                    PostMessage(d->hwnd, WM_HTTP_CHUNK, 0, (LPARAM)new std::wstring(wt)); } }
                            if (line.find("\"done\":true") != std::string::npos) goto done;
                        }
                    }
                }
            }
        } else fullResp += tmp;
    }
done:
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    if (!d->stream) {
        if (fullResp.empty()) PostMessage(d->hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"ERR:empty"));
        else { int wl = MultiByteToWideChar(CP_UTF8, 0, fullResp.c_str(), (int)fullResp.size(), NULL, 0);
            if (wl > 0) { std::wstring wr; wr.resize(wl); MultiByteToWideChar(CP_UTF8, 0, fullResp.c_str(), (int)fullResp.size(), &wr[0], wl);
                PostMessage(d->hwnd, WM_HTTP_DONE, 0, (LPARAM)new std::wstring(wr)); } }
    } else PostMessage(d->hwnd, WM_HTTP_DONE, 0, (LPARAM)0);
    delete d; return 0;
}

// ===== Connection check =====
DWORD WINAPI ConnCheckThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    std::wstring url = g_cfg.api_url;
    bool https = (url.find(L"https://") == 0);
    std::wstring host; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) { PostMessage(hwnd, WM_CONN_RESULT, CONN_FAIL, 0); return 0; }
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    size_t co = hp.find(L':');
    if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) { PostMessage(hwnd, WM_CONN_RESULT, CONN_FAIL, 0); return 0; }
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); PostMessage(hwnd, WM_CONN_RESULT, CONN_FAIL, 0); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"GET", L"/api/tags", NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); PostMessage(hwnd, WM_CONN_RESULT, CONN_FAIL, 0); return 0; }
    WinHttpSetTimeouts(hr, 5000, 5000, 5000, 5000);
    bool ok = false;
    if (WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0)) {
        ok = WinHttpReceiveResponse(hr, NULL);
    }
    if (!ok) {
        DWORD sc = 0; DWORD scs = sizeof(sc);
        if (WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &sc, &scs, NULL))
            ok = (sc >= 200 && sc < 500);
    }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    PostMessage(hwnd, WM_CONN_RESULT, ok ? CONN_OK : CONN_FAIL, 0);
    return 0;
}

// ===== Model list fetch =====
DWORD WINAPI ModelListThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    std::wstring url = g_cfg.api_url;
    bool https = (url.find(L"https://") == 0);
    std::wstring host; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) return 0;
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    size_t co = hp.find(L':');
    if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) return 0;
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"GET", L"/api/tags", NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    WinHttpSetTimeouts(hr, 5000, 5000, 5000, 5000);
    std::string resp;
    if (WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(hr, NULL)) {
        DWORD r = 0; char tmp[4096];
        while (WinHttpReadData(hr, tmp, sizeof(tmp)-1, &r) && r > 0) { tmp[r] = 0; resp += tmp; }
    }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    // Parse model names + sizes from JSON: {"models":[{"name":"...","size":123,...}]}
    std::vector<std::wstring> models;
    auto p = resp.find("\"name\":\"");
    while (p != std::string::npos) {
        p += 8; auto e = resp.find('"', p);
        if (e == std::string::npos) break;
        int wl = MultiByteToWideChar(CP_UTF8, 0, resp.c_str()+p, (int)(e-p), NULL, 0);
        if (wl > 0) { std::wstring ws; ws.resize(wl); MultiByteToWideChar(CP_UTF8, 0, resp.c_str()+p, (int)(e-p), &ws[0], wl);
            // Look for size after name
            auto sp = resp.find("\"size\":", e);
            if (sp != std::string::npos && sp - e < 200) {
                sp += 7; auto ep = resp.find(',', sp);
                if (ep != std::string::npos && ep - sp < 30) {
                    std::string sn = resp.substr(sp, ep - sp);
                    __int64 bytes = _atoi64(sn.c_str());
                    wchar_t szb[64];
                    if (bytes >= 1073741824LL) swprintf(szb, L" (%.1f GB)", bytes / 1073741824.0);
                    else if (bytes >= 1048576) swprintf(szb, L" (%.0f MB)", bytes / 1048576.0);
                    else swprintf(szb, L" (%lld B)", bytes);
                    ws += szb;
                }
            }
            models.push_back(ws); }
        p = resp.find("\"name\":\"", e);
    }
    // Send each model name to main window
    for (auto& m : models) {
        std::wstring* ms = new std::wstring(m);
        PostMessage(hwnd, WM_MODEL_LIST, 0, (LPARAM)ms);
    }
    PostMessage(hwnd, WM_MODEL_LIST, 1, 0); // done marker
    return 0;
}

// ===== UTF-8 to UTF-16 helper =====
std::wstring u2w(const std::string& s) {
    if (s.empty()) return L"";
    int l = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (l <= 0) return L"";
    std::wstring r; r.resize(l);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &r[0], l);
    return r;
}

// ===== Pull model =====
DWORD WINAPI PullModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t model[256]; GetDlgItemText(hwnd, ID_CHAT_MODEL, model, 256);
    if (model[0] == 0) { PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"No model")); return 0; }
    // Build API base from g_cfg.api_url
    std::wstring url = g_cfg.api_url;
    size_t sp = url.rfind('/'); if (sp != std::wstring::npos) url = url.substr(0, sp);
    url += L"/pull";
    bool https = (url.find(L"https://") == 0);
    std::wstring host, path; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) return 0;
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    path = (sl == std::wstring::npos) ? L"/" : url.substr(sl);
    size_t co = hp.find(L':'); if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) return 0;
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"POST", path.c_str(), NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    WinHttpSetTimeouts(hr, 120000, 120000, 120000, 120000);
    std::string jbody = "{\"name\":\""; int wl = WideCharToMultiByte(CP_UTF8, 0, model, -1, NULL, 0, NULL, NULL);
    if (wl > 0) { std::string mb; mb.resize(wl-1); WideCharToMultiByte(CP_UTF8, 0, model, -1, &mb[0], wl, NULL, NULL); jbody += mb; }
    jbody += "\",\"stream\":true}";
    LPCWSTR hdrs = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hr, hdrs, (DWORD)wcslen(hdrs), (LPVOID)jbody.data(), (DWORD)jbody.size(), (DWORD)jbody.size(), 0)) {
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    if (!WinHttpReceiveResponse(hr, NULL)) { WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    std::string buf; DWORD r = 0; char tmp[4096];
    while (WinHttpReadData(hr, tmp, sizeof(tmp)-1, &r) && r > 0) {
        tmp[r] = 0; buf += tmp; size_t nl;
        while ((nl = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, nl); buf.erase(0, nl + 1);
            if (line.empty()) continue;
            // Extract status + progress (total/completed)
            auto sp = line.find("\"status\":\"");
            if (sp != std::string::npos) {
                sp += 10; auto ep = line.find('"', sp);
                std::string sts = line.substr(sp, ep - sp);
                std::wstring wsts; int wl2 = MultiByteToWideChar(CP_UTF8, 0, sts.c_str(), (int)sts.size(), NULL, 0);
                if (wl2 > 0) { wsts.resize(wl2); MultiByteToWideChar(CP_UTF8, 0, sts.c_str(), (int)sts.size(), &wsts[0], wl2); }
                // progress calc
                auto tp = line.find("\"total\":");
                auto cp = line.find("\"completed\":");
                if (tp != std::string::npos && cp != std::string::npos) {
                    tp += 8; auto te = line.find_first_of(",}", tp);
                    cp += 12; auto ce = line.find_first_of(",}", cp);
                    if (te!=std::string::npos && ce!=std::string::npos) {
                        try {
                            long long total = _atoi64(line.substr(tp, te-tp).c_str());
                            long long completed = _atoi64(line.substr(cp, ce-cp).c_str());
                            int pct = 0;
                            if (total>0) pct = (int)(completed*100/total);
                            if (pct<0) pct=0; if (pct>100) pct=100;
                            PostMessage(hwnd, WM_MODEL_PROGRESS, (WPARAM)pct, 0);
                            wchar_t prog[64]; swprintf(prog, L" (%d%%)", pct);
                            wsts += prog;
                        } catch(...){}
                    }
                }
                std::wstring msg = L"[PULL] " + wsts + L"\n";
                PostMessage(hwnd, WM_HTTP_CHUNK, 0, (LPARAM)new std::wstring(msg));
                if (sts == "success") {
                    PostMessage(hwnd, WM_MODEL_PROGRESS, 100, 0);
                    PostMessage(hwnd, WM_HTTP_CHUNK, 1, (LPARAM)new std::wstring(L"[PULL] Done\n"));
                }
                if (line.find("\"error\"") != std::string::npos) PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(u2w(sts)));
            }
        }
    }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    PostMessage(hwnd, WM_HTTP_DONE, 1, (LPARAM)new std::wstring(L"[PULL] Finished"));
    return 0;
}

// ===== Embed helper (Stage3) =====
static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
inline std::string Base64Encode(const std::string& data) {
    std::string out; int val=0, valb=-6;
    for (unsigned char c : data) { val=(val<<8)+c; valb+=8; while(valb>=0){ out.push_back(b64chars[(val>>valb)&0x3F]); valb-=6; } }
    if (valb>-6) out.push_back(b64chars[((val<<8)>>(valb+8))&0x3F]);
    while(out.size()%4) out.push_back('=');
    return out;
}
inline std::string FileToBase64(const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h==INVALID_HANDLE_VALUE) return "";
    DWORD sz = GetFileSize(h, NULL); if (sz==INVALID_FILE_SIZE || sz==0){ CloseHandle(h); return ""; }
    std::string buf; buf.resize(sz); DWORD rd=0; ReadFile(h, buf.data(), sz, &rd, NULL); CloseHandle(h); buf.resize(rd);
    return Base64Encode(buf);
}
DWORD WINAPI EmbedThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t txt[1024], model[256];
    GetDlgItemText(hwnd, ID_MODEL_EMBED_INPUT, txt, 1024);
    GetDlgItemText(hwnd, ID_MODEL_NAME, model, 256);
    if (wcslen(txt)==0) { PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"Embed text empty")); return 0; }
    if (model[0]==0) wcscpy(model, g_cfg.model.c_str());
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp!=std::wstring::npos) url=url.substr(0,sp);
    url+=L"/embed";
    bool https = (url.find(L"https://")==0);
    std::wstring host, path; int port=https?443:80;
    size_t s=url.find(L"://"); if(s==std::wstring::npos) return 0;
    size_t st=s+3, sl=url.find(L'/',st); std::wstring hp=(sl==std::wstring::npos)?url.substr(st):url.substr(st,sl-st);
    path=(sl==std::wstring::npos)?L"/":url.substr(sl); size_t co=hp.find(L':'); if(co!=std::wstring::npos){host=hp.substr(0,co); port=_wtoi(hp.substr(co+1).c_str());} else host=hp;
    HINTERNET hs=WinHttpOpen(L"GOIDA/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs) return 0; HINTERNET hc=WinHttpConnect(hs,host.c_str(),(INTERNET_PORT)port,0);
    if(!hc){WinHttpCloseHandle(hs);return 0;}
    HINTERNET hr=WinHttpOpenRequest(hc,L"POST",path.c_str(),NULL,NULL,NULL,https?WINHTTP_FLAG_SECURE:0);
    if(!hr){WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    WinHttpSetTimeouts(hr,10000,10000,10000,10000);
    std::string j="{\"model\":\""; std::string mb=goida::utf8::w2utf8(model); std::string inp=goida::utf8::w2utf8(txt);
    j+=mb; j+="\",\"input\":\""; // escape minimal
    for(char c: inp){ if(c=='"') j+="\\\""; else if(c=='\\') j+="\\\\"; else if(c=='\n') j+="\\n"; else j+=c; }
    j+="\"}";
    LPCWSTR hdrs=L"Content-Type: application/json";
    if(!WinHttpSendRequest(hr,hdrs,(DWORD)wcslen(hdrs),(LPVOID)j.data(),(DWORD)j.size(),(DWORD)j.size(),0)){WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    if(!WinHttpReceiveResponse(hr,NULL)){WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    std::string resp; DWORD r=0; char tmp[4096]; while(WinHttpReadData(hr,tmp,sizeof(tmp)-1,&r)&&r>0){tmp[r]=0; resp+=tmp;}
    WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);
    std::wstring wresp = goida::utf8::utf8_to_w(resp);
    PostMessage(hwnd, WM_MODEL_EMBED_DONE, 0, (LPARAM)new std::wstring(wresp));
    PostMessage(hwnd, WM_HTTP_DONE, 1, (LPARAM)new std::wstring(L"[EMBED] done вЂ” check log"));
    return 0;
}

// ===== Delete model =====
DWORD WINAPI DeleteModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t model[256]; GetDlgItemText(hwnd, ID_CHAT_MODEL, model, 256);
    if (model[0] == 0) { PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"No model")); return 0; }
    std::wstring url = g_cfg.api_url;
    size_t sp = url.rfind('/'); if (sp != std::wstring::npos) url = url.substr(0, sp);
    url += L"/delete";
    bool https = (url.find(L"https://") == 0);
    std::wstring host, path; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) return 0;
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    path = (sl == std::wstring::npos) ? L"/" : url.substr(sl);
    size_t co = hp.find(L':'); if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) return 0;
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"DELETE", path.c_str(), NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    WinHttpSetTimeouts(hr, 10000, 10000, 10000, 10000);
    std::string jbody = "{\"name\":\""; int wl = WideCharToMultiByte(CP_UTF8, 0, model, -1, NULL, 0, NULL, NULL);
    if (wl > 0) { std::string mb; mb.resize(wl-1); WideCharToMultiByte(CP_UTF8, 0, model, -1, &mb[0], wl, NULL, NULL); jbody += mb; }
    jbody += "\"}";
    LPCWSTR hdrs = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hr, hdrs, (DWORD)wcslen(hdrs), (LPVOID)jbody.data(), (DWORD)jbody.size(), (DWORD)jbody.size(), 0)) {
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    if (!WinHttpReceiveResponse(hr, NULL)) { WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    DWORD sc = 0; DWORD scs = sizeof(sc);
    WinHttpQueryHeaders(hr, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &sc, &scs, NULL);
    std::string resp; DWORD r = 0; char tmp[4096];
    while (WinHttpReadData(hr, tmp, sizeof(tmp)-1, &r) && r > 0) { tmp[r] = 0; resp += tmp; }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    std::wstring result;
    if (sc >= 200 && sc < 300) {
        result = std::wstring(L"[DEL] Deleted: ") + model;
    } else {
        int wl2 = MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), NULL, 0);
        if (wl2 > 0) { std::wstring wr; wr.resize(wl2); MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), &wr[0], wl2);
            result = L"[DEL] Error: " + wr; }
    }
    PostMessage(hwnd, WM_HTTP_DONE, 1, (LPARAM)new std::wstring(result));
    return 0;
}

// ===== Show model info =====
DWORD WINAPI ShowModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t model[256]; GetDlgItemText(hwnd, ID_CHAT_MODEL, model, 256);
    if (model[0] == 0) { PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"No model")); return 0; }
    std::wstring url = g_cfg.api_url;
    size_t sp = url.rfind('/'); if (sp != std::wstring::npos) url = url.substr(0, sp);
    url += L"/show";
    bool https = (url.find(L"https://") == 0);
    std::wstring host, path; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) return 0;
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    path = (sl == std::wstring::npos) ? L"/" : url.substr(sl);
    size_t co = hp.find(L':'); if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co+1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) return 0;
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"POST", path.c_str(), NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    WinHttpSetTimeouts(hr, 10000, 10000, 10000, 10000);
    std::string jbody = "{\"name\":\""; int wl = WideCharToMultiByte(CP_UTF8, 0, model, -1, NULL, 0, NULL, NULL);
    if (wl > 0) { std::string mb; mb.resize(wl-1); WideCharToMultiByte(CP_UTF8, 0, model, -1, &mb[0], wl, NULL, NULL); jbody += mb; }
    jbody += "\"}";
    LPCWSTR hdrs = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hr, hdrs, (DWORD)wcslen(hdrs), (LPVOID)jbody.data(), (DWORD)jbody.size(), (DWORD)jbody.size(), 0)) {
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    if (!WinHttpReceiveResponse(hr, NULL)) { WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    std::string resp; DWORD r = 0; char tmp[4096];
    while (WinHttpReadData(hr, tmp, sizeof(tmp)-1, &r) && r > 0) { tmp[r] = 0; resp += tmp; }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    std::wstring result = std::wstring(L"[INFO] ") + model + L":\n";
    // Pretty-print JSON
    int wl3 = MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), NULL, 0);
    if (wl3 > 0) { std::wstring wr; wr.resize(wl3); MultiByteToWideChar(CP_UTF8, 0, resp.c_str(), (int)resp.size(), &wr[0], wl3);
        // Replace commas with newlines for readability
        for (size_t i = 0; i < wr.size(); i++) if (wr[i] == L',') wr[i] = L'\n';
        result += wr; }
    PostMessage(hwnd, WM_HTTP_DONE, 1, (LPARAM)new std::wstring(result));
    return 0;
}

// ===== Copy model =====
DWORD WINAPI CopyModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t src[256], dst[256]; GetDlgItemText(hwnd, ID_MODEL_NAME, src, 256); GetDlgItemText(hwnd, ID_MODEL_DST, dst, 256);
    if (src[0]==0||dst[0]==0) { PostMessage(hwnd, WM_HTTP_ERR,0,(LPARAM)new std::wstring(L"Need source & destination")); return 0; }
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp!=std::wstring::npos) url = url.substr(0,sp);
    url += L"/copy"; bool https = (url.find(L"https://")==0);
    std::wstring host,path; int port = https?443:80;
    size_t s = url.find(L"://"); if (s==std::wstring::npos) return 0;
    size_t st=s+3,sl=url.find(L'/',st); std::wstring hp = (sl==std::wstring::npos)?url.substr(st):url.substr(st,sl-st);
    path = (sl==std::wstring::npos)?L"/":url.substr(sl); size_t co=hp.find(L':');
    if (co!=std::wstring::npos){host=hp.substr(0,co);port=_wtoi(hp.substr(co+1).c_str());}else host=hp;
    HINTERNET hs=WinHttpOpen(L"GOIDA/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs)return 0; HINTERNET hc=WinHttpConnect(hs,host.c_str(),(INTERNET_PORT)port,0);
    if(!hc){WinHttpCloseHandle(hs);return 0;}
    HINTERNET hr=WinHttpOpenRequest(hc,L"POST",path.c_str(),NULL,NULL,NULL,https?WINHTTP_FLAG_SECURE:0);
    if(!hr){WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    WinHttpSetTimeouts(hr,10000,10000,10000,10000);
    std::string s1,s2; int wl;
    wl=WideCharToMultiByte(CP_UTF8,0,src,-1,NULL,0,NULL,NULL);if(wl>0){s1.resize(wl-1);WideCharToMultiByte(CP_UTF8,0,src,-1,&s1[0],wl,NULL,NULL);}
    wl=WideCharToMultiByte(CP_UTF8,0,dst,-1,NULL,0,NULL,NULL);if(wl>0){s2.resize(wl-1);WideCharToMultiByte(CP_UTF8,0,dst,-1,&s2[0],wl,NULL,NULL);}
    std::string jb = "{\"source\":\""+s1+"\",\"destination\":\""+s2+"\"}";
    LPCWSTR hdrs=L"Content-Type: application/json";
    if(!WinHttpSendRequest(hr,hdrs,(DWORD)wcslen(hdrs),(LPVOID)jb.data(),(DWORD)jb.size(),(DWORD)jb.size(),0))
    {WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    if(!WinHttpReceiveResponse(hr,NULL)){WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    DWORD sc=0,scs=sizeof(sc); WinHttpQueryHeaders(hr,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&sc,&scs,NULL);
    WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);
    std::wstring res = std::wstring(L"[COPY] ") + src + L" -> " + dst + L" : " + ((sc>=200&&sc<300)?L"OK":L"Failed");
    PostMessage(hwnd,WM_HTTP_DONE,1,(LPARAM)new std::wstring(res));
    return 0;
}

// ===== Create model =====
DWORD WINAPI CreateModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t model[256], from[256]; GetDlgItemText(hwnd, ID_MODEL_NAME, model, 256); GetDlgItemText(hwnd, ID_MODEL_DST, from, 256);
    if (model[0]==0) { PostMessage(hwnd, WM_HTTP_ERR,0,(LPARAM)new std::wstring(L"Need model name")); return 0; }
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp!=std::wstring::npos) url = url.substr(0,sp);
    url += L"/create"; bool https = (url.find(L"https://")==0);
    std::wstring host,path; int port = https?443:80;
    size_t s = url.find(L"://"); if (s==std::wstring::npos) return 0;
    size_t st=s+3,sl=url.find(L'/',st); std::wstring hp = (sl==std::wstring::npos)?url.substr(st):url.substr(st,sl-st);
    path = (sl==std::wstring::npos)?L"/":url.substr(sl); size_t co=hp.find(L':');
    if (co!=std::wstring::npos){host=hp.substr(0,co);port=_wtoi(hp.substr(co+1).c_str());}else host=hp;
    HINTERNET hs=WinHttpOpen(L"GOIDA/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs)return 0; HINTERNET hc=WinHttpConnect(hs,host.c_str(),(INTERNET_PORT)port,0);
    if(!hc){WinHttpCloseHandle(hs);return 0;}
    HINTERNET hr=WinHttpOpenRequest(hc,L"POST",path.c_str(),NULL,NULL,NULL,https?WINHTTP_FLAG_SECURE:0);
    if(!hr){WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    WinHttpSetTimeouts(hr,120000,120000,120000,120000);
    std::string s1,s2; int wl;
    wl=WideCharToMultiByte(CP_UTF8,0,model,-1,NULL,0,NULL,NULL);if(wl>0){s1.resize(wl-1);WideCharToMultiByte(CP_UTF8,0,model,-1,&s1[0],wl,NULL,NULL);}
    if (from[0]) {
        wl=WideCharToMultiByte(CP_UTF8,0,from,-1,NULL,0,NULL,NULL);if(wl>0){s2.resize(wl-1);WideCharToMultiByte(CP_UTF8,0,from,-1,&s2[0],wl,NULL,NULL);}
    }
    std::string jb = "{\"model\":\""+s1+"\",\"from\":\""+(from[0]?s2:"")+"\",\"stream\":true}";
    LPCWSTR hdrs=L"Content-Type: application/json";
    if(!WinHttpSendRequest(hr,hdrs,(DWORD)wcslen(hdrs),(LPVOID)jb.data(),(DWORD)jb.size(),(DWORD)jb.size(),0))
    {WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    if(!WinHttpReceiveResponse(hr,NULL)){WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    std::string buf; DWORD r=0; char tmp[4096];
    while(WinHttpReadData(hr,tmp,sizeof(tmp)-1,&r)&&r>0){tmp[r]=0;buf+=tmp;size_t nl;
        while((nl=buf.find('\n'))!=std::string::npos){std::string line=buf.substr(0,nl);buf.erase(0,nl+1);
            if (line.empty()) continue;
            auto sp=line.find("\"status\":\""); if (sp!=std::string::npos){sp+=10;auto ep=line.find('"',sp);
                std::wstring msg = L"[CREATE] " + u2w(line.substr(sp,ep-sp)) + L"\n";
                PostMessage(hwnd,WM_HTTP_CHUNK,1,(LPARAM)new std::wstring(msg));}
            if (line.find("\"error\"")!=std::string::npos) PostMessage(hwnd,WM_HTTP_ERR,1,(LPARAM)new std::wstring(u2w(line)));
        }
    }
    WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);
    PostMessage(hwnd,WM_HTTP_DONE,1,(LPARAM)new std::wstring(L"[CREATE] Done"));
    return 0;
}

// ===== Push model =====
DWORD WINAPI PushModelThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    wchar_t model[256]; GetDlgItemText(hwnd, ID_MODEL_NAME, model, 256);
    if (model[0] == 0) { PostMessage(hwnd, WM_HTTP_ERR, 0, (LPARAM)new std::wstring(L"No model")); return 0; }
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp != std::wstring::npos) url = url.substr(0, sp);
    url += L"/push"; bool https = (url.find(L"https://") == 0);
    std::wstring host, path; int port = https ? 443 : 80;
    size_t s = url.find(L"://"); if (s == std::wstring::npos) return 0;
    size_t st = s + 3, sl = url.find(L'/', st);
    std::wstring hp = (sl == std::wstring::npos) ? url.substr(st) : url.substr(st, sl - st);
    path = (sl == std::wstring::npos) ? L"/" : url.substr(sl);
    size_t co = hp.find(L':'); if (co != std::wstring::npos) { host = hp.substr(0, co); port = _wtoi(hp.substr(co + 1).c_str()); } else host = hp;
    HINTERNET hs = WinHttpOpen(L"GOIDA/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hs) return 0;
    HINTERNET hc = WinHttpConnect(hs, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hc) { WinHttpCloseHandle(hs); return 0; }
    HINTERNET hr = WinHttpOpenRequest(hc, L"POST", path.c_str(), NULL, NULL, NULL, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    WinHttpSetTimeouts(hr, 120000, 120000, 120000, 120000);
    std::string s1; int wl = WideCharToMultiByte(CP_UTF8, 0, model, -1, NULL, 0, NULL, NULL);
    if (wl > 0) { s1.resize(wl - 1); WideCharToMultiByte(CP_UTF8, 0, model, -1, &s1[0], wl, NULL, NULL); }
    std::string jb = "{\"model\":\"" + s1 + "\",\"stream\":true}";
    LPCWSTR hdrs = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hr, hdrs, (DWORD)wcslen(hdrs), (LPVOID)jb.data(), (DWORD)jb.size(), (DWORD)jb.size(), 0)) {
        WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    if (!WinHttpReceiveResponse(hr, NULL)) { WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return 0; }
    std::string buf; DWORD r = 0; char tmp[4096];
    while (WinHttpReadData(hr, tmp, sizeof(tmp) - 1, &r) && r > 0) {
        tmp[r] = 0; buf += tmp; size_t nl;
        while ((nl = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, nl); buf.erase(0, nl + 1);
            if (line.empty()) continue;
            auto sp2 = line.find("\"status\":\"");
            if (sp2 != std::string::npos) {
                sp2 += 10; auto ep = line.find('"', sp2);
                std::wstring msg = L"[PUSH] " + u2w(line.substr(sp2, ep - sp2)) + L"\n";
                PostMessage(hwnd, WM_HTTP_CHUNK, 1, (LPARAM)new std::wstring(msg));
            }
            if (line.find("\"error\"") != std::string::npos) PostMessage(hwnd, WM_HTTP_ERR, 1, (LPARAM)new std::wstring(u2w(line)));
        }
    }
    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    PostMessage(hwnd, WM_HTTP_DONE, 1, (LPARAM)new std::wstring(L"[PUSH] Finished"));
    return 0;
}

// ===== List running models =====
DWORD WINAPI RunningThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp!=std::wstring::npos) url = url.substr(0,sp);
    url += L"/ps"; bool https = (url.find(L"https://")==0);
    std::wstring host,path; int port = https?443:80;
    size_t s = url.find(L"://"); if (s==std::wstring::npos) return 0;
    size_t st=s+3,sl=url.find(L'/',st); std::wstring hp = (sl==std::wstring::npos)?url.substr(st):url.substr(st,sl-st);
    path = (sl==std::wstring::npos)?L"/":url.substr(sl); size_t co=hp.find(L':');
    if (co!=std::wstring::npos){host=hp.substr(0,co);port=_wtoi(hp.substr(co+1).c_str());}else host=hp;
    HINTERNET hs=WinHttpOpen(L"GOIDA/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs)return 0; HINTERNET hc=WinHttpConnect(hs,host.c_str(),(INTERNET_PORT)port,0);
    if(!hc){WinHttpCloseHandle(hs);return 0;}
    HINTERNET hr=WinHttpOpenRequest(hc,L"GET",path.c_str(),NULL,NULL,NULL,https?WINHTTP_FLAG_SECURE:0);
    if(!hr){WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    WinHttpSetTimeouts(hr,5000,5000,5000,5000);
    std::string resp;
    if(WinHttpSendRequest(hr,WINHTTP_NO_ADDITIONAL_HEADERS,0,NULL,0,0,0)&&WinHttpReceiveResponse(hr,NULL)){
        DWORD r=0;char tmp[4096];while(WinHttpReadData(hr,tmp,sizeof(tmp)-1,&r)&&r>0){tmp[r]=0;resp+=tmp;}
    }
    WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);
    std::wstring result = L"[RUNNING]\n";
    if (resp.find("\"models\":[")!=std::string::npos && resp.find("\"models\":[]")==std::string::npos) {
        auto p=resp.find("\"name\":\""); int count=0;
        while(p!=std::string::npos){p+=8;auto e=resp.find('"',p);if(e==std::string::npos)break;
            result += L"  \u25CF " + u2w(resp.substr(p,e-p)) + L"\n"; count++;
            p=resp.find("\"name\":\"",e);}
        if (count==0) result += L"  (none)\n";
    } else result += L"  (no running models)\n";
    PostMessage(hwnd,WM_HTTP_DONE,1,(LPARAM)new std::wstring(result));
    return 0;
}

// ===== Server version =====
std::wstring g_srvVersion = L"unknown";
DWORD WINAPI VersionThr(LPVOID lp) {
    HWND hwnd = (HWND)lp;
    std::wstring url = g_cfg.api_url; size_t sp = url.rfind('/'); if (sp!=std::wstring::npos) url = url.substr(0,sp);
    url += L"/version"; bool https = (url.find(L"https://")==0);
    std::wstring host,path; int port = https?443:80;
    size_t s = url.find(L"://"); if (s==std::wstring::npos) return 0;
    size_t st=s+3,sl=url.find(L'/',st); std::wstring hp = (sl==std::wstring::npos)?url.substr(st):url.substr(st,sl-st);
    path = (sl==std::wstring::npos)?L"/":url.substr(sl); size_t co=hp.find(L':');
    if (co!=std::wstring::npos){host=hp.substr(0,co);port=_wtoi(hp.substr(co+1).c_str());}else host=hp;
    HINTERNET hs=WinHttpOpen(L"GOIDA/2.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs)return 0; HINTERNET hc=WinHttpConnect(hs,host.c_str(),(INTERNET_PORT)port,0);
    if(!hc){WinHttpCloseHandle(hs);return 0;}
    HINTERNET hr=WinHttpOpenRequest(hc,L"GET",path.c_str(),NULL,NULL,NULL,https?WINHTTP_FLAG_SECURE:0);
    if(!hr){WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);return 0;}
    WinHttpSetTimeouts(hr,5000,5000,5000,5000); std::string resp;
    if(WinHttpSendRequest(hr,WINHTTP_NO_ADDITIONAL_HEADERS,0,NULL,0,0,0)&&WinHttpReceiveResponse(hr,NULL)){
        DWORD r=0;char tmp[4096];while(WinHttpReadData(hr,tmp,sizeof(tmp)-1,&r)&&r>0){tmp[r]=0;resp+=tmp;}
    }
    WinHttpCloseHandle(hr);WinHttpCloseHandle(hc);WinHttpCloseHandle(hs);
    auto p=resp.find("\"version\":\""); if (p!=std::string::npos){p+=11;auto e=resp.find('"',p);
        g_srvVersion = u2w(resp.substr(p,e-p));}
    PostMessage(hwnd,WM_HTTP_DONE,1,(LPARAM)new std::wstring(L"[VERSION] Ollama " + g_srvVersion));
    return 0;
}

// ===== UI helpers =====
HWND Btn(HWND p, int id, const wchar_t* t, int x, int y, int w, int h) {
    return CreateWindowEx(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND Edt(HWND p, int id, int x, int y, int w, int h) {
    return CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND EdtM(HWND p, int id, int x, int y, int w, int h) {
    return CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND Lbl(HWND p, const wchar_t* t, int x, int y, int w) {
    HWND h = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y, w, 20, p, NULL, g_hInst, NULL);
    if (h) SendMessage(h, WM_SETFONT, (WPARAM)g_fontUI, 0);
    return h;
}
// Accent-tinted section header (bold, brighter text)
HWND SecLbl(HWND p, const wchar_t* t, int x, int y, int w) {
    HWND h = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y, w, 20, p, NULL, g_hInst, NULL);
    if (h) { SendMessage(h, WM_SETFONT, (WPARAM)g_fontBold, 0); SetProp(h, L"GOIDA_SEC", (HANDLE)1); }
    return h;
}
HWND Lbx(HWND p, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, x, y, w, h, p, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (hw) SendMessage(hw, WM_SETFONT, (WPARAM)g_fontUI, 0);
    return hw;
}
void DestroyPageControls() {
    std::vector<HWND> kill;
    HWND child = NULL;
    while ((child = FindWindowEx(g_hWnd, child, NULL, NULL)) != NULL) {
        int id = GetDlgCtrlID(child);
        if (!(id >= ID_NAV_CHAT && id <= ID_NAV_ABOUT) && id != 0)
            kill.push_back(child);
    }
    for (HWND h : kill) DestroyWindow(h);
}

void Lg(const wchar_t* s, const wchar_t* m) {
    std::wstring l = std::wstring(s) + L": " + m;
    g_logCache.push_back(l);
    DBLog(s, m);
    HWND lb = GetDlgItem(g_hWnd, ID_CONS_LOG);
    if (lb) { SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)l.c_str());
        int c = (int)SendMessage(lb, LB_GETCOUNT, 0, 0);
        SendMessage(lb, LB_SETTOPINDEX, c - 1, 0); }
}

// ===== Drawing helpers =====
void DrawSep(HWND hwnd, int x, int y, int w, COLORREF c) {
    HDC hdc = GetDC(hwnd);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL); LineTo(hdc, x + w, y);
    SelectObject(hdc, old); DeleteObject(pen);
    ReleaseDC(hwnd, hdc);
}

// ===== Nav button drawing =====
static const wchar_t* NAV_ICONS[] = { L"\u25B6", L"\u270F", L"\u263A", L"\u2601", L"\u2699", L"\u2691", L"\u2139" };
static const wchar_t* NAV_LABELS[] = { L"Chat", L"Trainer", L"Profiles", L"Memory", L"Console", L"Models", L"About" };

void DrawBtn(HDC hdc, RECT r, const wchar_t* text, bool active, bool hover, bool down) {
    COLORREF bg = active ? g_colAccent : (down ? RGB(30,30,33) : (hover ? COL_HOVER : COL_NAV));
    HBRUSH br = CreateSolidBrush(bg);
    if (g_cfg.rounded) {
        HRGN rg = CreateRoundRectRgn(r.left, r.top, r.right, r.bottom, 12, 12);
        FillRgn(hdc, rg, br); DeleteObject(rg);
    } else FillRect(hdc, &r, br);
    DeleteObject(br);
    if (active) {
        RECT bar = {r.left, r.top, r.left + 3, r.bottom};
        HBRUSH ba = CreateSolidBrush(RGB(255,255,255)); FillRect(hdc, &bar, ba); DeleteObject(ba);
    }
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, g_fontUI);
    SetTextColor(hdc, active ? RGB(255,255,255) : (hover ? COL_TEXT : COL_TEXT_DIM));
    wchar_t buf[128]; int idx = text - NAV_LABELS[0];
    for (int i = 0; i < 7; i++) if (text == NAV_LABELS[i]) { idx = i; break; }
    if (idx >= 0 && idx < 7) swprintf(buf, L"%s  %s", NAV_ICONS[idx], NAV_LABELS[idx]);
    else wcscpy(buf, text);
    RECT tr = {r.left + 16, r.top, r.right - 8, r.bottom};
    DrawTextW(hdc, buf, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
}

void DrawNavBtn(DRAWITEMSTRUCT* di) {
    DrawBtn(di->hDC, di->rcItem, NAV_LABELS[di->itemID - ID_NAV_CHAT],
        (di->itemID - ID_NAV_CHAT == g_curPage), (g_hoverBtn == di->itemID), (g_downBtn == di->itemID));
}

// ===== Chat =====
void ApplyRichFormat(HWND h, int start, int end, DWORD mask, DWORD effects, COLORREF color, int yHeight, const wchar_t* face) {
    CHARFORMAT2 cf={0}; cf.cbSize=sizeof(cf); cf.dwMask=mask; cf.dwEffects=effects;
    if (mask & CFM_COLOR) cf.crTextColor=color;
    if (mask & CFM_SIZE) cf.yHeight=yHeight;
    if (mask & CFM_FACE && face) wcscpy(cf.szFaceName, face);
    SendMessage(h, EM_SETSEL, start, end);
    SendMessage(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
void AppendChat(const wchar_t* t) {
    HWND h = GetDlgItem(g_hWnd, ID_CHAT_HIST); if (!h) return;
    wchar_t cls[64]={0}; GetClassNameW(h, cls, 64);
    bool isRich = (wcsstr(cls, L"RichEdit")!=NULL || wcsstr(cls, L"RICHEDIT")!=NULL);
    int start = GetWindowTextLength(h);
    SendMessage(h, EM_REPLACESEL, 0, (LPARAM)t);
    int end = GetWindowTextLength(h);
    int len = (int)wcslen(t);
    if (!isRich || len==0) {
        SendMessage(h, EM_SETSEL, end, end); SendMessage(h, EM_SCROLLCARET, 0, 0);
        return;
    }
    // Color prefixes
    if (wcsncmp(t, L">> User:", 8)==0) {
        ApplyRichFormat(h, start, start+8, CFM_COLOR|CFM_BOLD, CFE_BOLD, COL_MSG_USER, 0, NULL);
        // rest of line default
        ApplyRichFormat(h, start+8, end, CFM_COLOR, 0, COL_TEXT, 0, NULL);
    } else if (wcsncmp(t, L"<< AI:", 6)==0) {
        ApplyRichFormat(h, start, start+6, CFM_COLOR|CFM_BOLD, CFE_BOLD, COL_MSG_AI, 0, NULL);
        ApplyRichFormat(h, start+6, end, CFM_COLOR, 0, COL_TEXT, 0, NULL);
    } else if (wcsncmp(t, L">> ", 3)==0) {
        ApplyRichFormat(h, start, start+len, CFM_COLOR, 0, COL_MSG_USER, 0, NULL);
    } else if (wcsncmp(t, L"<< ", 3)==0) {
        ApplyRichFormat(h, start, start+len, CFM_COLOR, 0, COL_MSG_AI, 0, NULL);
    }
    // Simple markdown inline parsing inside inserted range
    std::wstring txt(t);
    // headers: lines starting with # 
    // We'll handle by scanning txt for lines
    // For simplicity, apply larger size for header lines
    size_t pos=0, cur=start;
    while (pos < txt.size()) {
        size_t eol = txt.find(L'\n', pos);
        if (eol==std::wstring::npos) eol=txt.size();
        std::wstring line = txt.substr(pos, eol-pos);
        int lineStart = cur;
        int lineEnd = cur + (int)line.size();
        if (!line.empty() && line[0]==L'#') {
            int lvl=0; while(lvl<(int)line.size() && line[lvl]==L'#') lvl++;
            if (lvl>0 && lvl<6 && line.size()>(size_t)lvl && line[lvl]==L' ') {
                // header: bigger + bold + accent color
                int sz = 260 - lvl*20; if (sz<180) sz=180;
                ApplyRichFormat(h, lineStart, lineEnd, CFM_SIZE|CFM_BOLD|CFM_COLOR, CFE_BOLD, RGB(120,180,255), sz, NULL);
            }
        } else if (!line.empty() && line.rfind(L"- ",0)==0) {
            ApplyRichFormat(h, lineStart, lineStart+2, CFM_COLOR, 0, COL_WARN, 0, NULL);
        } else if (!line.empty() && line.rfind(L"> ",0)==0) {
            ApplyRichFormat(h, lineStart, lineEnd, CFM_COLOR|CFM_ITALIC, CFE_ITALIC, COL_TEXT_DIM, 0, NULL);
        }
        cur = lineEnd + 1; // +1 for \n
        pos = (eol==txt.size()? eol : eol+1);
    }
    // Inline **bold**
    {
        std::wstring wtxt = txt;
        int base = start;
        size_t p=0;
        while ((p=wtxt.find(L"**", p)) != std::wstring::npos) {
            size_t q = wtxt.find(L"**", p+2);
            if (q==std::wstring::npos) break;
            int s = base + (int)p + 2;
            int e = base + (int)q;
            ApplyRichFormat(h, s, e, CFM_BOLD, CFE_BOLD, COL_TEXT, 0, NULL);
            // remove markers visually? Keep them but bold interior
            p = q+2;
        }
    }
    // Inline `code`
    {
        std::wstring wtxt = txt;
        int base = start;
        size_t p=0;
        while ((p=wtxt.find(L'`', p)) != std::wstring::npos) {
            size_t q = wtxt.find(L'`', p+1);
            if (q==std::wstring::npos) break;
            int s = base + (int)p + 1;
            int e = base + (int)q;
            ApplyRichFormat(h, s, e, CFM_COLOR|CFM_FACE, 0, COL_WARN, 0, L"Consolas");
            // also back ticks color dim
            ApplyRichFormat(h, base+(int)p, base+(int)p+1, CFM_COLOR, 0, COL_TEXT_DIM, 0, NULL);
            ApplyRichFormat(h, base+(int)q, base+(int)q+1, CFM_COLOR, 0, COL_TEXT_DIM, 0, NULL);
            p = q+1;
        }
    }
    // Reset selection to end
    SendMessage(h, EM_SETSEL, end, end); SendMessage(h, EM_SCROLLCARET, 0, 0);
    // ensure default format for next insert
    ApplyRichFormat(h, end, end, CFM_COLOR, 0, COL_TEXT, 0, NULL);
}

// Read current generation params from chat page controls (falls back to config)
void ReadChatParams(HWND h) {
    HWND sl = GetDlgItem(h, ID_CHAT_TEMP);
    if (sl) { int pos = (int)SendMessage(sl, TBM_GETPOS, 0, 0); g_cfg.temp = pos / 100.0f; }
    wchar_t tk[32]; if (GetDlgItemText(h, ID_CHAT_TOKENS, tk, 32) > 0) { int n = _wtoi(tk); if (n > 0) g_cfg.max_tokens = n; }
    wchar_t sd[32]; if (GetDlgItemText(h, ID_CHAT_SEED, sd, 32) > 0) g_cfg.seed = _wtoi64(sd);
    wchar_t sp[4096]; if (GetDlgItemText(h, ID_CHAT_SYS, sp, 4096) > 0) g_cfg.sys_prompt = sp;
}

std::wstring ChatOpts() {
    std::wstring o = L"{\"temperature\":" + std::to_wstring(g_cfg.temp) + L",\"num_predict\":" + std::to_wstring(g_cfg.max_tokens);
    if (g_cfg.seed >= 0) o += L",\"seed\":" + std::to_wstring(g_cfg.seed);
    o += L"}";
    return o;
}

std::wstring ChatJson(const wchar_t* m, const wchar_t* u, bool stream) {
    // Sync the system message with current sys_prompt
    if (!g_history.empty() && g_history[0].first == L"system") g_history[0].second = g_cfg.sys_prompt;
    // Build messages with optional images on last user entry
    std::wstring ms;
    for (size_t i=0;i<g_history.size();++i) {
        auto &x = g_history[i];
        std::wstring entry = L"{\"role\":\"" + x.first + L"\",\"content\":\"" + JEsc(x.second) + L"\"";
        if (i+1==g_history.size() && x.first==L"user" && !g_pendingImages.empty()) {
            entry += L",\"images\":[";
            for (size_t j=0;j<g_pendingImages.size();++j) {
                std::wstring wb = goida::utf8::utf8_to_w(g_pendingImages[j]);
                entry += L"\"" + wb + L"\"";
                if (j+1<g_pendingImages.size()) entry+=L",";
            }
            entry += L"]";
        }
        entry += L"}";
        ms += entry + L",";
    }
    std::wstring ss = stream ? L"true" : L"false";
    std::wstring fmt = g_cfg.json_mode ? L",\"format\":\"json\"" : L"";
    std::wstring ka = GetActiveKeepAlive();
    std::wstring keep = ka.empty()? L"" : L",\"keep_alive\":\"" + ka + L"\"";
    std::wstring tools = (g_toolsEnabled && !g_toolsJson.empty()) ? L",\"tools\":" + g_toolsJson : L"";
    if (std::wstring(u).find(L"/api/generate") != std::wstring::npos) {
        std::wstring p; for (auto& x : g_history) p += x.first + L": " + x.second + L"\n";
        return L"{\"model\":\"" + JEsc(m) + L"\",\"prompt\":\"" + JEsc(p) + L"\",\"stream\":" + ss + fmt + keep + tools + L",\"options\":" + ChatOpts() + L"}";
    }
    if (!ms.empty()) ms.pop_back();
    return L"{\"model\":\"" + JEsc(m) + L"\",\"messages\":[" + ms + L"],\"stream\":" + ss + fmt + keep + tools + L",\"options\":" + ChatOpts() + L"}";
}

void DoSend(HWND h) {
    wchar_t buf[8192]; GetDlgItemText(h, ID_CHAT_INPUT, buf, 8192); if (wcslen(buf) == 0) return;
    ReadChatParams(h);
    std::wstring userMsg = buf;
    if (!g_pendingImages.empty()) { userMsg += L" [images:" + std::to_wstring(g_pendingImages.size()) + L"]"; }
    g_history.push_back({L"user", buf}); AppendChat(L">> User: "); AppendChat(buf);
    if (!g_pendingImages.empty()) AppendChat(L" [with image]");
    if (g_toolsEnabled) AppendChat(L" [tools on]");
    AppendChat(L"\n<< AI: ");
    SetDlgItemText(h, ID_CHAT_INPUT, L""); g_streamBuf.clear();
    wchar_t model[256], url[512]; GetDlgItemText(h, ID_CHAT_MODEL, model, 256); GetDlgItemText(h, ID_CHAT_API, url, 512);
    SetDlgItemText(h, ID_CHAT_STAT, L"Streaming...");
    EnableWindow(GetDlgItem(h, ID_CHAT_SEND), FALSE); EnableWindow(GetDlgItem(h, ID_CHAT_INPUT), FALSE);
    g_startTick = GetTickCount64();
    std::wstring j = ChatJson(model, url, true);
    // clear pending images after building json (one-shot)
    g_pendingImages.clear();
    CreateThread(NULL, 0, HttpThr, new HttpD{url, j, h, 1}, 0, NULL);
}

LRESULT CALLBACK InputProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) { DoSend(GetParent(h)); return 0; }
    return CallWindowProc(g_origInputProc, h, m, w, l);
}

// ===== Pages =====
void ShowChat(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    SecLbl(parent, L"\u25B6 Chat  [RichEdit markdown]", x, y, 360);
    y += 28; CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    // Load RichEdit library on demand
    static HMODULE hRich = LoadLibraryW(L"Msftedit.dll");
    HWND hist = CreateWindowEx(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL,
        x, y, right - x, 300, parent, (HMENU)ID_CHAT_HIST, g_hInst, NULL);
    // Fallback to plain EDIT if RichEdit not available
    if (!hist) hist = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL, x, y, right - x, 300, parent, (HMENU)ID_CHAT_HIST, g_hInst, NULL);
    if (hist) {
        SendMessage(hist, EM_SETBKGNDCOLOR, 0, COL_EDIT_BG);
        // default char format
        CHARFORMAT2 cf={0}; cf.cbSize=sizeof(cf); cf.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE; cf.crTextColor=COL_TEXT; wcscpy(cf.szFaceName, L"Consolas"); cf.yHeight=180;
        SendMessage(hist, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        SendMessage(hist, EM_SETEVENTMASK, 0, ENM_LINK);
        SendMessage(hist, EM_AUTOURLDETECT, 1, 0);
    }
    y += 310;
    HWND inp = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        x, y, right - x - 144, 56, parent, (HMENU)ID_CHAT_INPUT, g_hInst, NULL);
    g_origInputProc = (WNDPROC)SetWindowLongPtr(inp, GWLP_WNDPROC, (LONG_PTR)InputProc);
    Btn(parent, ID_CHAT_SEND, L"\u2192 Send", right - 134, y, 130, 40); y += 42;
    Btn(parent, ID_CHAT_STOP, L"\u25A0 Stop", right - 134, y, 130, 22); y += 30;
    Btn(parent, ID_CHAT_CLEAR, L"\u2716 Clear", x, y, 84, 26);
    Btn(parent, ID_CHAT_SAVE, L"\u2B07 DB", x + 90, y, 64, 26);
    Btn(parent, ID_CHAT_LOAD, L"\u2B06 DB", x + 160, y, 64, 26);
    Btn(parent, ID_CHAT_TEST, L"\u2699 Test", x + 230, y, 68, 26);
    Btn(parent, ID_CHAT_CTXCLR, L"\u267B Ctx", x + 304, y, 64, 26);
    Btn(parent, ID_CHAT_SAVEAS, L"\u2B07 TXT", x + 374, y, 76, 26);
    Btn(parent, ID_CHAT_PULL, L"\u2B07 Pull", x, y + 34, 60, 26);
    Btn(parent, ID_CHAT_DEL, L"\u2718 Del", x + 66, y + 34, 58, 26);
    Btn(parent, ID_CHAT_INFO, L"\u2139 Info", x + 130, y + 34, 58, 26);
    Btn(parent, ID_CHAT_REG, L"\u21BA Reg", x + 194, y + 34, 60, 26);
    Btn(parent, ID_CHAT_BRANCH, L"\u2B60 Branch", x + 260, y + 34, 76, 26);
    Btn(parent, ID_CHAT_COPYLAST, L"\u2398 Copy", x + 342, y + 34, 66, 26);
    Lbl(parent, L"Status:", right - 190, y + 37, 44);
    CreateWindowEx(0, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_NOTIFY, right - 146, y + 37, 140, 20, parent, (HMENU)ID_CHAT_STAT, g_hInst, NULL);
    y += 68;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 8;
    Lbl(parent, L"API:", x, y, 30); Edt(parent, ID_CHAT_API, x + 30, y, 240, 24);
    Lbl(parent, L"Model:", x + 280, y, 40);
    CreateWindowEx(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + 320, y, 140, 24, parent, (HMENU)ID_CHAT_MODEL, g_hInst, NULL);
    SetDlgItemText(parent, ID_CHAT_MODEL, g_cfg.model.c_str());
    Btn(parent, ID_CHAT_REFRESH, L"\u21BB", x + 464, y - 1, 26, 26);
    Btn(parent, ID_CHAT_CONN, L"Apply", x + 494, y - 1, 72, 26);
    y += 32;
    Lbl(parent, L"Temp:", x, y + 3, 40);
    CreateWindowEx(0, L"TRACKBAR", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, x + 44, y, 130, 26, parent, (HMENU)ID_CHAT_TEMP, g_hInst, NULL);
    SendMessage(GetDlgItem(parent, ID_CHAT_TEMP), TBM_SETRANGE, 1, MAKELPARAM(0, 150));
    SendMessage(GetDlgItem(parent, ID_CHAT_TEMP), TBM_SETPOS, 1, (int)(g_cfg.temp * 100));
    CreateWindowEx(0, L"STATIC", L"0.70", WS_CHILD | WS_VISIBLE | SS_NOTIFY, x + 180, y + 3, 44, 20, parent, (HMENU)ID_CHAT_TEMPV, g_hInst, NULL);
    Lbl(parent, L"Tokens:", x + 230, y + 3, 50); Edt(parent, ID_CHAT_TOKENS, x + 284, y, 60, 24);
    SetDlgItemText(parent, ID_CHAT_TOKENS, std::to_wstring(g_cfg.max_tokens).c_str());
    Lbl(parent, L"Seed:", x + 354, y + 3, 40); Edt(parent, ID_CHAT_SEED, x + 394, y, 80, 24);
    SetDlgItemText(parent, ID_CHAT_SEED, g_cfg.seed >= 0 ? std::to_wstring(g_cfg.seed).c_str() : L"-1");
    Btn(parent, ID_CHAT_JSON, g_cfg.json_mode ? L"JSON ON" : L"JSON OFF", x + 486, y - 2, 84, 26);
    y += 34;
    Lbl(parent, L"System:", x, y + 3, 50);
    CreateWindowEx(0, L"EDIT", g_cfg.sys_prompt.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + 56, y, right - x - 56, 24, parent, (HMENU)ID_CHAT_SYS, g_hInst, NULL);
    SetDlgItemText(parent, ID_CHAT_API, g_cfg.api_url.c_str());
    AppendChat(L"=== GOIDA v2.1 ===\n"); AppendChat(L"API: "); AppendChat(g_cfg.api_url.c_str()); AppendChat(L"\nModel: "); AppendChat(g_cfg.model.c_str()); AppendChat(L"\n\n");
    g_history.push_back({L"system", g_cfg.sys_prompt});
    SetDlgItemText(parent, ID_CHAT_STAT, L"Checking...");
    g_connStatus = CONN_UNKNOWN;
    DWORD tid; CreateThread(NULL, 0, ConnCheckThr, g_hWnd, 0, &tid);
}

void ShowTrain(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    SecLbl(parent, L"\u270F Prompt Trainer", x, y, 200); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    Lbl(parent, L"System:", x, y, 60); Edt(parent, ID_TRAIN_SYS, x + 60, y, right - x - 60, 24); y += 32;
    Lbl(parent, L"Prompt:", x, y, 60); y += 20;
    EdtM(parent, ID_TRAIN_PROMPT, x, y, 608, 80); y += 90;
    Btn(parent, ID_TRAIN_GEN, L"Generate", x, y, 100, 28); Btn(parent, ID_TRAIN_SAVE, L"Save", x + 108, y, 80, 28);
    Btn(parent, ID_TRAIN_LOAD, L"Load", x + 196, y, 80, 28); Btn(parent, ID_TRAIN_REPORT, L"Report", x + 284, y, 80, 28);
    y += 36;
    Lbl(parent, L"Temp: 0.70", x, y, 100);
    CreateWindowEx(0, L"TRACKBAR", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, x + 100, y, 150, 26, parent, (HMENU)ID_TRAIN_TEMP, g_hInst, NULL);
    SendMessage(GetDlgItem(parent, ID_TRAIN_TEMP), TBM_SETRANGE, 1, MAKELPARAM(0, 100));
    SendMessage(GetDlgItem(parent, ID_TRAIN_TEMP), TBM_SETPOS, 1, (int)(g_cfg.temp * 100));
    Lbl(parent, L"Tokens:", x + 260, y, 50); Edt(parent, ID_TRAIN_TOKENS, x + 310, y, 60, 24);
    SetDlgItemText(parent, ID_TRAIN_TOKENS, std::to_wstring(g_cfg.max_tokens).c_str()); y += 34;
    Lbl(parent, L"Output:", x, y, 80); Btn(parent, ID_TRAIN_COPY, L"Copy", x + 80, y - 2, 70, 22); y += 20;
    EdtM(parent, ID_TRAIN_OUT, x, y, right - x, 100); y += 110;
    Lbl(parent, L"Saved prompts:", x, y, 120); y += 18;
    HWND lb = Lbx(parent, ID_TRAIN_LIST, x, y, 608, 50);
    sqlite3_stmt* s = DBPrep("SELECT name,prompt FROM prompts ORDER BY created_at DESC LIMIT 50");
    if (s) { while (sqlite3_step(s) == SQLITE_ROW) {
        const char* n = (const char*)sqlite3_column_text(s, 0);
        const char* p = (const char*)sqlite3_column_text(s, 1);
        if (p) { int wl = MultiByteToWideChar(CP_UTF8, 0, p, -1, NULL, 0);
            if (wl > 0) { std::wstring ws; ws.resize(wl-1); MultiByteToWideChar(CP_UTF8, 0, p, -1, &ws[0], wl);
                std::wstring ns; if (n && *n) { int wln = MultiByteToWideChar(CP_UTF8, 0, n, -1, NULL, 0); if (wln > 0) { ns.resize(wln-1); MultiByteToWideChar(CP_UTF8, 0, n, -1, &ns[0], wln); } }
                std::wstring tag = ns.empty() ? L"" : (ns + L": "); tag += ws;
                SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)tag.c_str()); } }
    } sqlite3_finalize(s); }
}

void ShowFemboy(HWND parent) {
    // Now Profiles page (Femboy -> Profiles migration)
    EnsureDefaultProfile();
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    SecLbl(parent, L"\u263A Profiles  (L1:5m L2:30m L3:1h)", x, y, 320); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    // List
    HWND lb = Lbx(parent, ID_PROF_LIST, x, y, right - x, 110); y += 118;
    auto profiles = LoadAllProfiles();
    for (auto &p : profiles) {
        std::wstring item = p.name + L" [" + p.keep_alive + L"] " + std::to_wstring(p.max_tokens) + L"tok";
        SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
    // Edit fields
    Lbl(parent, L"Name:", x, y, 50); Edt(parent, ID_PROF_NAME, x + 56, y, 180, 24);
    Lbl(parent, L"Avatar:", x + 260, y, 50); Edt(parent, ID_PROF_AVATAR, x + 314, y, right - x - 314, 24); y += 30;
    Lbl(parent, L"System:", x, y, 50); Edt(parent, ID_PROF_SYS, x + 56, y, right - x - 56, 24); y += 30;
    // Temp slider
    Lbl(parent, L"Temp:", x, y + 3, 40);
    HWND tr = CreateWindowEx(0, L"TRACKBAR", L"", WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS, x+44, y, 130, 26, parent, (HMENU)ID_PROF_TEMP, g_hInst, NULL);
    SendMessage(tr, TBM_SETRANGE, 1, MAKELPARAM(0,150));
    SendMessage(tr, TBM_SETPOS, 1, (int)(g_cfg.temp*100));
    CreateWindowEx(0, L"STATIC", L"0.70", WS_CHILD|WS_VISIBLE|SS_NOTIFY, x+180, y+3, 40,20, parent, (HMENU)ID_PROF_TEMPV, g_hInst, NULL);
    Lbl(parent, L"Tokens:", x+230, y+3, 50); Edt(parent, ID_PROF_TOKENS, x+284, y, 60,24);
    SetDlgItemText(parent, ID_PROF_TOKENS, std::to_wstring(g_cfg.max_tokens).c_str());
    // KeepAlive combo
    Lbl(parent, L"Keep:", x+360, y+3, 40);
    HWND cb = CreateWindowEx(0, L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, x+400, y, 90, 100, parent, (HMENU)ID_PROF_KEEP, g_hInst, NULL);
    SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"L1 - 5m");
    SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"L2 - 30m");
    SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"L3 - 1h");
    SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)L"Off");
    SendMessage(cb, CB_SETCURSEL, 0, 0);
    y += 34;
    // Buttons
    Btn(parent, ID_PROF_CREATE, L"+ Create", x, y, 84, 28);
    Btn(parent, ID_PROF_SAVE, L"\u2714 Save", x+92, y, 84,28);
    Btn(parent, ID_PROF_DEL, L"\u2716 Delete", x+184, y, 84,28);
    Btn(parent, ID_PROF_LOAD, L"\u2B06 Load", x+276, y, 84,28);
    // Keep legacy femboy quick save for compat (hidden but functional via ID_FEMBOY_SAVE)
    Btn(parent, ID_FEMBOY_SAVE, L"Femboy Save", x+370, y, 96,28);
    y += 36;
    // Legacy femboy details collapsed into avatar field вЂ” show hint
    Lbl(parent, L"Tips: Profiles keep_alive L1/L2/L3 controls Ollama keep_alive. Limit 100 memories.", x, y, 600);
    // Pre-fill first profile if exists
    if (!profiles.empty()) {
        SetDlgItemText(parent, ID_PROF_NAME, profiles[0].name.c_str());
        SetDlgItemText(parent, ID_PROF_SYS, profiles[0].system_prompt.c_str());
        SetDlgItemText(parent, ID_PROF_AVATAR, profiles[0].avatar.c_str());
        wchar_t tb[32]; swprintf(tb, L"%.2f", profiles[0].temp);
        SetDlgItemText(parent, ID_PROF_TEMPV, tb);
        SendMessage(tr, TBM_SETPOS, 1, (int)(profiles[0].temp*100));
        SetDlgItemText(parent, ID_PROF_TOKENS, std::to_wstring(profiles[0].max_tokens).c_str());
        int lvl = LevelFromKeepAlive(profiles[0].keep_alive);
        int sel = (lvl==1?0:lvl==2?1:lvl==3?2:3);
        SendMessage(cb, CB_SETCURSEL, sel, 0);
    }
}

void ShowMemory(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    int cnt = MemoryCount();
    wchar_t title[128]; swprintf(title, L"\u2601 Memory Store  (%d/100)", cnt);
    SecLbl(parent, title, x, y, 300); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    Lbx(parent, ID_MEM_LIST, x, y, 608, 200); y += 210;
    Lbl(parent, L"Tag:", x, y, 50); Edt(parent, ID_MEM_TAG, x + 50, y, 180, 22); y += 30;
    Lbl(parent, L"Value:", x, y, 50); Edt(parent, ID_MEM_VAL, x + 50, y, 400, 22); y += 32;
    Btn(parent, ID_MEM_ADD, L"+ Add", x + 60, y, 90, 28); Btn(parent, ID_MEM_DEL, L"\u2716 Delete", x + 160, y, 90, 28);
    // Count indicator
    wchar_t cc[64]; swprintf(cc, L"%d/100 used", cnt);
    CreateWindowEx(0, L"STATIC", cc, WS_CHILD|WS_VISIBLE|SS_NOTIFY, x+280, y+4, 120,20, parent, (HMENU)ID_MEM_COUNT, g_hInst, NULL);
    if (cnt>=100) {
        Lbl(parent, L"Limit reached (100). Delete old entries.", x, y+28, 400);
    }
    sqlite3_stmt* s = DBPrep("SELECT tag,value FROM memory ORDER BY created_at DESC LIMIT 100");
    if (s) { while (sqlite3_step(s) == SQLITE_ROW) {
        const char* t = (const char*)sqlite3_column_text(s, 0);
        const char* v = (const char*)sqlite3_column_text(s, 1);
        if (t && v) {
            std::wstring ws = goida::utf8::utf8_to_w(t);
            ws += L"|";
            ws += goida::utf8::utf8_to_w(v);
            SendMessage(GetDlgItem(parent, ID_MEM_LIST), LB_ADDSTRING, 0, (LPARAM)ws.c_str());
        }
    } sqlite3_finalize(s); }
}

void ShowConsole(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    SecLbl(parent, L"\u2699 Console / Logs", x, y, 200); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    Lbx(parent, ID_CONS_LOG, x, y, 608, 340); y += 350;
    Btn(parent, ID_CONS_CLR, L"Clear", x, y, 90, 26);
    Btn(parent, ID_CONS_RELOAD, L"Reload", x + 100, y, 100, 26);
    Btn(parent, ID_CONS_UPDATE, L"Update", x + 210, y, 100, 26);
    Btn(parent, ID_CONS_AUTO, L"Auto-start", x + 320, y, 100, 26); y += 34;
    HKEY hk; wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH); bool autoOn = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        wchar_t v[MAX_PATH]; DWORD sz = sizeof(v);
        if (RegQueryValueExW(hk, L"GOIDA_Launcher", NULL, NULL, (LPBYTE)v, &sz) == ERROR_SUCCESS) autoOn = (wcscmp(v, exe) == 0);
        RegCloseKey(hk); }
    SetDlgItemText(parent, ID_CONS_AUTO, autoOn ? L"Auto: ON" : L"Auto: OFF");
    Lg(L"SYSTEM", L"GOIDA v2.1 started");
    sqlite3_stmt* s = DBPrep("SELECT source,message FROM logs ORDER BY id DESC LIMIT 100");
    if (s) { std::vector<std::wstring> rev;
        while (sqlite3_step(s) == SQLITE_ROW) {
            const char* src = (const char*)sqlite3_column_text(s, 0);
            const char* msg = (const char*)sqlite3_column_text(s, 1);
            if (src && msg) {
                int wl = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
                if (wl > 0) { std::wstring ws; ws.resize(wl-1); MultiByteToWideChar(CP_UTF8, 0, src, -1, &ws[0], wl);
                    ws += L": "; int wl2 = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
                    if (wl2 > 0) { int old = (int)ws.size(); ws.resize(old + wl2 - 1);
                        MultiByteToWideChar(CP_UTF8, 0, msg, -1, &ws[old], wl2); }
                    rev.push_back(ws); } }
        } sqlite3_finalize(s);
        g_logCache.clear();
        for (auto it = rev.rbegin(); it != rev.rend(); ++it) {
            g_logCache.push_back(*it);
            SendMessage(GetDlgItem(parent, ID_CONS_LOG), LB_ADDSTRING, 0, (LPARAM)it->c_str());
        }
    }
    int c = (int)SendMessage(GetDlgItem(parent, ID_CONS_LOG), LB_GETCOUNT, 0, 0);
    if (c > 0) SendMessage(GetDlgItem(parent, ID_CONS_LOG), LB_SETTOPINDEX, c - 1, 0);
}

void ShowModels(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    int w = right - x;
    SecLbl(parent, L"\u2691 Models вЂ” Local + Marketplace", x, y, 340); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, w, 2, parent, NULL, g_hInst, NULL); y += 10;
    // Model name and status
    Lbl(parent, L"Model:", x, y, 44); Edt(parent, ID_MODEL_NAME, x + 46, y, 180, 24);
    Lbl(parent, L"From/To:", x + 236, y, 56); Edt(parent, ID_MODEL_DST, x + 294, y, 160, 24);
    CreateWindowEx(0, L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_NOTIFY, right - 100, y + 3, 100, 20, parent, (HMENU)ID_MODEL_STAT, g_hInst, NULL);
    y += 32;
    // Action buttons row 1
    Btn(parent, ID_MODEL_PULL, L"\u2B07 Pull", x, y, 72, 24);
    Btn(parent, ID_MODEL_DEL, L"\u2718 Del", x + 78, y, 68, 24);
    Btn(parent, ID_MODEL_INFO, L"\u2139 Info", x + 152, y, 64, 24);
    Btn(parent, ID_MODEL_COPY, L"\u25C6 Copy", x + 222, y, 70, 24);
    Btn(parent, ID_MODEL_CREATE, L"\u2728 Create", x + 298, y, 74, 24);
    Btn(parent, ID_MODEL_RUNNING, L"\u25B6 Run", x + 378, y, 70, 24);
    Btn(parent, ID_MODEL_PUSH, L"\u2191 Push", x + 454, y, 60, 24);
    Btn(parent, ID_MODEL_REFRESH, L"\u21BB", x + 520, y - 1, 26, 26);
    y += 32;
    // Progress bar
    Lbl(parent, L"Progress:", x, y + 3, 60);
    HWND prog = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD|WS_VISIBLE|PBS_SMOOTH, x+66, y+4, w-70, 14, parent, (HMENU)ID_MODEL_PROGRESS, g_hInst, NULL);
    SendMessage(prog, PBM_SETRANGE, 0, MAKELPARAM(0,100));
    SendMessage(prog, PBM_SETPOS, 0, 0);
    y += 22;
    // Local ListView
    Lbl(parent, L"Local models (ListView):", x, y, 200); y += 18;
    HWND lv = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, x, y, w, 86, parent, (HMENU)ID_MODEL_LIST, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
    LVCOLUMN col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH;
    col.pszText=(LPWSTR)L"Name"; col.cx=220; ListView_InsertColumn(lv,0,&col);
    col.pszText=(LPWSTR)L"Size"; col.cx=110; ListView_InsertColumn(lv,1,&col);
    col.pszText=(LPWSTR)L"Modified"; col.cx=140; ListView_InsertColumn(lv,2,&col);
    col.pszText=(LPWSTR)L"Family/Status"; col.cx=140; ListView_InsertColumn(lv,3,&col);
    y += 92;
    // Marketplace
    Lbl(parent, L"Marketplace (popular):", x, y, 200);
    Btn(parent, ID_MODEL_MKT_INSTALL, L"\u2B07 Install", right - 86, y-2, 86, 22); y += 18;
    HWND mkt = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, x, y, w, 70, parent, (HMENU)ID_MODEL_MKT_LIST, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(mkt, LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    col.pszText=(LPWSTR)L"Marketplace"; col.cx=240; ListView_InsertColumn(mkt,0,&col);
    col.pszText=(LPWSTR)L"Size hint"; col.cx=100; ListView_InsertColumn(mkt,1,&col);
    col.pszText=(LPWSTR)L"Use"; col.cx=260; ListView_InsertColumn(mkt,2,&col);
    // populate marketplace static
    struct Mk{ const wchar_t* name; const wchar_t* sz; const wchar_t* use; } mk[] = {
        {L"qwen:14b", L"9 GB", L"General chat"},
        {L"llama3:8b", L"4.7 GB", L"General"},
        {L"mistral:7b", L"4.1 GB", L"Instruct"},
        {L"gemma2:9b", L"5.5 GB", L"Gemma"},
        {L"phi3:mini", L"2.2 GB", L"Small/fast"},
        {L"codellama:13b", L"7 GB", L"Code"},
        {L"nomic-embed-text", L"274 MB", L"Embeddings"},
        {L"llava:13b", L"9.5 GB", L"Vision (images)"}
    };
    for (int i=0;i<8;i++) {
        LVITEM it={0}; it.mask=LVIF_TEXT; it.iItem=i;
        it.pszText=(LPWSTR)mk[i].name; it.iSubItem=0; ListView_InsertItem(mkt,&it);
        ListView_SetItemText(mkt,i,1,(LPWSTR)mk[i].sz);
        ListView_SetItemText(mkt,i,2,(LPWSTR)mk[i].use);
    }
    y += 76;
    // Embed / Images / Tools quick demo
    Lbl(parent, L"Embed:", x, y+3, 44); Edt(parent, ID_MODEL_EMBED_INPUT, x+46, y, 260, 24);
    Btn(parent, ID_MODEL_EMBED, L"Embed", x+312, y-1, 70, 26);
    Btn(parent, ID_MODEL_TOOLS, L"Tools demo", x+388, y-1, 90, 26);
    Lbl(parent, L"Tools/Images handled via /api/chat", x+488, y+3, 200);
    y += 28;
    Lbl(parent, L"Pull uses Model; Copy/Create uses Model+From/To. Progress bar shows download. Images/tools via Chat.", x, y, 700);
}

void ShowAbout(HWND parent) {
    DestroyPageControls();
    RECT cr; GetClientRect(parent, &cr);
    int x = 216, y = 12, right = cr.right - 16;
    SecLbl(parent, L"About + Personalization", x, y, 320); y += 28;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 10;
    // Info block centered
    int cx = 380; int cy = y;
    SecLbl(parent, L"GOIDA AI MANAGER", cx, cy, 300); cy += 24;
    Lbl(parent, L"Version 2.1 \u2014 SQLite + Clean UI + Foundation", cx, cy, 400); cy += 20;
    Lbl(parent, L"Single-process | Win32 Native | Dark Theme", cx, cy, 500); cy += 24;
    CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, cx, cy, 260, 2, parent, NULL, g_hInst, NULL); cy += 14;
    wchar_t buf[512]; swprintf(buf, L"Model: %s | Temp: %.2f | KeepAlive: %s", g_cfg.model.c_str(), g_cfg.temp, GetActiveKeepAlive().c_str());
    Lbl(parent, buf, cx, cy, 500); cy += 20;
    swprintf(buf, L"API: %s", g_cfg.api_url.c_str()); Lbl(parent, buf, cx, cy, 500); cy += 20;
    swprintf(buf, L"Server: Ollama %s | Lang: %s | Theme: %s", g_srvVersion.c_str(), g_cfg.lang.c_str(), g_cfg.theme.c_str()); Lbl(parent, buf, cx, cy, 500); cy += 20;
    swprintf(buf, L"Profile: %s | Accent: %s", g_cfg.f_name.c_str(), g_cfg.accent.c_str()); Lbl(parent, buf, cx, cy, 500); cy += 20;
    Lbl(parent, L"\u00a9 2026 GOIDA AI", cx, cy, 300); cy += 30;
    DWORD tid; CreateThread(NULL, 0, VersionThr, g_hWnd, 0, &tid);
    // Personalization controls in main content area (x=216)
    SecLbl(parent, L"\u2630 Personalization", x, y, 200);
    y += 24; CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, right - x, 2, parent, NULL, g_hInst, NULL); y += 12;
    // Theme
    Lbl(parent, L"Theme:", x, y+3, 60);
    HWND cbTheme = CreateWindowEx(0, L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, x+66, y, 110, 100, parent, (HMENU)ID_SET_THEME, g_hInst, NULL);
    SendMessage(cbTheme, CB_ADDSTRING, 0, (LPARAM)L"dark");
    SendMessage(cbTheme, CB_ADDSTRING, 0, (LPARAM)L"amoled");
    SendMessage(cbTheme, CB_ADDSTRING, 0, (LPARAM)L"light");
    int ti = (g_cfg.theme==L"amoled"?1:(g_cfg.theme==L"light"?2:0));
    SendMessage(cbTheme, CB_SETCURSEL, ti, 0);
    // Accent
    Lbl(parent, L"Accent:", x+190, y+3, 50);
    HWND cbAccent = CreateWindowEx(0, L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, x+240, y, 110, 100, parent, (HMENU)ID_SET_ACCENT, g_hInst, NULL);
    SendMessage(cbAccent, CB_ADDSTRING, 0, (LPARAM)L"blue");
    SendMessage(cbAccent, CB_ADDSTRING, 0, (LPARAM)L"purple");
    SendMessage(cbAccent, CB_ADDSTRING, 0, (LPARAM)L"green");
    SendMessage(cbAccent, CB_ADDSTRING, 0, (LPARAM)L"orange");
    SendMessage(cbAccent, CB_ADDSTRING, 0, (LPARAM)L"pink");
    int ai=0; if(g_cfg.accent==L"purple") ai=1; else if(g_cfg.accent==L"green") ai=2; else if(g_cfg.accent==L"orange") ai=3; else if(g_cfg.accent==L"pink") ai=4;
    SendMessage(cbAccent, CB_SETCURSEL, ai, 0);
    // Language
    Lbl(parent, L"Lang:", x+370, y+3, 40);
    HWND cbLang = CreateWindowEx(0, L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL, x+410, y, 80, 100, parent, (HMENU)ID_SET_LANG, g_hInst, NULL);
    SendMessage(cbLang, CB_ADDSTRING, 0, (LPARAM)L"ru");
    SendMessage(cbLang, CB_ADDSTRING, 0, (LPARAM)L"en");
    SendMessage(cbLang, CB_SETCURSEL, (g_cfg.lang==L"en"?1:0), 0);
    y += 32;
    // Checkboxes for rounded + adaptive
    HWND ckRound = CreateWindowEx(0, L"BUTTON", L"Rounded corners", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, x, y, 160, 22, parent, (HMENU)ID_SET_ROUND, g_hInst, NULL);
    SendMessage(ckRound, BM_SETCHECK, g_cfg.rounded?BST_CHECKED:BST_UNCHECKED, 0);
    HWND ckAdapt = CreateWindowEx(0, L"BUTTON", L"Adaptive layout", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX, x+180, y, 160, 22, parent, (HMENU)ID_SET_ADAPTIVE, g_hInst, NULL);
    SendMessage(ckAdapt, BM_SETCHECK, g_cfg.adaptive?BST_CHECKED:BST_UNCHECKED, 0);
    y += 30;
    // Shortcuts legend
    SecLbl(parent, L"\u2328 Shortcuts", x, y, 200); y+=18;
    Lbl(parent, L"Ctrl+1..7: Pages | Ctrl+Enter: Send | Ctrl+R: Regenerate | Ctrl+B: Branch | Ctrl+L: Clear | F5: Refresh models | Esc: Stop", x, y, right - x);
    y+=22;
    Lbl(parent, L"Shift+Enter: New line | Shift+Click Tools: Image picker | Double-click model/profile to load", x, y, right - x);
    // Apply styling to checkboxes
    SendMessage(ckRound, WM_SETFONT, (WPARAM)g_fontUI, 0);
    SendMessage(ckAdapt, WM_SETFONT, (WPARAM)g_fontUI, 0);
}

void ShowPage(HWND hwnd, int page) {
    g_curPage = page;
    switch (page) {
        case 0: ShowChat(hwnd); break; case 1: ShowTrain(hwnd); break;
        case 2: ShowFemboy(hwnd); break; case 3: ShowMemory(hwnd); break;
        case 4: ShowConsole(hwnd); break; case 5: ShowModels(hwnd); break;
        case 6: ShowAbout(hwnd); break;
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

// ===== WndProc =====
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            for (int i = 0; i < 7; i++)
                Btn(hWnd, ID_NAV_CHAT + i, NAV_LABELS[i], 8, 60 + i * 44, 184, 36);
            ShowPage(hWnd, 0);
            break;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lParam;
            if (di->CtlType == ODT_BUTTON && di->itemID >= ID_NAV_CHAT && di->itemID <= ID_NAV_ABOUT)
                DrawNavBtn(di);
            else if (di->CtlType == ODT_BUTTON) {
                RECT r = di->rcItem; int id = (int)di->itemID;
                bool hover = (g_hoverBtn == id), down = (g_downBtn == id);
                bool accent = (id == ID_CHAT_SEND || id == ID_CHAT_PULL || id == ID_MODEL_PULL || id == ID_MODEL_COPY || id == ID_MODEL_CREATE || id == ID_MODEL_PUSH || id == ID_PROF_SAVE || id == ID_SET_ACCENT);
                COLORREF bg, border = COL_BORDER;
                COLORREF acc = g_colAccent;
                COLORREF accHover = RGB(GetRValue(acc)+20>255?255:GetRValue(acc)+20, GetGValue(acc)+20>255?255:GetGValue(acc)+20, GetBValue(acc)+20>255?255:GetBValue(acc)+20);
                COLORREF accDown = RGB(GetRValue(acc)>20?GetRValue(acc)-20:0, GetGValue(acc)>20?GetGValue(acc)-20:0, GetBValue(acc)>20?GetBValue(acc)-20:0);
                if (accent) { bg = down ? accDown : (hover ? accHover : acc); }
                else { bg = down ? RGB(30,30,33) : (hover ? COL_HOVER : COL_PANEL); }
                HBRUSH br = CreateSolidBrush(bg);
                if (g_cfg.rounded) {
                    HRGN rg = CreateRoundRectRgn(r.left, r.top, r.right+1, r.bottom+1, 10, 10);
                    FillRgn(di->hDC, rg, br); DeleteObject(rg);
                } else FillRect(di->hDC, &r, br);
                DeleteObject(br);
                if (!g_cfg.rounded) {
                    RECT fr = {r.left, r.top, r.right, r.bottom};
                    HBRUSH fb = CreateSolidBrush(border); FrameRect(di->hDC, &fr, fb); DeleteObject(fb);
                } else {
                    // rounded border
                    HPEN pen = CreatePen(PS_SOLID, 1, border);
                    HPEN oldPen = (HPEN)SelectObject(di->hDC, pen);
                    HBRUSH oldBr = (HBRUSH)SelectObject(di->hDC, GetStockObject(NULL_BRUSH));
                    RoundRect(di->hDC, r.left, r.top, r.right, r.bottom, 10, 10);
                    SelectObject(di->hDC, oldPen); SelectObject(di->hDC, oldBr); DeleteObject(pen);
                }
                SetBkMode(di->hDC, TRANSPARENT); HFONT old = (HFONT)SelectObject(di->hDC, g_fontUI);
                SetTextColor(di->hDC, accent ? RGB(255,255,255) : COL_TEXT);
                wchar_t txt[256]; GetWindowTextW(GetDlgItem(hWnd, id), txt, 256);
                RECT tr = {r.left + 8, r.top, r.right - 8, r.bottom};
                DrawTextW(di->hDC, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(di->hDC, old);
                return TRUE;
            }
            return TRUE;
        }
        case WM_MOUSEMOVE: {
            if (!g_tracking) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE | TME_HOVER, hWnd, HOVER_DEFAULT };
                TrackMouseEvent(&tme); g_tracking = true;
            }
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            int old = g_hoverBtn; g_hoverBtn = -1;
            HWND child = ChildWindowFromPoint(hWnd, pt);
            if (child) {
                wchar_t cls[64]; GetClassNameW(child, cls, 64);
                if (wcscmp(cls, L"Button") == 0) {
                    g_hoverBtn = GetDlgCtrlID(child);
                }
            }
            if (g_hoverBtn != old) InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_MOUSELEAVE: { g_tracking = false; if (g_hoverBtn != -1) { g_hoverBtn = -1; InvalidateRect(hWnd, NULL, FALSE); } break; }
        case WM_MOVE: { g_hoverBtn = -1; g_tracking = false; break; }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int notif = HIWORD(wParam);
            // Profiles list selection вЂ” auto-fill fields
            if (id == ID_PROF_LIST && (notif == LBN_SELCHANGE || notif == LBN_DBLCLK)) {
                HWND lb = GetDlgItem(hWnd, ID_PROF_LIST);
                int sel = lb ? (int)SendMessage(lb, LB_GETCURSEL, 0, 0) : LB_ERR;
                if (sel != LB_ERR) {
                    wchar_t buf[256]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf);
                    wchar_t* br = wcsstr(buf, L" ["); if (br) *br = 0;
                    auto all = LoadAllProfiles();
                    for (auto &pr : all) if (pr.name == buf) {
                        SetDlgItemText(hWnd, ID_PROF_NAME, pr.name.c_str());
                        SetDlgItemText(hWnd, ID_PROF_SYS, pr.system_prompt.c_str());
                        SetDlgItemText(hWnd, ID_PROF_AVATAR, pr.avatar.c_str());
                        SetDlgItemText(hWnd, ID_PROF_TOKENS, std::to_wstring(pr.max_tokens).c_str());
                        SendMessage(GetDlgItem(hWnd, ID_PROF_TEMP), TBM_SETPOS, 1, (int)(pr.temp*100));
                        wchar_t tb[32]; swprintf(tb, L"%.2f", pr.temp); SetDlgItemText(hWnd, ID_PROF_TEMPV, tb);
                        int lvl = LevelFromKeepAlive(pr.keep_alive);
                        int csel = (lvl==1?0:lvl==2?1:lvl==3?2:3);
                        SendMessage(GetDlgItem(hWnd, ID_PROF_KEEP), CB_SETCURSEL, csel, 0);
                        if (notif == LBN_DBLCLK) {
                            // double-click loads & activates
                            g_cfg.f_name = pr.name; g_cfg.sys_prompt = pr.system_prompt; g_cfg.temp = pr.temp; g_cfg.max_tokens = pr.max_tokens; g_cfg.Save();
                            DBSet(L"active_profile", pr.name.c_str());
                            Lg(L"PROFILE", (std::wstring(L"Activated: ")+pr.name).c_str());
                        }
                        break;
                    }
                }
                break;
            }
            if (id >= ID_NAV_CHAT && id <= ID_NAV_ABOUT) ShowPage(hWnd, id - ID_NAV_CHAT);
            else if (id == ID_CHAT_SEND) DoSend(hWnd);
            else if (id == ID_CHAT_STOP) {
                g_cancelStream = true;
                AppendChat(L"\n[Stopped]\n");
                EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), TRUE); EnableWindow(GetDlgItem(hWnd, ID_CHAT_INPUT), TRUE);
                SetDlgItemText(hWnd, ID_CHAT_STAT, L"Stopped");
            }
            else if (id == ID_CHAT_CLEAR) { SetDlgItemText(hWnd, ID_CHAT_HIST, L""); g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."}); AppendChat(L"=== Cleared ===\n"); }
            else if (id == ID_CHAT_SAVE) {
                sqlite3_stmt* s = DBPrep("INSERT INTO chat_messages(role,content) VALUES(?1,?2)");
                if (s) { for (auto& m : g_history) {
                    char r[64], c[65536]; WideCharToMultiByte(CP_UTF8, 0, m.first.c_str(), -1, r, sizeof(r), NULL, NULL);
                    WideCharToMultiByte(CP_UTF8, 0, m.second.c_str(), -1, c, sizeof(c), NULL, NULL);
                    sqlite3_bind_text(s, 1, r, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(s, 2, c, -1, SQLITE_TRANSIENT);
                    sqlite3_step(s); sqlite3_reset(s);
                } sqlite3_finalize(s); AppendChat(L"--- Chat saved to DB ---\n"); }
            }
            else if (id == ID_CHAT_LOAD) {
                SetDlgItemText(hWnd, ID_CHAT_HIST, L""); g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."});
                sqlite3_stmt* s = DBPrep("SELECT role,content FROM chat_messages ORDER BY id ASC LIMIT 100");
                if (s) { while (sqlite3_step(s) == SQLITE_ROW) {
                    const char* r = (const char*)sqlite3_column_text(s, 0);
                    const char* c = (const char*)sqlite3_column_text(s, 1);
                    if (r && c) {
                        int wl = MultiByteToWideChar(CP_UTF8, 0, r, -1, NULL, 0);
                        if (wl > 0) { std::wstring wr; wr.resize(wl-1); MultiByteToWideChar(CP_UTF8, 0, r, -1, &wr[0], wl);
                            int wl2 = MultiByteToWideChar(CP_UTF8, 0, c, -1, NULL, 0);
                            if (wl2 > 0) { std::wstring wc; wc.resize(wl2-1); MultiByteToWideChar(CP_UTF8, 0, c, -1, &wc[0], wl2);
                                g_history.push_back({wr, wc}); } }
                    }
                } sqlite3_finalize(s); }
                AppendChat(L"--- Chat loaded from DB ---\n");
                for (auto& m : g_history) {
                    if (m.first == L"user") AppendChat(L">> ");
                    else if (m.first == L"assistant") AppendChat(L"<< ");
                    else AppendChat(L"** ");
                    AppendChat(m.first.c_str()); AppendChat(L": "); AppendChat(m.second.c_str()); AppendChat(L"\n"); }
            }
            else if (id == ID_CHAT_TEST) { AppendChat(L"--- Testing...\n"); EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), FALSE);
                wchar_t url[512]; GetDlgItemText(hWnd, ID_CHAT_API, url, 512);
                CreateThread(NULL, 0, HttpThr, new HttpD{url, L"{\"model\":\"" + JEsc(g_cfg.model) + L"\",\"prompt\":\"ping\",\"stream\":false}", hWnd, 0}, 0, NULL); }
            else if (id == ID_CHAT_CTXCLR) { g_history.clear(); g_history.push_back({L"system", L"You are a helpful AI assistant."}); AppendChat(L"--- Context cleared ---\n"); }
            else if (id == ID_CHAT_SAVEAS) {
                wchar_t path[MAX_PATH] = {}; OPENFILENAMEW ofn = {sizeof(ofn), hWnd, NULL, L"Text Files\0*.txt\0All Files\0*.*\0", NULL, 0, 0, path, MAX_PATH, NULL, 0, NULL, L"Save Chat As", OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY};
                if (GetSaveFileNameW(&ofn)) {
                    std::wofstream f(path); if (f.is_open()) {
                        for (auto& m : g_history) f << m.first << L": " << m.second << L"\r\n";
                        f.close(); AppendChat(L"--- Saved to "); AppendChat(path); AppendChat(L" ---\n"); } } }
            else if (id == ID_CHAT_CONN) {
                wchar_t url[512], model[128]; GetDlgItemText(hWnd, ID_CHAT_API, url, 512); GetDlgItemText(hWnd, ID_CHAT_MODEL, model, 128);
                g_cfg.api_url = url; g_cfg.model = model; ReadChatParams(hWnd); g_cfg.Save(); AppendChat(L"--- Applied ---\n"); Lg(L"CONFIG", L"Updated");
                SetDlgItemText(hWnd, ID_CHAT_STAT, L"Checking..."); g_connStatus = CONN_UNKNOWN;
                DWORD tid2; CreateThread(NULL, 0, ConnCheckThr, g_hWnd, 0, &tid2); }
            else if (id == ID_CHAT_REFRESH) {
                SetDlgItemText(hWnd, ID_CHAT_MODEL, L""); // clear, first result will fill it
                DWORD tid3; CreateThread(NULL, 0, ModelListThr, g_hWnd, 0, &tid3); }
            else if (id == ID_TRAIN_GEN) {
                wchar_t sys[1024], prompt[4096], tokens[16]; GetDlgItemText(hWnd, ID_TRAIN_SYS, sys, 1024); GetDlgItemText(hWnd, ID_TRAIN_PROMPT, prompt, 4096);
                int pos = (int)SendMessage(GetDlgItem(hWnd, ID_TRAIN_TEMP), TBM_GETPOS, 0, 0);
                GetDlgItemText(hWnd, ID_TRAIN_TOKENS, tokens, 16); int mt = _wtoi(tokens); if (mt <= 0) mt = 4096;
                g_cfg.temp = pos / 100.0f; g_cfg.max_tokens = mt;
                std::wstring fp = std::wstring(sys) + L"\n\n" + prompt;
                std::wstring j = L"{\"model\":\"" + JEsc(g_cfg.model) + L"\",\"prompt\":\"" + JEsc(fp) + L"\",\"stream\":false,\"options\":" + ChatOpts() + L"}";
                SetDlgItemText(hWnd, ID_TRAIN_OUT, L"Generating..."); EnableWindow(GetDlgItem(hWnd, ID_TRAIN_GEN), FALSE);
                CreateThread(NULL, 0, HttpThr, new HttpD{g_cfg.api_url, j, hWnd, 0}, 0, NULL); }
            else if (id == ID_TRAIN_SAVE) { wchar_t buf[4096]; GetDlgItemText(hWnd, ID_TRAIN_PROMPT, buf, 4096);
                if (wcslen(buf) > 0) {
                    sqlite3_stmt* s = DBPrep("INSERT INTO prompts(name,prompt) VALUES(?1,?2)");
                    if (s) { char n[256], p[4096]; WideCharToMultiByte(CP_UTF8, 0, L"unnamed", -1, n, sizeof(n), NULL, NULL);
                        WideCharToMultiByte(CP_UTF8, 0, buf, -1, p, sizeof(p), NULL, NULL);
                        sqlite3_bind_text(s, 1, n, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(s, 2, p, -1, SQLITE_TRANSIENT);
                        sqlite3_step(s); sqlite3_finalize(s); }
                    std::wstring tag = L"unnamed: "; tag += buf;
                    SendMessage(GetDlgItem(hWnd, ID_TRAIN_LIST), LB_ADDSTRING, 0, (LPARAM)tag.c_str()); } }
            else if (id == ID_TRAIN_LOAD) { HWND lb = GetDlgItem(hWnd, ID_TRAIN_LIST); int sel = (int)SendMessage(lb, LB_GETCURSEL, 0, 0); if (sel != LB_ERR) { wchar_t buf[4096]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf); wchar_t* p = wcschr(buf, L':'); if (p) { p += 2; SetDlgItemText(hWnd, ID_TRAIN_PROMPT, p); } else SetDlgItemText(hWnd, ID_TRAIN_PROMPT, buf); } }
            else if (id == ID_TRAIN_COPY) {
                wchar_t buf[65536]; GetDlgItemText(hWnd, ID_TRAIN_OUT, buf, 65536);
                if (wcslen(buf) > 0) { int n = (int)wcslen(buf); HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t));
                    if (hg) { wchar_t* p = (wchar_t*)GlobalLock(hg); if (p) { wcscpy(p, buf); GlobalUnlock(hg);
                        OpenClipboard(hWnd); EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, hg); CloseClipboard(); }
                        else GlobalFree(hg); } SetDlgItemText(hWnd, ID_CHAT_STAT, L"Copied!"); } }
            else if (id == ID_TRAIN_REPORT) {
                wchar_t sys[1024], prompt[4096], tokens[16]; int pos = (int)SendMessage(GetDlgItem(hWnd, ID_TRAIN_TEMP), TBM_GETPOS, 0, 0);
                GetDlgItemText(hWnd, ID_TRAIN_SYS, sys, 1024); GetDlgItemText(hWnd, ID_TRAIN_PROMPT, prompt, 4096); GetDlgItemText(hWnd, ID_TRAIN_TOKENS, tokens, 16);
                wchar_t buf[8192]; swprintf(buf, L"=== REPORT ===\r\nSystem: %s\r\nPrompt: %s\r\nTemp: %.2f\r\nTokens: %s\r\nModel: %s\r\nAPI: %s\r\n==============", sys, prompt, pos/100.0f, tokens, g_cfg.model.c_str(), g_cfg.api_url.c_str());
                SetDlgItemText(hWnd, ID_TRAIN_OUT, buf); Lg(L"TRAIN", L"Report"); }
            else if (id == ID_FEMBOY_SAVE) {
                // Legacy femboy save вЂ” also sync to default profile for migration
                wchar_t buf[256];
                GetDlgItemText(hWnd, ID_FEMBOY_NAME, buf, 256); g_cfg.f_name = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_VOICE, buf, 256); g_cfg.f_voice = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_EYES, buf, 256); g_cfg.f_eyes = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_HAIR, buf, 256); g_cfg.f_hair = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_TOP, buf, 256); g_cfg.f_top = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_BOTTOM, buf, 256); g_cfg.f_bottom = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_FEET, buf, 256); g_cfg.f_feet = buf;
                GetDlgItemText(hWnd, ID_FEMBOY_EARS, buf, 256); g_cfg.f_ears = buf;
                g_cfg.Save(); Lg(L"CONFIG", L"Femboy saved"); MessageBox(hWnd, L"Saved!", L"GOIDA", MB_OK | MB_ICONINFORMATION);
            }
            else if (id == ID_PROF_CREATE) {
                wchar_t name[128], sys[1024], avatar[256], tok[32];
                GetDlgItemText(hWnd, ID_PROF_NAME, name, 128);
                GetDlgItemText(hWnd, ID_PROF_SYS, sys, 1024);
                GetDlgItemText(hWnd, ID_PROF_AVATAR, avatar, 256);
                GetDlgItemText(hWnd, ID_PROF_TOKENS, tok, 32);
                if (wcslen(name)==0) { MessageBox(hWnd, L"Name required", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                int pos = (int)SendMessage(GetDlgItem(hWnd, ID_PROF_TEMP), TBM_GETPOS, 0, 0);
                float temp = pos/100.0f;
                int tokens = _wtoi(tok); if (tokens<=0) tokens=4096;
                int sel = (int)SendMessage(GetDlgItem(hWnd, ID_PROF_KEEP), CB_GETCURSEL, 0, 0);
                std::wstring ka = (sel==0?L"5m":sel==1?L"30m":sel==2?L"1h":L"0");
                Profile p; p.name=name; p.avatar=avatar; p.system_prompt=sys; p.temp=temp; p.max_tokens=tokens; p.keep_alive=ka;
                if (SaveProfile(p)) {
                    Lg(L"PROFILE", (std::wstring(L"Created: ")+name).c_str());
                    // refresh list
                    HWND lb = GetDlgItem(hWnd, ID_PROF_LIST);
                    if (lb) {
                        SendMessage(lb, LB_RESETCONTENT, 0, 0);
                        auto all = LoadAllProfiles();
                        for (auto &pr : all) {
                            std::wstring item = pr.name + L" [" + pr.keep_alive + L"] " + std::to_wstring(pr.max_tokens) + L"tok";
                            SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
                        }
                    }
                    MessageBox(hWnd, L"Profile created", L"GOIDA", MB_OK|MB_ICONINFORMATION);
                }
            }
            else if (id == ID_PROF_SAVE) {
                wchar_t name[128], sys[1024], avatar[256], tok[32];
                GetDlgItemText(hWnd, ID_PROF_NAME, name, 128);
                GetDlgItemText(hWnd, ID_PROF_SYS, sys, 1024);
                GetDlgItemText(hWnd, ID_PROF_AVATAR, avatar, 256);
                GetDlgItemText(hWnd, ID_PROF_TOKENS, tok, 32);
                if (wcslen(name)==0) { MessageBox(hWnd, L"Select or enter name", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                int pos = (int)SendMessage(GetDlgItem(hWnd, ID_PROF_TEMP), TBM_GETPOS, 0, 0);
                float temp = pos/100.0f;
                int tokens = _wtoi(tok); if (tokens<=0) tokens=4096;
                int sel = (int)SendMessage(GetDlgItem(hWnd, ID_PROF_KEEP), CB_GETCURSEL, 0, 0);
                std::wstring ka = (sel==0?L"5m":sel==1?L"30m":sel==2?L"1h":L"0");
                Profile p; p.name=name; p.avatar=avatar; p.system_prompt=sys; p.temp=temp; p.max_tokens=tokens; p.keep_alive=ka;
                if (SaveProfile(p)) {
                    Lg(L"PROFILE", (std::wstring(L"Saved: ")+name).c_str());
                    // update g_cfg to reflect active profile
                    g_cfg.f_name = name; g_cfg.sys_prompt = sys; g_cfg.temp = temp; g_cfg.max_tokens = tokens; g_cfg.Save();
                    DBSet(L"active_profile", name);
                    // refresh list
                    HWND lb = GetDlgItem(hWnd, ID_PROF_LIST);
                    if (lb) {
                        SendMessage(lb, LB_RESETCONTENT, 0, 0);
                        auto all = LoadAllProfiles();
                        for (auto &pr : all) {
                            std::wstring item = pr.name + L" [" + pr.keep_alive + L"] " + std::to_wstring(pr.max_tokens) + L"tok";
                            SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
                        }
                    }
                    MessageBox(hWnd, L"Profile saved & activated", L"GOIDA", MB_OK|MB_ICONINFORMATION);
                }
            }
            else if (id == ID_PROF_DEL) {
                HWND lb = GetDlgItem(hWnd, ID_PROF_LIST);
                int sel = lb ? (int)SendMessage(lb, LB_GETCURSEL, 0, 0) : LB_ERR;
                wchar_t name[128]={}; if (sel!=LB_ERR) {
                    wchar_t buf[256]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf);
                    // name is before " ["
                    wchar_t* br = wcsstr(buf, L" ["); if (br) *br=0; wcscpy(name, buf);
                } else {
                    GetDlgItemText(hWnd, ID_PROF_NAME, name, 128);
                }
                if (wcslen(name)==0) { MessageBox(hWnd, L"No profile selected", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                wchar_t msg[256]; swprintf(msg, L"Delete profile \"%s\"?", name);
                if (MessageBox(hWnd, msg, L"GOIDA", MB_YESNO|MB_ICONWARNING)!=IDYES) break;
                if (DeleteProfile(name)) {
                    Lg(L"PROFILE", (std::wstring(L"Deleted: ")+name).c_str());
                    if (lb) {
                        SendMessage(lb, LB_RESETCONTENT, 0, 0);
                        auto all = LoadAllProfiles();
                        for (auto &pr : all) {
                            std::wstring item = pr.name + L" [" + pr.keep_alive + L"] " + std::to_wstring(pr.max_tokens) + L"tok";
                            SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
                        }
                    }
                    SetDlgItemText(hWnd, ID_PROF_NAME, L""); SetDlgItemText(hWnd, ID_PROF_SYS, L"");
                }
            }
            else if (id == ID_PROF_LOAD) {
                HWND lb = GetDlgItem(hWnd, ID_PROF_LIST);
                int sel = lb ? (int)SendMessage(lb, LB_GETCURSEL, 0, 0) : LB_ERR;
                if (sel==LB_ERR) { MessageBox(hWnd, L"Select a profile", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                wchar_t buf[256]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf);
                wchar_t* br = wcsstr(buf, L" ["); if (br) *br=0;
                // find profile
                auto all = LoadAllProfiles();
                for (auto &pr : all) if (pr.name==buf) {
                    SetDlgItemText(hWnd, ID_PROF_NAME, pr.name.c_str());
                    SetDlgItemText(hWnd, ID_PROF_SYS, pr.system_prompt.c_str());
                    SetDlgItemText(hWnd, ID_PROF_AVATAR, pr.avatar.c_str());
                    SetDlgItemText(hWnd, ID_PROF_TOKENS, std::to_wstring(pr.max_tokens).c_str());
                    SendMessage(GetDlgItem(hWnd, ID_PROF_TEMP), TBM_SETPOS, 1, (int)(pr.temp*100));
                    wchar_t tb[32]; swprintf(tb, L"%.2f", pr.temp); SetDlgItemText(hWnd, ID_PROF_TEMPV, tb);
                    int lvl = LevelFromKeepAlive(pr.keep_alive);
                    int csel = (lvl==1?0:lvl==2?1:lvl==3?2:3);
                    SendMessage(GetDlgItem(hWnd, ID_PROF_KEEP), CB_SETCURSEL, csel, 0);
                    // activate
                    g_cfg.f_name = pr.name; g_cfg.sys_prompt = pr.system_prompt; g_cfg.temp = pr.temp; g_cfg.max_tokens = pr.max_tokens; g_cfg.Save();
                    DBSet(L"active_profile", pr.name.c_str());
                    // also update chat system edit if chat page not active, it will reload next time
                    Lg(L"PROFILE", (std::wstring(L"Loaded: ")+pr.name).c_str());
                    MessageBox(hWnd, (std::wstring(L"Profile ")+pr.name+L" loaded").c_str(), L"GOIDA", MB_OK|MB_ICONINFORMATION);
                    break;
                }
            }
            else if (id == ID_MEM_ADD) {
                if (MemoryCount() >= 100) { MessageBox(hWnd, L"Memory limit 100 reached. Delete some entries.", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                wchar_t tag[128], val[256]; GetDlgItemText(hWnd, ID_MEM_TAG, tag, 128); GetDlgItemText(hWnd, ID_MEM_VAL, val, 256);
                if (wcslen(tag) > 0 && wcslen(val) > 0) {
                    goida::raii::Stmt s(DBPrep("INSERT INTO memory(tag,value) VALUES(?1,?2)"));
                    if (s) {
                        std::string t = goida::utf8::w2utf8(tag);
                        std::string v = goida::utf8::w2utf8(val);
                        sqlite3_bind_text(s.get(), 1, t.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(s.get(), 2, v.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(s.get());
                    }
                    wchar_t buf[512]; swprintf(buf, L"%s|%s", tag, val);
                    SendMessage(GetDlgItem(hWnd, ID_MEM_LIST), LB_ADDSTRING, 0, (LPARAM)buf);
                    SetDlgItemText(hWnd, ID_MEM_TAG, L""); SetDlgItemText(hWnd, ID_MEM_VAL, L"");
                    // update count label
                    wchar_t cc[64]; swprintf(cc, L"%d/100 used", MemoryCount());
                    SetDlgItemText(hWnd, ID_MEM_COUNT, cc);
                } }
            else if (id == ID_MEM_DEL) {
                HWND lb = GetDlgItem(hWnd, ID_MEM_LIST); int sel = (int)SendMessage(lb, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    wchar_t buf[512]; SendMessage(lb, LB_GETTEXT, sel, (LPARAM)buf);
                    wchar_t* p = wcschr(buf, L'|'); if (p) { *p = 0;
                        sqlite3_stmt* s = DBPrep("DELETE FROM memory WHERE tag=?1 AND value=?2");
                        if (s) { char t[128], v[256]; WideCharToMultiByte(CP_UTF8, 0, buf, -1, t, sizeof(t), NULL, NULL);
                            WideCharToMultiByte(CP_UTF8, 0, p+1, -1, v, sizeof(v), NULL, NULL);
                            sqlite3_bind_text(s, 1, t, -1, SQLITE_TRANSIENT);
                            sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
                            sqlite3_step(s); sqlite3_finalize(s); } }
                    SendMessage(lb, LB_DELETESTRING, sel, 0); } }
            else if (id == ID_CONS_CLR) { g_logCache.clear(); DBExec("DELETE FROM logs"); SetDlgItemText(hWnd, ID_CONS_LOG, L""); }
            else if (id == ID_CONS_RELOAD) { g_cfg.Load(); Lg(L"CONFIG", L"Reloaded"); ShowPage(hWnd, 4); }
            else if (id == ID_CONS_UPDATE) {
                std::wstring m = L"Update?\n" + g_cfg.update_url; if (MessageBox(hWnd, m.c_str(), L"GOIDA", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
                Lg(L"UPDATE", L"Downloading..."); wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); std::wstring t = std::wstring(tmp) + L"goida_new.exe";
                HRESULT hr = URLDownloadToFileW(NULL, g_cfg.update_url.c_str(), t.c_str(), 0, NULL);
                if (FAILED(hr)) { MessageBox(hWnd, L"Download failed!", L"GOIDA", MB_OK | MB_ICONERROR); Lg(L"UPDATE", L"Failed"); }
                else { wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH); wchar_t bt[MAX_PATH]; GetTempPathW(MAX_PATH, bt); wcscat(bt, L"goida_up.bat");
                    wchar_t bc[4096]; swprintf(bc, L"@timeout /t 2 /nobreak >nul\ncopy /Y \"%s\" \"%s\" >nul\ndel \"%s\"\nstart \"\" \"%s\"\ndel \"%~f0\"\n", t.c_str(), exe, t.c_str(), exe);
                    std::wofstream bf(bt); if (bf.is_open()) { bf << bc; bf.close(); } ShellExecuteW(NULL, L"open", bt, NULL, NULL, SW_HIDE); PostMessage(hWnd, WM_CLOSE, 0, 0); } }
            else if (id == ID_CONS_AUTO) {
                HKEY hk; wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
                if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
                    wchar_t cur[MAX_PATH]; DWORD sz = sizeof(cur); bool on = false;
                    if (RegQueryValueExW(hk, L"GOIDA_Launcher", NULL, NULL, (LPBYTE)cur, &sz) == ERROR_SUCCESS && wcscmp(cur, exe) == 0) on = true;
                    if (on) RegDeleteValueW(hk, L"GOIDA_Launcher"); else RegSetValueExW(hk, L"GOIDA_Launcher", 0, REG_SZ, (BYTE*)exe, (DWORD)((wcslen(exe)+1)*sizeof(wchar_t)));
                    RegCloseKey(hk); Lg(L"AUTO", on ? L"Off" : L"On"); ShowPage(hWnd, 4); } }
            else if (id == ID_CHAT_PULL) {
                AppendChat(L"--- Pulling model...\n"); EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), FALSE);
                DWORD tid; CreateThread(NULL, 0, PullModelThr, hWnd, 0, &tid); }
            else if (id == ID_CHAT_DEL) {
                wchar_t model[256]; GetDlgItemText(hWnd, ID_CHAT_MODEL, model, 256);
                if (model[0]) { wchar_t msg[512]; swprintf(msg, L"Delete model \"%s\"?", model);
                    if (MessageBox(hWnd, msg, L"GOIDA", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        AppendChat(L"--- Deleting model...\n");
                        DWORD tid; CreateThread(NULL, 0, DeleteModelThr, hWnd, 0, &tid); } } }
            else if (id == ID_CHAT_INFO) {
                AppendChat(L"--- Fetching model info...\n");
                DWORD tid; CreateThread(NULL, 0, ShowModelThr, hWnd, 0, &tid); }
            else if (id == ID_CHAT_JSON) {
                g_cfg.json_mode = !g_cfg.json_mode; g_cfg.Save();
                HWND jb = GetDlgItem(hWnd, ID_CHAT_JSON);
                if (jb) SetWindowText(jb, g_cfg.json_mode ? L"JSON ON" : L"JSON OFF");
                AppendChat(g_cfg.json_mode ? L"--- JSON mode ON ---\n" : L"--- JSON mode OFF ---\n");
                Lg(L"CONFIG", g_cfg.json_mode ? L"JSON ON" : L"JSON OFF"); }
            else if (id == ID_CHAT_COPYLAST) {
                for (int i = (int)g_history.size() - 1; i >= 0; i--) {
                    if (g_history[i].first == L"assistant") {
                        int n = (int)g_history[i].second.size();
                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t));
                        if (hg) { wchar_t* p = (wchar_t*)GlobalLock(hg); if (p) { wcscpy(p, g_history[i].second.c_str()); GlobalUnlock(hg);
                            OpenClipboard(hWnd); EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, hg); CloseClipboard(); } else GlobalFree(hg); }
                        SetDlgItemText(hWnd, ID_CHAT_STAT, L"Copied!"); break; } } }
            else if (id == ID_CHAT_REG) {
                // Remove last assistant response, resend last user message
                if (g_history.size() >= 2 && g_history.back().first == L"assistant")
                    g_history.pop_back();
                if (!g_history.empty() && g_history.back().first == L"user") {
                    std::wstring lastUser = g_history.back().second;
                    // Trim the last "<< AI: " line from history view
                    HWND hv = GetDlgItem(hWnd, ID_CHAT_HIST);
                    if (hv) { int len = GetWindowTextLength(hv); if (len > 0) {
                        std::wstring txt; txt.resize(len); GetWindowText(hv, &txt[0], len + 1);
                        auto pos = txt.rfind(L"\n<< AI:"); if (pos != std::wstring::npos) txt = txt.substr(0, pos + 1);
                        else if (txt.rfind(L"<< AI:") != std::wstring::npos) { pos = txt.rfind(L"<< AI:"); txt = txt.substr(0, pos); }
                        SetWindowText(hv, txt.c_str()); } }
                    AppendChat(L"<< AI: ");
                    SetDlgItemText(hWnd, ID_CHAT_STAT, L"Regenerating...");
                    g_streamBuf.clear();
                    wchar_t model[256], url[512]; GetDlgItemText(hWnd, ID_CHAT_MODEL, model, 256); GetDlgItemText(hWnd, ID_CHAT_API, url, 512);
                    EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), FALSE); EnableWindow(GetDlgItem(hWnd, ID_CHAT_INPUT), FALSE);
                    g_startTick = GetTickCount64();
                    CreateThread(NULL, 0, HttpThr, new HttpD{url, ChatJson(model, url, true), hWnd, 1}, 0, NULL);
                } }
            else if (id == ID_CHAT_BRANCH) {
                if (g_history.empty()) { MessageBox(hWnd, L"No history to branch", L"GOIDA", MB_OK|MB_ICONINFORMATION); break; }
                // Save current history as branch snapshot to DB (chat_branches)
                goida::raii::Stmt ins(DBPrep("INSERT INTO chat_branches(role, content) VALUES(?1,?2)"));
                // We'll store a marker and copy last few messages as branch
                // Save branch marker
                std::string branchTag = "branch_" + std::to_string(GetTickCount());
                // Insert all current history as branch entries (parent_id NULL for now)
                for (auto &m : g_history) {
                    if (!ins) break;
                    std::string r = goida::utf8::w2utf8(m.first);
                    std::string c = goida::utf8::w2utf8(m.second);
                    sqlite3_bind_text(ins.get(),1,r.c_str(),-1,SQLITE_TRANSIENT);
                    sqlite3_bind_text(ins.get(),2,c.c_str(),-1,SQLITE_TRANSIENT);
                    sqlite3_step(ins.get()); sqlite3_reset(ins.get());
                }
                // Append visual marker
                AppendChat(L"\n--- Branch created (history snapshot saved to DB) ---\n");
                // Push system marker to history to indicate branch point
                g_history.push_back({L"system", L"branch_point"});
                Lg(L"BRANCH", L"Snapshot saved");
                MessageBox(hWnd, L"Branch created! Continue chatting вЂ” this is a fork. Use Load to restore previous DB snapshot if needed.", L"GOIDA", MB_OK|MB_ICONINFORMATION);
            }
            // === Models page ===
            else if (id == ID_MODEL_PULL) {
                wchar_t mb[256]; GetDlgItemText(hWnd, ID_MODEL_NAME, mb, 256);
                SetDlgItemText(hWnd, ID_CHAT_MODEL, mb);
                AppendChat(L"--- Pulling model...\n"); EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), FALSE);
                DWORD tid; CreateThread(NULL, 0, PullModelThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_DEL) {
                wchar_t model[256]; GetDlgItemText(hWnd, ID_MODEL_NAME, model, 256);
                if (model[0]) { SetDlgItemText(hWnd, ID_CHAT_MODEL, model);
                    wchar_t msg[512]; swprintf(msg, L"Delete model \"%s\"?", model);
                    if (MessageBox(hWnd, msg, L"GOIDA", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        AppendChat(L"--- Deleting model...\n");
                        DWORD tid; CreateThread(NULL, 0, DeleteModelThr, hWnd, 0, &tid); } } }
            else if (id == ID_MODEL_INFO) {
                wchar_t mb[256]; GetDlgItemText(hWnd, ID_MODEL_NAME, mb, 256);
                SetDlgItemText(hWnd, ID_CHAT_MODEL, mb);
                AppendChat(L"--- Fetching model info...\n");
                DWORD tid; CreateThread(NULL, 0, ShowModelThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_COPY) {
                AppendChat(L"--- Copying model...\n");
                DWORD tid; CreateThread(NULL, 0, CopyModelThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_CREATE) {
                AppendChat(L"--- Creating model...\n");
                DWORD tid; CreateThread(NULL, 0, CreateModelThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_RUNNING) {
                AppendChat(L"--- Fetching running models...\n");
                DWORD tid; CreateThread(NULL, 0, RunningThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_PUSH) {
                wchar_t mb[256]; GetDlgItemText(hWnd, ID_MODEL_NAME, mb, 256);
                SetDlgItemText(hWnd, ID_CHAT_MODEL, mb);
                AppendChat(L"--- Pushing model...\n");
                DWORD tid; CreateThread(NULL, 0, PushModelThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_REFRESH) {
                SetDlgItemText(hWnd, ID_CHAT_MODEL, L"");
                SetDlgItemText(hWnd, ID_MODEL_NAME, L"");
                HWND lv = GetDlgItem(hWnd, ID_MODEL_LIST);
                if (lv) {
                    // clear ListView vs ListBox
                    char cls[64]={0}; GetClassNameA(lv, cls, 64);
                    if (strcmp(cls, WC_LISTVIEWA)==0 || strcmp(cls, "SysListView32")==0) ListView_DeleteAllItems(lv);
                    else SendMessage(lv, LB_RESETCONTENT, 0, 0);
                }
                // reset progress
                HWND prog = GetDlgItem(hWnd, ID_MODEL_PROGRESS);
                if (prog) SendMessage(prog, PBM_SETPOS, 0, 0);
                DWORD tid; CreateThread(NULL, 0, ModelListThr, hWnd, 0, &tid); }
            else if (id == ID_MODEL_MKT_INSTALL) {
                HWND mkt = GetDlgItem(hWnd, ID_MODEL_MKT_LIST);
                int sel = mkt ? ListView_GetNextItem(mkt, -1, LVNI_SELECTED) : -1;
                if (sel==-1) { MessageBox(hWnd, L"Select a marketplace model", L"GOIDA", MB_OK|MB_ICONWARNING); break; }
                wchar_t name[128]; ListView_GetItemText(mkt, sel, 0, name, 128);
                SetDlgItemText(hWnd, ID_MODEL_NAME, name);
                SetDlgItemText(hWnd, ID_CHAT_MODEL, name);
                AppendChat(L"--- Marketplace installing: "); AppendChat(name); AppendChat(L" ---\n");
                // reset progress bar
                HWND prog = GetDlgItem(hWnd, ID_MODEL_PROGRESS);
                if (prog) SendMessage(prog, PBM_SETPOS, 0, 0);
                DWORD tid; CreateThread(NULL, 0, PullModelThr, hWnd, 0, &tid);
            }
            else if (id == ID_MODEL_EMBED) {
                AppendChat(L"--- Embed request ---\n");
                DWORD tid; CreateThread(NULL, 0, EmbedThr, hWnd, 0, &tid);
            }
            else if (id == ID_MODEL_TOOLS) {
                // Toggle tools + demo images handling
                if (GetKeyState(VK_SHIFT) & 0x8000) {
                    // Shift+click => pick image file
                    wchar_t path[MAX_PATH]={0};
                    OPENFILENAMEW ofn={sizeof(ofn), hWnd, NULL, L"Images\0*.png;*.jpg;*.jpeg;*.webp\0All\0*.*\0", NULL,0,0,path,MAX_PATH,NULL,0,NULL,L"Select image", OFN_FILEMUSTEXIST|OFN_HIDEREADONLY};
                    if (GetOpenFileNameW(&ofn)) {
                        std::string b64 = FileToBase64(path);
                        if (!b64.empty()) {
                            g_pendingImages.clear(); g_pendingImages.push_back(b64);
                            std::wstring msg = L"[IMAGE] added: "; msg += path; msg += L" (" + std::to_wstring(b64.size()) + L" b64)\n";
                            AppendChat(msg.c_str());
                            SetDlgItemText(hWnd, ID_CHAT_STAT, L"Image queued");
                        } else AppendChat(L"[IMAGE] failed to load\n");
                    }
                } else {
                    g_toolsEnabled = !g_toolsEnabled;
                    if (g_toolsEnabled) {
                        // sample tool: calculator + search
                        g_toolsJson = L"[{\"type\":\"function\",\"function\":{\"name\":\"calculator\",\"description\":\"Evaluate math expression\",\"parameters\":{\"type\":\"object\",\"properties\":{\"expression\":{\"type\":\"string\"}},\"required\":[\"expression\"]}}},{\"type\":\"function\",\"function\":{\"name\":\"web_search\",\"description\":\"Search web\",\"parameters\":{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"}},\"required\":[\"query\"]}}}]";
                        AppendChat(L"--- Tools ENABLED (calculator, web_search) ---\n");
                        SetDlgItemText(hWnd, ID_MODEL_TOOLS, L"Tools ON");
                    } else {
                        g_toolsJson.clear();
                        AppendChat(L"--- Tools DISABLED ---\n");
                        SetDlgItemText(hWnd, ID_MODEL_TOOLS, L"Tools demo");
                    }
                }
            }
            else if (id == ID_SET_THEME && HIWORD(wParam)==CBN_SELCHANGE) {
                int sel=(int)SendMessage(GetDlgItem(hWnd, ID_SET_THEME), CB_GETCURSEL,0,0);
                wchar_t buf[32]={0}; SendMessage(GetDlgItem(hWnd, ID_SET_THEME), CB_GETLBTEXT, sel, (LPARAM)buf);
                g_cfg.theme=buf; g_cfg.Save(); UpdateAccent(); Lg(L"THEME", buf); InvalidateRect(hWnd,NULL,TRUE);
                AppendChat((std::wstring(L"[THEME] ")+buf+L"\n").c_str());
            }
            else if (id == ID_SET_ACCENT && HIWORD(wParam)==CBN_SELCHANGE) {
                int sel=(int)SendMessage(GetDlgItem(hWnd, ID_SET_ACCENT), CB_GETCURSEL,0,0);
                wchar_t buf[32]={0}; SendMessage(GetDlgItem(hWnd, ID_SET_ACCENT), CB_GETLBTEXT, sel, (LPARAM)buf);
                g_cfg.accent=buf; g_cfg.Save(); UpdateAccent(); Lg(L"ACCENT", buf); InvalidateRect(hWnd,NULL,TRUE);
                AppendChat((std::wstring(L"[ACCENT] ")+buf+L"\n").c_str());
            }
            else if (id == ID_SET_LANG && HIWORD(wParam)==CBN_SELCHANGE) {
                int sel=(int)SendMessage(GetDlgItem(hWnd, ID_SET_LANG), CB_GETCURSEL,0,0);
                wchar_t buf[32]={0}; SendMessage(GetDlgItem(hWnd, ID_SET_LANG), CB_GETLBTEXT, sel, (LPARAM)buf);
                g_cfg.lang=buf; g_cfg.Save();
                // reload i18n
                goida::i18n::instance().load((std::wstring(L"lang/")+buf+L".json").c_str(), goida::utf8::w2utf8(buf));
                Lg(L"LANG", buf); MessageBox(hWnd, L"Language switched вЂ” restart to apply fully", L"GOIDA", MB_OK|MB_ICONINFORMATION);
            }
            else if (id == ID_SET_ROUND && HIWORD(wParam)==BN_CLICKED) {
                g_cfg.rounded = (SendMessage(GetDlgItem(hWnd, ID_SET_ROUND), BM_GETCHECK,0,0)==BST_CHECKED);
                g_cfg.Save(); InvalidateRect(hWnd,NULL,TRUE);
                Lg(L"ROUND", g_cfg.rounded?L"on":L"off");
            }
            else if (id == ID_SET_ADAPTIVE && HIWORD(wParam)==BN_CLICKED) {
                g_cfg.adaptive = (SendMessage(GetDlgItem(hWnd, ID_SET_ADAPTIVE), BM_GETCHECK,0,0)==BST_CHECKED);
                g_cfg.Save(); Lg(L"ADAPTIVE", g_cfg.adaptive?L"on":L"off");
            }
            break;
        }
        case WM_HSCROLL: {
            HWND s = (HWND)lParam;
            if (s == GetDlgItem(hWnd, ID_TRAIN_TEMP)) {
                int pos = (int)SendMessage(s, TBM_GETPOS, 0, 0);
                wchar_t buf[32]; swprintf(buf, L"Temp: %.2f", pos / 100.0f);
                SetDlgItemText(hWnd, ID_TRAIN_TEMPV, buf);
            }
            else if (s == GetDlgItem(hWnd, ID_CHAT_TEMP)) {
                int pos = (int)SendMessage(s, TBM_GETPOS, 0, 0);
                wchar_t buf[32]; swprintf(buf, L"%.2f", pos / 100.0f);
                SetDlgItemText(hWnd, ID_CHAT_TEMPV, buf);
            }
            else if (s == GetDlgItem(hWnd, ID_PROF_TEMP)) {
                int pos = (int)SendMessage(s, TBM_GETPOS, 0, 0);
                wchar_t buf[32]; swprintf(buf, L"%.2f", pos / 100.0f);
                SetDlgItemText(hWnd, ID_PROF_TEMPV, buf);
            }
            break;
        }
        case WM_HTTP_CHUNK: { std::wstring* t = (std::wstring*)lParam; if (t) { AppendChat(t->c_str()); if (wParam == 0) g_streamBuf += *t; delete t; } break; }
        case WM_HTTP_DONE: {
            std::wstring* r = (std::wstring*)lParam;
            if (wParam == 1) {
                if (r) { AppendChat(r->c_str()); AppendChat(L"\n"); delete r; }
                HWND snd = GetDlgItem(hWnd, ID_CHAT_SEND); if (snd) EnableWindow(snd, TRUE);
                HWND st = GetDlgItem(hWnd, ID_CHAT_STAT); if (st) SetWindowText(st, L"Done");
                st = GetDlgItem(hWnd, ID_MODEL_STAT); if (st) SetWindowText(st, L"Done");
            } else {
                if (r && !g_cancelStream) {
                    std::wstring text = JStr(*r, L"response"); if (text.empty()) text = JStr(*r, L"content");
                    if (text.empty()) { auto cp = r->find(L"\"content\":\""); if (cp != std::wstring::npos) { cp += 11; while (cp < r->size() && !((*r)[cp] == L'"')) text += (*r)[cp++]; } }
                    if (text.empty()) text = L"(empty)"; AppendChat(text.c_str()); AppendChat(L"\n"); g_streamBuf += text;
                    g_history.push_back({L"assistant", g_streamBuf}); g_streamBuf.clear(); delete r;
                } else { if (r) delete r; if (!g_streamBuf.empty() && !g_cancelStream) { g_history.push_back({L"assistant", g_streamBuf}); g_streamBuf.clear(); } AppendChat(L"\n"); }
                g_cancelStream = false;
                EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), TRUE); EnableWindow(GetDlgItem(hWnd, ID_CHAT_INPUT), TRUE);
                EnableWindow(GetDlgItem(hWnd, ID_TRAIN_GEN), TRUE);
                g_connStatus = CONN_OK;
                // Response stats
                if (g_startTick) {
                    __int64 elapsed = GetTickCount64() - g_startTick; g_startTick = 0;
                    int tokCount = (int)(g_streamBuf.size() / 4);
                    wchar_t sb[128]; swprintf(sb, L"OK %d tok  %.1fs", tokCount, elapsed / 1000.0);
                    HWND st = GetDlgItem(hWnd, ID_CHAT_STAT);
                    if (st) SetWindowText(st, sb);
                }
            }
            InvalidateRect(g_hWnd, NULL, FALSE);
            break;
        }
        case WM_MODEL_LIST: {
            if (wParam == 1) {
                // done marker вЂ” ensure ListView populated
                if (g_curPage == 5) {
                    HWND lv = GetDlgItem(hWnd, ID_MODEL_LIST);
                    if (lv) {
                        char cls[64]={0}; GetClassNameA(lv, cls, 64);
                        if (strcmp(cls, WC_LISTVIEWA)==0 || strcmp(cls, "SysListView32")==0) {
                            // ensure first item visible
                            ListView_EnsureVisible(lv, 0, FALSE);
                        } else {
                            int c = (int)SendMessage(lv, LB_GETCOUNT, 0, 0);
                            if (c > 0) SendMessage(lv, LB_SETTOPINDEX, 0, 0);
                        }
                    }
                }
                break;
            }
            std::wstring* m = (std::wstring*)lParam;
            if (m) {
                // auto-fill model fields if empty
                HWND ed = GetDlgItem(hWnd, ID_CHAT_MODEL);
                if (ed) { wchar_t cur[256]; GetWindowText(ed, cur, 256); if (wcslen(cur)==0) SetWindowText(ed, m->c_str()); }
                HWND ed2 = GetDlgItem(hWnd, ID_MODEL_NAME);
                if (ed2) { wchar_t cur[256]; GetWindowText(ed2, cur, 256); if (wcslen(cur)==0) {
                    // strip size suffix for ed2
                    std::wstring name = *m;
                    auto p = name.find(L" ("); if (p!=std::wstring::npos) name = name.substr(0,p);
                    SetWindowText(ed2, name.c_str());
                }}
                if (g_curPage == 5) {
                    HWND lv = GetDlgItem(hWnd, ID_MODEL_LIST);
                    if (lv) {
                        char cls[64]={0}; GetClassNameA(lv, cls, 64);
                        if (strcmp(cls, WC_LISTVIEWA)==0 || strcmp(cls, "SysListView32")==0) {
                            std::wstring name = *m;
                            std::wstring sz = L"-";
                            auto p = name.find(L" (");
                            if (p!=std::wstring::npos) { sz = name.substr(p+2); if (!sz.empty() && sz.back()==L')') sz.pop_back(); name = name.substr(0,p); }
                            int idx = ListView_GetItemCount(lv);
                            LVITEM it={0}; it.mask=LVIF_TEXT; it.iItem=idx; it.pszText=(LPWSTR)name.c_str();
                            ListView_InsertItem(lv, &it);
                            ListView_SetItemText(lv, idx, 1, (LPWSTR)sz.c_str());
                            // modified placeholder
                            ListView_SetItemText(lv, idx, 2, (LPWSTR)L"local");
                            ListView_SetItemText(lv, idx, 3, (LPWSTR)L"ready");
                        } else {
                            SendMessage(lv, LB_ADDSTRING, 0, (LPARAM)m->c_str());
                        }
                    }
                }
                delete m;
            }
            break;
        }
        case WM_MODEL_PROGRESS: {
            int pct = (int)wParam;
            HWND prog = GetDlgItem(hWnd, ID_MODEL_PROGRESS);
            if (prog) SendMessage(prog, PBM_SETPOS, pct, 0);
            wchar_t txt[64]; swprintf(txt, L"%d%%", pct);
            HWND st = GetDlgItem(hWnd, ID_MODEL_STAT);
            if (st) SetWindowText(st, txt);
            // also update title
            if (pct>=100) { if (st) SetWindowText(st, L"Done"); }
            break;
        }
        case WM_MODEL_EMBED_DONE: {
            std::wstring* r = (std::wstring*)lParam;
            if (r) { AppendChat(L"[EMBED] "); AppendChat(r->c_str()); AppendChat(L"\n"); delete r; }
            SetDlgItemText(hWnd, ID_MODEL_STAT, L"Embed done");
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR hdr = (LPNMHDR)lParam;
            if (hdr && hdr->idFrom==ID_MODEL_LIST && hdr->code==LVN_ITEMCHANGED) {
                LPNMLISTVIEW pnm = (LPNMLISTVIEW)lParam;
                if (pnm->uNewState & LVIS_SELECTED) {
                    wchar_t buf[256]; ListView_GetItemText(hdr->hwndFrom, pnm->iItem, 0, buf, 256);
                    SetDlgItemText(hWnd, ID_MODEL_NAME, buf);
                    SetDlgItemText(hWnd, ID_CHAT_MODEL, buf);
                }
            } else if (hdr && hdr->idFrom==ID_MODEL_MKT_LIST && hdr->code==NM_DBLCLK) {
                // double-click marketplace installs
                int sel = ListView_GetNextItem(hdr->hwndFrom, -1, LVNI_SELECTED);
                if (sel!=-1) {
                    wchar_t name[128]; ListView_GetItemText(hdr->hwndFrom, sel, 0, name, 128);
                    SetDlgItemText(hWnd, ID_MODEL_NAME, name);
                    SetDlgItemText(hWnd, ID_CHAT_MODEL, name);
                }
            }
            break;
        }
        case WM_CONN_RESULT: {
            g_connStatus = (ConnStatus)wParam;
            SetDlgItemText(hWnd, ID_CHAT_STAT, (g_connStatus == CONN_OK) ? L"Connected" : L"Disconnected");
            InvalidateRect(g_hWnd, NULL, FALSE);
            if (g_connStatus == CONN_OK) {
                DWORD tid; CreateThread(NULL, 0, ModelListThr, g_hWnd, 0, &tid);
            }
            break;
        }
        case WM_HTTP_ERR: { std::wstring* e = (std::wstring*)lParam; if (e) { AppendChat(L"\n[ERR] "); AppendChat(e->c_str()); AppendChat(L"\n"); delete e; }
            g_connStatus = CONN_FAIL;
            SetDlgItemText(hWnd, ID_CHAT_STAT, L"Disconnected");
            EnableWindow(GetDlgItem(hWnd, ID_CHAT_SEND), TRUE); EnableWindow(GetDlgItem(hWnd, ID_CHAT_INPUT), TRUE); EnableWindow(GetDlgItem(hWnd, ID_TRAIN_GEN), TRUE); InvalidateRect(g_hWnd, NULL, FALSE); break;
        }
        case WM_CTLCOLORSTATIC: {
            HWND ctrl = (HWND)lParam;
            if (ctrl == GetDlgItem(hWnd, ID_CHAT_STAT)) {
                COLORREF c = (g_connStatus == CONN_OK) ? RGB(60, 200, 80) : (g_connStatus == CONN_FAIL) ? RGB(220, 60, 60) : COL_TEXT;
                SetTextColor((HDC)wParam, c);
            } else if (GetProp(ctrl, L"GOIDA_SEC")) {
                SetTextColor((HDC)wParam, RGB(0, 155, 255));
            } else SetTextColor((HDC)wParam, COL_TEXT);
            SetBkColor((HDC)wParam, COL_PANEL);
            return (LRESULT)g_brPanel;
        }
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: { SetBkColor((HDC)wParam, COL_EDIT_BG); SetTextColor((HDC)wParam, COL_TEXT); return (LRESULT)g_brEdit; }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
            RECT r; GetClientRect(hWnd, &r);
            FillRect(hdc, &r, g_brBg);
            // Nav panel
            RECT nr = {0, 0, 198, r.bottom}; FillRect(hdc, &nr, g_brNav);
            // Separator line
            HPEN pen = CreatePen(PS_SOLID, 1, COL_BORDER);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, 198, 0, NULL); LineTo(hdc, 198, r.bottom);
            SelectObject(hdc, oldPen); DeleteObject(pen);
            // Title in nav
            SetBkMode(hdc, TRANSPARENT);
            HFONT old = (HFONT)SelectObject(hdc, g_fontTitle);
            SetTextColor(hdc, RGB(220, 220, 225));
            RECT tr = {0, 16, 198, 48}; DrawTextW(hdc, L"GOIDA", -1, &tr, DT_CENTER);
            SelectObject(hdc, g_fontSmall);
            SetTextColor(hdc, COL_TEXT_DIM);
            DrawTextW(hdc, L"AI MANAGER", -1, &tr, DT_CENTER | DT_BOTTOM);
            SelectObject(hdc, old);
            // Separator under title
            pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 65));
            oldPen = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, 20, 52, NULL); LineTo(hdc, 178, 52);
            SelectObject(hdc, oldPen); DeleteObject(pen);
            // Status at bottom of nav
            SelectObject(hdc, g_fontSmall);
            COLORREF dot = (g_connStatus == CONN_OK) ? RGB(60, 200, 80) : (g_connStatus == CONN_FAIL) ? RGB(220, 60, 60) : RGB(100, 100, 105);
            HBRUSH dotBr = CreateSolidBrush(dot); RECT dr = {14, r.bottom - 22, 22, r.bottom - 14};
            FillRect(hdc, &dr, dotBr); DeleteObject(dotBr);
            SetTextColor(hdc, COL_TEXT_DIM);
            wchar_t sb[128]; swprintf(sb, L" %s", (g_connStatus == CONN_OK) ? L"Connected" : (g_connStatus == CONN_FAIL) ? L"Offline" : L"Unknown");
            RECT sr = {24, r.bottom - 24, 196, r.bottom - 4}; DrawTextW(hdc, sb, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(hdc, RGB(90, 90, 95));
            RECT vr = {0, r.bottom - 44, 198, r.bottom - 28};
            DrawTextW(hdc, g_cfg.model.c_str(), -1, &vr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            EndPaint(hWnd, &ps); break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK) { ShowWindow(hWnd, SW_SHOW); ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd); }
            else if (lParam == WM_RBUTTONDOWN) {
                HMENU m = CreatePopupMenu();
                AppendMenu(m, MF_STRING, ID_TRAY_SHOW, L"Show / Hide");
                AppendMenu(m, MF_SEPARATOR, 0, NULL);
                AppendMenu(m, MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hWnd); POINT pt; GetCursorPos(&pt);
                int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(m);
                if (cmd == ID_TRAY_EXIT) PostMessage(hWnd, WM_CLOSE, 0, 0);
                else if (cmd == ID_TRAY_SHOW) ShowWindow(hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
            }
            return 0;
        }
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) { ShowWindow(hWnd, SW_HIDE); return 0; }
            if (g_cfg.adaptive && g_curPage>=0) {
                RECT cr; GetClientRect(hWnd, &cr);
                int newRight = cr.right;
                // iterate content controls (id > 2000)
                HWND child=NULL;
                while ((child=FindWindowEx(hWnd, child, NULL, NULL))!=NULL) {
                    int id = GetDlgCtrlID(child);
                    if (id>=ID_CHAT_HIST && id<=ID_CONS_AUTO) {
                        RECT rc; GetWindowRect(child, &rc);
                        MapWindowPoints(NULL, hWnd, (LPPOINT)&rc, 2);
                        int x = rc.left;
                        // only reposition content area (>=216)
                        if (x >= 200 && newRight - x > 100) {
                            int w = newRight - x - 16;
                            if (w<100) w=100;
                            // special handling for history/inp width
                            if (id==ID_CHAT_HIST || id==ID_CHAT_INPUT || id==ID_TRAIN_OUT || id==ID_TRAIN_PROMPT || id==ID_MEM_LIST || id==ID_CONS_LOG || id==ID_MODEL_LIST || id==ID_MODEL_MKT_LIST) {
                                MoveWindow(child, x, rc.top, w, rc.bottom-rc.top, TRUE);
                            } else if (id==ID_PROF_LIST) {
                                MoveWindow(child, x, rc.top, w, rc.bottom-rc.top, TRUE);
                            } else {
                                // keep height, adjust x if needed?
                                // For buttons/status, keep position relative to right edge for some
                                if (id==ID_CHAT_SEND || id==ID_CHAT_STOP || id==ID_CHAT_STAT) {
                                    // keep distance from right
                                    // not move, just invalidate
                                } else {
                                    // adjust width for edits
                                    if (w>50) MoveWindow(child, x, rc.top, w, rc.bottom-rc.top, TRUE);
                                }
                            }
                        }
                    }
                }
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == IDT_CONN_CHECK && g_curPage == 0) {
                DWORD tid; CreateThread(NULL, 0, ConnCheckThr, g_hWnd, 0, &tid);
            }
            return 0;
        }
        case WM_GETMINMAXINFO: { MINMAXINFO* mmi = (MINMAXINFO*)lParam; mmi->ptMinTrackSize.x = 720; mmi->ptMinTrackSize.y = 480; return 0; }
        case WM_DESTROY: {
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            // Auto-save chat
            if (!g_history.empty()) {
                sqlite3_stmt* s = DBPrep("INSERT INTO chat_messages(role,content) VALUES(?1,?2)");
                if (s) {
                    for (auto& m : g_history) {
                        char r[64], c[65536];
                        WideCharToMultiByte(CP_UTF8, 0, m.first.c_str(), -1, r, sizeof(r), NULL, NULL);
                        WideCharToMultiByte(CP_UTF8, 0, m.second.c_str(), -1, c, sizeof(c), NULL, NULL);
                        sqlite3_bind_text(s, 1, r, -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(s, 2, c, -1, SQLITE_TRANSIENT);
                        sqlite3_step(s); sqlite3_reset(s);
                    } sqlite3_finalize(s);
                }
            }
            DBLog(L"SYSTEM", L"Shutdown");
            sqlite3_close(g_db);
            PostQuitMessage(0);
            break;
        }
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst;
    INITCOMMONCONTROLSEX icc={sizeof(icc), ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS|ICC_BAR_CLASSES|ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    DBInit(); EnsureDefaultProfile(); g_cfg.Load(); g_cfg.Save();
    // i18n init вЂ” load lang/ru.json or lang/en.json based on config (fallback to "ru")
    {
        std::wstring lc = g_cfg.lang.empty()? L"ru" : g_cfg.lang;
        std::wstring langFile = L"lang/" + lc + L".json";
        goida::i18n::instance().load(langFile.c_str(), goida::utf8::w2utf8(lc));
        if (goida::i18n::instance().size()==0) {
            goida::i18n::instance().load(L"lang/en.json", "en");
            if (goida::i18n::instance().size()==0) {
                // try absolute path fallback
                goida::i18n::instance().load(L"D:/GOIDA-AI-MANAGER/lang/ru.json", "ru");
            }
        }
    }

    g_fontTitle = CreateFontW(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontUI = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    g_brBg = CreateSolidBrush(COL_BG); g_brNav = CreateSolidBrush(COL_NAV);
    g_brPanel = CreateSolidBrush(COL_PANEL); g_brEdit = CreateSolidBrush(COL_EDIT_BG);

    WNDCLASS wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = L"GOIDA"; wc.hbrBackground = NULL; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&wc);

    RECT wr = {0, 0, 840, 580}; AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(0, L"GOIDA", APPNAME, WS_OVERLAPPEDWINDOW,
        50, 50, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 0;

    g_nid.cbSize = sizeof(NOTIFYICONDATAW); g_nid.hWnd = g_hWnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); wcscpy(g_nid.szTip, APPNAME);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    ACCEL acc[] = {
        {FCONTROL, '1', ID_NAV_CHAT}, {FCONTROL, '2', ID_NAV_TRAIN}, {FCONTROL, '3', ID_NAV_FEMBOY},
        {FCONTROL, '4', ID_NAV_MEMORY}, {FCONTROL, '5', ID_NAV_CONSOLE}, {FCONTROL, '6', ID_NAV_MODELS},
        {FCONTROL, '7', ID_NAV_ABOUT},
        {FCONTROL, 'R', ID_CHAT_REG}, {FCONTROL, 'B', ID_CHAT_BRANCH}, {FCONTROL, 'L', ID_CHAT_CLEAR},
        {FVIRTKEY, VK_F5, ID_MODEL_REFRESH}, {FVIRTKEY, VK_ESCAPE, ID_CHAT_STOP},
        {FCONTROL, VK_RETURN, ID_CHAT_SEND}, {FCONTROL, 'W', ID_TRAY_EXIT}
    };
    HACCEL hAccel = CreateAcceleratorTableW(acc, 14);
    // Init accent from config
    UpdateAccent();

    DBLog(L"SYSTEM", L"GOIDA v2.1 started");
    SetTimer(g_hWnd, IDT_CONN_CHECK, 30000, NULL);
    ShowWindow(g_hWnd, nShow); UpdateWindow(g_hWnd);

    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(g_hWnd, hAccel, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    DestroyAcceleratorTable(hAccel);
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    sqlite3_close(g_db);
    return 0;
}

