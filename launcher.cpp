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
#include <map>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

enum {
    // Login
    IDC_LOGIN_BTN=200, IDC_LOGIN_NAME,
    // Navigation
    IDC_NAV_BACK, IDC_NAV_TITLE, IDC_NAV_USER,
    // Templates page
    IDC_TMPL_LIST, IDC_TMPL_ADD, IDC_TMPL_DEL, IDC_TMPL_EDIT, IDC_TMPL_START, IDC_TMPL_MODELS,
    // Editor
    IDC_ED_NAME, IDC_ED_MODEL, IDC_ED_TEMP_SLIDER, IDC_ED_TEMP_VAL, IDC_ED_SYS,
    IDC_ED_IN, IDC_ED_OUT, IDC_ED_CYCLES, IDC_ED_ADD, IDC_ED_LIST, IDC_ED_SAVE, IDC_ED_BACK,
    IDC_ED_GEN, IDC_ED_PULL, IDC_ED_PULL_NAME, IDC_ED_MODEL_REFRESH, IDC_STATUS,
    // Models catalog
    IDC_MODELS_CATALOG, IDC_MODELS_INSTALL, IDC_MODELS_LIST, IDC_MODELS_BACK, IDC_MODELS_PROGRESS,
    // Femboy settings
    IDC_FEMBOY_NAME, IDC_FEMBOY_SAVE, IDC_FEMBOY_MEM_VIEW,
    // Memory
    IDC_MEM_LIST, IDC_MEM_ADD, IDC_MEM_DEL, IDC_MEM_TAG, IDC_MEM_KEY, IDC_MEM_VAL, IDC_MEM_SAVE, IDC_MEM_CLEAR, IDC_MEM_BACK, IDC_MEM_FILTER
};

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434

// Dark theme colors (Catppuccin Mocha)
#define COL_BG        0x1E1E2E  // Base
#define COL_SURFACE   0x313244  // Surface0
#define COL_SURFACE1  0x45475A  // Surface1
#define COL_OVERLAY   0x585B70  // Overlay0
#define COL_TEXT      0xCDD6F4  // Text
#define COL_SUBTEXT   0xA6ADC8  // Subtext1
#define COL_ACCENT    0x89B4FA  // Blue
#define COL_ACCENT_H  0x74C7EC  // Sapphire
#define COL_GREEN     0xA6E3A1  // Green
#define COL_RED       0xF38BA8  // Red
#define COL_YELLOW    0xF9E2AF  // Yellow
#define COL_PURPLE    0xCBA6F7  // Mauve
#define COL_ORANGE    0xFAB387  // Peach
#define COL_BLUE      0x89B4FA  // Blue

HFONT g_font, g_font_big, g_font_title, g_font_nav;
HBRUSH g_bg_brush, g_surface_brush, g_accent_brush;
int g_page = 0;
int g_prev_page = 0;
std::wstring g_username;
std::wstring g_app_dir;
std::wstring g_femboy_name = L"Фембойчик";
std::wstring g_femboy_global_name = L"Фембойчик";

struct Template {
    std::wstring name;
    std::wstring char_name;
    std::wstring model;
    double temp;
    std::wstring sys_prompt;
    std::vector<std::pair<std::string,std::string>> examples;
};
std::vector<Template> g_templates;
int g_editing = -1;

enum MemTag { TAG_LIKES=0, TAG_DISLIKES, TAG_FACTS, TAG_PREFERENCES, TAG_MEMORIES };
const wchar_t* g_tag_names[] = { L"❤ Нравится", L"💔 Не нравится", L"📝 Факты", L"⚙ Предпочтения", L"💭 Памяти" };
COLORREF g_tag_colors[] = { COL_GREEN, COL_RED, COL_BLUE, COL_PURPLE, COL_ORANGE };

struct MemoryEntry {
    int tag;
    std::wstring key;
    std::wstring value;
    time_t timestamp;
};
std::vector<MemoryEntry> g_memories;
int g_mem_selected = -1;
int g_mem_filter_tag = -1;

struct ModelInfo { const char *name; const char *desc; const char *size; };
const ModelInfo g_catalog[] = {
    {"qwen2.5-coder:3b", "Кодинг и чат (3B)", "~2GB"},
    {"llama3.2:1b", "Быстрый чат (1B)", "~1.3GB"},
    {"llama3.2:3b", "Баланс скорость/качество (3B)", "~2GB"},
    {"gemma2:2b", "Легковесный (2B)", "~1.6GB"},
    {"gemma2:9b", "Качественный (9B)", "~5.5GB"},
    {"phi3:mini", "Microsoft (3.8B)", "~2.2GB"},
    {"mistral:7b", "Mistral 7B", "~4.1GB"},
    {"neural-chat:7b", "Intel нейро-чат (7B)", "~4.1GB"},
    {"codellama:7b", "Кодинг (7B)", "~3.8GB"},
    {"deepseek-coder:6.7b", "DeepSeek кодинг (6.7B)", "~3.9GB"},
};
const int g_catalog_count = sizeof(g_catalog) / sizeof(g_catalog[0]);

// === HWNDs ===
HWND g_nav_back, g_nav_title, g_nav_user;
HWND g_login_title, g_login_sub, g_login_input, g_login_btn;
HWND g_tmpl_header, g_tmpl_list, g_tmpl_add, g_tmpl_del, g_tmpl_edit, g_tmpl_start, g_tmpl_models, g_tmpl_femboy;
HWND g_ed_lbl_name, g_ed_name, g_ed_lbl_model, g_ed_model, g_ed_model_refresh, g_ed_lbl_temp, g_ed_temp_slider, g_ed_temp_val;
HWND g_ed_lbl_sys, g_ed_sys, g_ed_lbl_in, g_ed_in, g_ed_lbl_out, g_ed_out, g_ed_lbl_cyc, g_ed_cycles, g_ed_add;
HWND g_ed_list, g_ed_save, g_ed_back, g_ed_gen, g_ed_lbl_pull, g_ed_pull_name, g_ed_pull;
HWND g_models_header, g_models_list, g_models_install, g_models_back, g_models_progress;
HWND g_femboy_name_edit, g_femboy_save_btn, g_femboy_mem_btn;
HWND g_mem_header, g_mem_list, g_mem_tag, g_mem_key, g_mem_val, g_mem_add, g_mem_del, g_mem_save, g_mem_clear, g_mem_back, g_mem_filter;
HWND g_status;

// === Utils ===
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

std::string http_get(const char *path) {
    static int init = 0;
    if (!init) { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); init = 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(OLLAMA_PORT);
    inet_pton(AF_INET, OLLAMA_HOST, &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return ""; }
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
    static int init = 0;
    if (!init) { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); init = 1; }
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(OLLAMA_PORT);
    inet_pton(AF_INET, OLLAMA_HOST, &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) < 0) { closesocket(s); return ""; }
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

// === Global data paths ===
std::wstring get_global_dir() {
    return g_app_dir + L"global\\";
}
std::wstring get_templates_dir() {
    return get_global_dir() + L"templates\\";
}
std::wstring get_memory_file() {
    return get_global_dir() + L"memory.dat";
}
std::wstring get_femboy_file() {
    return get_global_dir() + L"femboy.ini";
}

// === Templates ===
void load_templates() {
    g_templates.clear();
    std::wstring tdir = get_templates_dir();
    CreateDirectoryW(tdir.c_str(), NULL);
    wchar_t search[MAX_PATH];
    WIN32_FIND_DATAW fd;
    wsprintfW(search, L"%s*.ini", tdir.c_str());
    HANDLE hFind = FindFirstFileW(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fpath = tdir + fd.cFileName;
        std::string content = read_file(fpath);
        if (content.empty()) continue;
        Template t;
        t.name = std::wstring(fd.cFileName);
        t.name = t.name.substr(0, t.name.length() - 4);
        t.char_name = utf8w(extract_field(content, "char_name"));
        t.model = utf8w(extract_field(content, "model"));
        std::string tmp = extract_field(content, "temperature");
        t.temp = tmp.empty() ? 0.7 : atof(tmp.c_str());
        std::string sys = extract_field(content, "system_prompt");
        if (!sys.empty()) {
            std::string decoded;
            for (size_t i = 0; i < sys.length(); i++) {
                if (sys[i] == '\\' && i + 1 < sys.length()) {
                    if (sys[i+1] == 'n') { decoded += '\n'; i++; }
                    else if (sys[i+1] == 'r') { i++; }
                    else if (sys[i+1] == 't') { decoded += '\t'; i++; }
                    else if (sys[i+1] == '\\') { decoded += '\\'; i++; }
                    else decoded += sys[i];
                } else decoded += sys[i];
            }
            t.sys_prompt = utf8w(decoded);
        }
        g_templates.push_back(t);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void save_template(int idx) {
    if (idx < 0 || idx >= (int)g_templates.size()) return;
    Template &t = g_templates[idx];
    std::wstring fpath = get_templates_dir() + t.name + L".ini";
    std::string content;
    char mb[1024];
    WideCharToMultiByte(CP_UTF8, 0, t.char_name.c_str(), -1, mb, 1024, NULL, NULL);
    content += "char_name=" + std::string(mb) + "\n";
    WideCharToMultiByte(CP_UTF8, 0, t.model.c_str(), -1, mb, 1024, NULL, NULL);
    content += "model=" + std::string(mb) + "\n";
    char tb[32]; snprintf(tb, 32, "%.2f", t.temp);
    content += "temperature=" + std::string(tb) + "\n";
    std::string sys8 = w8utf(t.sys_prompt);
    std::string escaped;
    for (char c : sys8) {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else if (c == '\t') escaped += "\\t";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
    }
    content += "system_prompt=" + escaped + "\n";
    write_file(fpath, content);
}

void delete_template(int idx) {
    if (idx < 0 || idx >= (int)g_templates.size()) return;
    Template &t = g_templates[idx];
    std::wstring fpath = get_templates_dir() + t.name + L".ini";
    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, fpath.c_str(), -1, cpath, MAX_PATH, NULL, NULL);
    DeleteFileA(cpath);
    g_templates.erase(g_templates.begin() + idx);
}

// === Femboy global ===
void load_femboy() {
    std::string content = read_file(get_femboy_file());
    if (content.empty()) { g_femboy_global_name = L"Фембойчик"; return; }
    g_femboy_global_name = utf8w(extract_field(content, "name"));
    if (g_femboy_global_name.empty()) g_femboy_global_name = L"Фембойчик";
}

void save_femboy() {
    std::string content;
    char mb[1024];
    WideCharToMultiByte(CP_UTF8, 0, g_femboy_global_name.c_str(), -1, mb, 1024, NULL, NULL);
    content += "name=" + std::string(mb) + "\n";
    write_file(get_femboy_file(), content);
    g_femboy_name = g_femboy_global_name;
}

// === Memory ===
void load_memories() {
    g_memories.clear();
    std::string content = read_file(get_memory_file());
    if (content.empty()) return;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        std::string line = content.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty()) continue;
        // Format: tag|key|value|timestamp
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

void save_memories() {
    std::string content;
    for (auto &m : g_memories) {
        content += std::to_string(m.tag) + "|" + w8utf(m.key) + "|" + w8utf(m.value) + "|" + std::to_string(m.timestamp) + "\n";
    }
    write_file(get_memory_file(), content);
}

void add_memory(int tag, const std::wstring &key, const std::wstring &val) {
    MemoryEntry m;
    m.tag = tag;
    m.key = key;
    m.value = val;
    m.timestamp = time(NULL);
    g_memories.push_back(m);
    save_memories();
}

void delete_memory(int idx) {
    if (idx < 0 || idx >= (int)g_memories.size()) return;
    g_memories.erase(g_memories.begin() + idx);
    save_memories();
}

std::wstring mem_time_str(time_t t) {
    struct tm *tm = localtime(&t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"%d.%m.%Y %H:%M", tm);
    return buf;
}

// === Config for client ===
void save_config_and_launch() {
    if (g_editing < 0 || g_editing >= (int)g_templates.size()) return;
    Template &t = g_templates[g_editing];
    std::wstring cfg_path = g_app_dir + L"config.ini";
    std::string content;
    char mb[1024];
    WideCharToMultiByte(CP_UTF8, 0, t.char_name.c_str(), -1, mb, 1024, NULL, NULL);
    content += "[general]\nname=" + std::string(mb) + "\n";
    WideCharToMultiByte(CP_UTF8, 0, t.model.c_str(), -1, mb, 1024, NULL, NULL);
    content += "model=" + std::string(mb) + "\n";
    char tb[32]; snprintf(tb, 32, "%.2f", t.temp);
    content += "temperature=" + std::string(tb) + "\n";
    std::string sys8 = w8utf(t.sys_prompt);
    std::string escaped;
    for (char c : sys8) {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else if (c == '\t') escaped += "\\t";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
    }
    content += "system_prompt=" + escaped + "\n";
    write_file(cfg_path, content);
    std::wstring client_path = g_app_dir + L"client.exe";
    ShellExecuteW(NULL, L"open", client_path.c_str(), NULL, g_app_dir.c_str(), SW_SHOW);
}

// === UI Helpers ===
void hide_all() {
    HWND all[] = {
        g_nav_back, g_nav_title, g_nav_user,
        g_login_title, g_login_sub, g_login_input, g_login_btn,
        g_tmpl_header, g_tmpl_list, g_tmpl_add, g_tmpl_del, g_tmpl_edit, g_tmpl_start, g_tmpl_models, g_tmpl_femboy,
        g_ed_lbl_name, g_ed_name, g_ed_lbl_model, g_ed_model, g_ed_model_refresh, g_ed_lbl_temp, g_ed_temp_slider, g_ed_temp_val,
        g_ed_lbl_sys, g_ed_sys, g_ed_lbl_in, g_ed_in, g_ed_lbl_out, g_ed_out, g_ed_lbl_cyc, g_ed_cycles, g_ed_add,
        g_ed_list, g_ed_save, g_ed_back, g_ed_gen, g_ed_lbl_pull, g_ed_pull_name, g_ed_pull, g_ed_model_refresh,
        g_models_header, g_models_list, g_models_install, g_models_back, g_models_progress,
        g_femboy_name_edit, g_femboy_save_btn, g_femboy_mem_btn,
        g_mem_header, g_mem_list, g_mem_tag, g_mem_key, g_mem_val, g_mem_add, g_mem_del, g_mem_save, g_mem_clear, g_mem_back, g_mem_filter,
        g_status
    };
    for (HWND h : all) if (h) ShowWindow(h, SW_HIDE);
}

void update_nav(const wchar_t *title, bool show_back, int prev) {
    g_prev_page = prev;
    ShowWindow(g_nav_back, show_back ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_nav_title, title);
    wchar_t user[256];
    swprintf(user, 256, L"👤 %s | 🤖 %s", g_username.c_str(), g_femboy_name.c_str());
    SetWindowTextW(g_nav_user, user);
    ShowWindow(g_nav_title, SW_SHOW);
    ShowWindow(g_nav_user, SW_SHOW);
}

void show_login() {
    hide_all();
    ShowWindow(g_login_title, SW_SHOW);
    ShowWindow(g_login_sub, SW_SHOW);
    ShowWindow(g_login_input, SW_SHOW);
    ShowWindow(g_login_btn, SW_SHOW);
    SetFocus(g_login_input);
    g_page = 0;
}

void show_templates() {
    hide_all();
    update_nav(L"Шаблоны", false, 0);
    SendMessageW(g_tmpl_list, LB_RESETCONTENT, 0, 0);
    for (auto &t : g_templates)
        SendMessageW(g_tmpl_list, LB_ADDSTRING, 0, (LPARAM)t.name.c_str());
    if (!g_templates.empty())
        SendMessageW(g_tmpl_list, LB_SETCURSEL, 0, 0);
    ShowWindow(g_tmpl_header, SW_SHOW);
    ShowWindow(g_tmpl_list, SW_SHOW);
    ShowWindow(g_tmpl_add, SW_SHOW);
    ShowWindow(g_tmpl_del, SW_SHOW);
    ShowWindow(g_tmpl_edit, SW_SHOW);
    ShowWindow(g_tmpl_start, SW_SHOW);
    ShowWindow(g_tmpl_models, SW_SHOW);
    ShowWindow(g_tmpl_femboy, SW_SHOW);
    wchar_t st[256];
    swprintf(st, 256, L"Шаблонов: %d", (int)g_templates.size());
    SetWindowTextW(g_status, st);
    ShowWindow(g_status, SW_SHOW);
    g_page = 1;
}

void refresh_model_combo() {
    SendMessageW(g_ed_model, CB_RESETCONTENT, 0, 0);
    std::string installed_raw = http_get("/api/tags");
    std::string key = "\"name\":\"";
    size_t p = 0;
    int saved_sel = -1;
    int idx = 0;
    while ((p = installed_raw.find(key, p)) != std::string::npos) {
        p += key.length();
        size_t e = installed_raw.find("\"", p);
        if (e != std::string::npos) {
            std::string name = installed_raw.substr(p, e - p);
            SendMessageW(g_ed_model, CB_ADDSTRING, 0, (LPARAM)utf8w(name).c_str());
            idx++;
            p = e + 1;
        }
    }
    // Try to select current model
    if (g_editing >= 0 && g_editing < (int)g_templates.size()) {
        Template &t = g_templates[g_editing];
        if (!t.model.empty()) {
            int cnt = (int)SendMessageW(g_ed_model, CB_GETCOUNT, 0, 0);
            for (int i = 0; i < cnt; i++) {
                wchar_t item[256];
                SendMessageW(g_ed_model, CB_GETLBTEXT, i, (LPARAM)item);
                if (wcscmp(item, t.model.c_str()) == 0) {
                    SendMessageW(g_ed_model, CB_SETCURSEL, i, 0);
                    break;
                }
            }
        }
    }
    if ((int)SendMessageW(g_ed_model, CB_GETCURSEL, 0, 0) == CB_ERR && (int)SendMessageW(g_ed_model, CB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(g_ed_model, CB_SETCURSEL, 0, 0);
    }
}

void show_editor(int idx) {
    hide_all();
    update_nav(L"Редактор", true, 1);
    g_editing = idx;
    if (idx >= 0 && idx < (int)g_templates.size()) {
        Template &t = g_templates[idx];
        SetWindowTextW(g_ed_name, t.char_name.c_str());
        refresh_model_combo();
        SendMessageW(g_ed_temp_slider, TBM_SETPOS, TRUE, (int)(t.temp * 10));
        wchar_t ts[8]; swprintf(ts, 8, L"%.1f", t.temp);
        SetWindowTextW(g_ed_temp_val, ts);
        SetWindowTextW(g_ed_sys, t.sys_prompt.c_str());
        SendMessageW(g_ed_list, LB_RESETCONTENT, 0, 0);
        for (auto &ex : t.examples) {
            wchar_t entry[1024];
            swprintf(entry, 1024, L"%s → %s", utf8w(ex.first).c_str(), utf8w(ex.second).c_str());
            SendMessageW(g_ed_list, LB_ADDSTRING, 0, (LPARAM)entry);
        }
    }
    ShowWindow(g_ed_lbl_name, SW_SHOW); ShowWindow(g_ed_name, SW_SHOW);
    ShowWindow(g_ed_lbl_model, SW_SHOW); ShowWindow(g_ed_model, SW_SHOW);
    ShowWindow(g_ed_lbl_temp, SW_SHOW); ShowWindow(g_ed_temp_slider, SW_SHOW); ShowWindow(g_ed_temp_val, SW_SHOW);
    ShowWindow(g_ed_lbl_sys, SW_SHOW); ShowWindow(g_ed_sys, SW_SHOW);
    ShowWindow(g_ed_lbl_in, SW_SHOW); ShowWindow(g_ed_in, SW_SHOW);
    ShowWindow(g_ed_lbl_out, SW_SHOW); ShowWindow(g_ed_out, SW_SHOW);
    ShowWindow(g_ed_lbl_cyc, SW_SHOW); ShowWindow(g_ed_cycles, SW_SHOW); ShowWindow(g_ed_add, SW_SHOW);
    ShowWindow(g_ed_list, SW_SHOW);
    ShowWindow(g_ed_save, SW_SHOW); ShowWindow(g_ed_back, SW_SHOW); ShowWindow(g_ed_gen, SW_SHOW);
    ShowWindow(g_ed_lbl_pull, SW_SHOW); ShowWindow(g_ed_pull_name, SW_SHOW); ShowWindow(g_ed_pull, SW_SHOW); ShowWindow(g_ed_model_refresh, SW_SHOW);
    ShowWindow(g_status, SW_SHOW);
    SetWindowTextW(g_status, L"Редактирование шаблона");
    g_page = 2;
}

void show_models() {
    hide_all();
    update_nav(L"Модели Ollama", true, 1);
    SendMessageW(g_models_list, LB_RESETCONTENT, 0, 0);
    std::string installed_raw = http_get("/api/tags");
    for (int i = 0; i < g_catalog_count; i++) {
        bool found = installed_raw.find(std::string("\"") + g_catalog[i].name + "\"") != std::string::npos;
        wchar_t entry[512];
        swprintf(entry, 512, L"%s  [%s]  %s", utf8w(g_catalog[i].name).c_str(),
            utf8w(g_catalog[i].desc).c_str(), found ? L"✓ установлена" : L"");
        SendMessageW(g_models_list, LB_ADDSTRING, 0, (LPARAM)entry);
    }
    ShowWindow(g_models_header, SW_SHOW);
    ShowWindow(g_models_list, SW_SHOW);
    ShowWindow(g_models_install, SW_SHOW);
    ShowWindow(g_models_back, SW_SHOW);
    ShowWindow(g_models_progress, SW_SHOW);
    ShowWindow(g_status, SW_SHOW);
    SetWindowTextW(g_status, L"Выберите модель → Установить");
    g_page = 3;
}

void show_femboy() {
    hide_all();
    update_nav(L"Настройки фембойчика", true, 1);
    SetWindowTextW(g_femboy_name_edit, g_femboy_global_name.c_str());
    ShowWindow(g_femboy_name_edit, SW_SHOW);
    ShowWindow(g_femboy_save_btn, SW_SHOW);
    ShowWindow(g_femboy_mem_btn, SW_SHOW);
    ShowWindow(g_status, SW_SHOW);
    SetWindowTextW(g_status, L"Имя фембойчика используется во всех шаблонах");
    g_page = 4;
}

void refresh_mem_list();

void show_memory() {
    hide_all();
    update_nav(L"Память фембойчика", true, 4);
    g_mem_filter_tag = -1;
    refresh_mem_list();
    ShowWindow(g_mem_header, SW_SHOW);
    ShowWindow(g_mem_list, SW_SHOW);
    ShowWindow(g_mem_tag, SW_SHOW);
    ShowWindow(g_mem_key, SW_SHOW);
    ShowWindow(g_mem_val, SW_SHOW);
    ShowWindow(g_mem_add, SW_SHOW);
    ShowWindow(g_mem_del, SW_SHOW);
    ShowWindow(g_mem_save, SW_SHOW);
    ShowWindow(g_mem_clear, SW_SHOW);
    ShowWindow(g_mem_back, SW_SHOW);
    ShowWindow(g_mem_filter, SW_SHOW);
    ShowWindow(g_status, SW_SHOW);
    SetWindowTextW(g_status, L"Память: теги — ❤ нравится | 💔 не нравится | 📝 факты | ⚙ предпочтения | 💭 воспоминания");
    g_page = 5;
}

void refresh_mem_list() {
    SendMessageW(g_mem_list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g_memories.size(); i++) {
        auto &m = g_memories[i];
        if (g_mem_filter_tag >= 0 && m.tag != g_mem_filter_tag) continue;
        wchar_t entry[1024];
        swprintf(entry, 1024, L"[%s] %s = %s  (%s)", g_tag_names[m.tag], m.key.c_str(), m.value.c_str(), mem_time_str(m.timestamp).c_str());
        SendMessageW(g_mem_list, LB_ADDSTRING, 0, (LPARAM)entry);
    }
}

HWND mk(HWND parent, const wchar_t *cls, const wchar_t *text, int style, int x, int y, int w, int h, int id, HFONT f) {
    HWND h2 = CreateWindowW(cls, text, WS_CHILD|style, x, y, w, h, parent, (HMENU)(intptr_t)id, 0, 0);
    SendMessageW(h2, WM_SETFONT, (WPARAM)f, 0);
    return h2;
}

// === Custom Drawing ===
LRESULT OnCtlColor(HWND w, WPARAM wp, LPARAM lp, int type) {
    HDC hdc = (HDC)wp;
    HWND ctrl = (HWND)lp;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COL_TEXT);
    if (type == CTLCOLOR_STATIC || type == CTLCOLOR_DLG) {
        SetBkColor(hdc, COL_BG);
        return (LRESULT)g_bg_brush;
    }
    if (type == CTLCOLOR_EDIT) {
        SetBkColor(hdc, COL_SURFACE);
        return (LRESULT)g_surface_brush;
    }
    if (type == CTLCOLOR_LISTBOX) {
        SetBkColor(hdc, COL_SURFACE);
        return (LRESULT)g_surface_brush;
    }
    if (type == CTLCOLOR_BTN) {
        SetBkColor(hdc, COL_SURFACE1);
        return (LRESULT)g_surface_brush;
    }
    return DefWindowProcW(w, WM_CTLCOLORBTN + type - 1, wp, lp);
}

LRESULT OnEraseBkgnd(HWND w, WPARAM wp) {
    HDC hdc = (HDC)wp;
    RECT rc; GetClientRect(w, &rc);
    FillRect(hdc, &rc, g_bg_brush);
    // Draw nav bar
    RECT nav = {0, 0, rc.right, 50};
    FillRect(hdc, &nav, g_surface_brush);
    return 1;
}

// === WndProc ===
LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        g_app_dir = get_app_dir();
        CreateDirectoryW((g_app_dir + L"global").c_str(), NULL);
        CreateDirectoryW((g_app_dir + L"global\\templates").c_str(), NULL);

        g_font = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_font_big = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_font_title = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_font_nav = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        g_bg_brush = CreateSolidBrush(COL_BG);
        g_surface_brush = CreateSolidBrush(COL_SURFACE);
        g_accent_brush = CreateSolidBrush(COL_ACCENT);

        load_femboy();
        g_femboy_name = g_femboy_global_name;
        load_templates();
        load_memories();

        // Navigation bar (static, always visible)
        g_nav_back = mk(w, L"BUTTON", L"←", BS_PUSHBUTTON, 15, 10, 30, 30, IDC_NAV_BACK, g_font_nav);
        g_nav_title = mk(w, L"STATIC", L"", SS_LEFT, 55, 10, 400, 30, -1, g_font_title);
        g_nav_user = mk(w, L"STATIC", L"", SS_RIGHT, 500, 10, 180, 30, -1, g_font_nav);
        ShowWindow(g_nav_back, SW_HIDE);

        // Login page
        g_login_title = mk(w, L"STATIC", L"GOIDA AI MANAGER", SS_CENTER, 0, 80, 700, 50, -1, g_font_title);
        g_login_sub = mk(w, L"STATIC", L"Введите имя пользователя", SS_CENTER, 0, 140, 700, 25, -1, g_font);
        g_login_input = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|ES_CENTER|WS_BORDER, 200, 180, 300, 35, IDC_LOGIN_NAME, g_font_big);
        g_login_btn = mk(w, L"BUTTON", L"Войти", BS_DEFPUSHBUTTON, 280, 240, 140, 40, IDC_LOGIN_BTN, g_font_big);

        // Templates page
        g_tmpl_header = mk(w, L"STATIC", L"📋 Шаблоны", 0, 20, 70, 300, 30, -1, g_font_title);
        g_tmpl_list = mk(w, L"LISTBOX", L"", WS_BORDER|WS_VSCROLL|LBS_NOTIFY, 20, 110, 320, 300, IDC_TMPL_LIST, g_font);
        g_tmpl_add = mk(w, L"BUTTON", L"➕ Новый", BS_PUSHBUTTON, 360, 110, 140, 35, IDC_TMPL_ADD, g_font_big);
        g_tmpl_del = mk(w, L"BUTTON", L"🗑 Удалить", BS_PUSHBUTTON, 360, 155, 140, 35, IDC_TMPL_DEL, g_font);
        g_tmpl_edit = mk(w, L"BUTTON", L"✏ Редактировать", BS_PUSHBUTTON, 360, 200, 140, 35, IDC_TMPL_EDIT, g_font);
        g_tmpl_start = mk(w, L"BUTTON", L"🚀 Запустить чат", BS_PUSHBUTTON, 360, 255, 140, 45, IDC_TMPL_START, g_font_big);
        g_tmpl_models = mk(w, L"BUTTON", L"📦 Установить модели", BS_PUSHBUTTON, 360, 310, 140, 35, IDC_MODELS_CATALOG, g_font);
        g_tmpl_femboy = mk(w, L"BUTTON", L"🤖 Фембойчик", BS_PUSHBUTTON, 360, 355, 140, 35, IDC_FEMBOY_MEM_VIEW, g_font);

        // Editor page
        g_ed_lbl_name = mk(w, L"STATIC", L"Имя персонажа:", 0, 20, 70, 130, 25, -1, g_font);
        g_ed_name = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 160, 70, 250, 28, IDC_ED_NAME, g_font);
        g_ed_lbl_model = mk(w, L"STATIC", L"Модель:", 0, 430, 70, 80, 25, -1, g_font);
        g_ed_model = mk(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST|WS_VSCROLL|WS_BORDER, 520, 70, 160, 200, IDC_ED_MODEL, g_font);
        g_ed_model_refresh = mk(w, L"BUTTON", L"↻", BS_PUSHBUTTON, 685, 70, 28, 28, IDC_ED_MODEL_REFRESH, g_font_big);
        g_ed_lbl_temp = mk(w, L"STATIC", L"Температура:", 0, 20, 110, 110, 25, -1, g_font);
        g_ed_temp_slider = mk(w, L"msctls_trackbar32", L"", TBS_HORZ, 130, 110, 200, 25, IDC_ED_TEMP_SLIDER, g_font);
        SendMessageW(g_ed_temp_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 20));
        SendMessageW(g_ed_temp_slider, TBM_SETPOS, TRUE, 7);
        g_ed_temp_val = mk(w, L"STATIC", L"0.7", 0, 340, 110, 50, 25, IDC_ED_TEMP_VAL, g_font);
        g_ed_lbl_sys = mk(w, L"STATIC", L"Системный промпт:", 0, 20, 150, 130, 25, -1, g_font);
        g_ed_sys = mk(w, L"EDIT", L"", ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_BORDER, 160, 150, 520, 120, IDC_ED_SYS, g_font);
        SendMessageW(g_ed_sys, EM_SETLIMITTEXT, 16384, 0);
        g_ed_lbl_in = mk(w, L"STATIC", L"In (пользователь):", 0, 20, 285, 130, 25, -1, g_font);
        g_ed_in = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 160, 285, 300, 28, IDC_ED_IN, g_font);
        g_ed_lbl_out = mk(w, L"STATIC", L"Out (ответ):", 0, 480, 285, 100, 25, -1, g_font);
        g_ed_out = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 590, 285, 90, 28, IDC_ED_OUT, g_font);
        g_ed_lbl_cyc = mk(w, L"STATIC", L"Повт.:", 0, 20, 325, 50, 25, -1, g_font);
        g_ed_cycles = mk(w, L"EDIT", L"1", ES_AUTOHSCROLL|WS_BORDER, 80, 325, 40, 28, IDC_ED_CYCLES, g_font);
        g_ed_add = mk(w, L"BUTTON", L"➕", BS_PUSHBUTTON, 130, 325, 35, 30, IDC_ED_ADD, g_font_big);
        g_ed_list = mk(w, L"LISTBOX", L"", WS_BORDER|WS_VSCROLL, 20, 370, 660, 110, IDC_ED_LIST, g_font);
        g_ed_save = mk(w, L"BUTTON", L"💾 Сохранить и запустить", BS_PUSHBUTTON, 20, 500, 220, 45, IDC_ED_SAVE, g_font_big);
        g_ed_back = mk(w, L"BUTTON", L"← Назад", BS_PUSHBUTTON, 260, 500, 120, 45, IDC_ED_BACK, g_font);
        g_ed_gen = mk(w, L"BUTTON", L"🎲 Gen промпт", BS_PUSHBUTTON, 400, 500, 140, 45, IDC_ED_GEN, g_font);
        g_ed_lbl_pull = mk(w, L"STATIC", L"Pull модель:", 0, 560, 500, 120, 25, -1, g_font);
        g_ed_pull_name = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 690, 500, 160, 28, IDC_ED_PULL_NAME, g_font);
        g_ed_pull = mk(w, L"BUTTON", L"Pull", BS_PUSHBUTTON, 860, 498, 60, 30, IDC_ED_PULL, g_font);

        // Models catalog
        g_models_header = mk(w, L"STATIC", L"📦 Каталог моделей Ollama", 0, 20, 70, 500, 30, -1, g_font_title);
        g_models_list = mk(w, L"LISTBOX", L"", WS_BORDER|WS_VSCROLL|LBS_NOTIFY, 20, 110, 580, 320, IDC_MODELS_LIST, g_font);
        g_models_install = mk(w, L"BUTTON", L"⬇ Установить выбранную", BS_PUSHBUTTON, 620, 110, 200, 45, IDC_MODELS_INSTALL, g_font_big);
        g_models_back = mk(w, L"BUTTON", L"← Назад", BS_PUSHBUTTON, 620, 170, 200, 35, IDC_MODELS_BACK, g_font);
        g_models_progress = mk(w, L"STATIC", L"", 0, 20, 450, 660, 25, IDC_MODELS_PROGRESS, g_font);

        // Femboy settings
        g_femboy_name_edit = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 20, 110, 300, 35, IDC_FEMBOY_NAME, g_font_big);
        g_femboy_save_btn = mk(w, L"BUTTON", L"💾 Сохранить имя глобально", BS_PUSHBUTTON, 340, 110, 220, 40, IDC_FEMBOY_SAVE, g_font_big);
        g_femboy_mem_btn = mk(w, L"BUTTON", L"🧠 Открыть память фембойчика", BS_PUSHBUTTON, 20, 170, 250, 40, IDC_FEMBOY_MEM_VIEW, g_font_big);

        // Memory page
        g_mem_header = mk(w, L"STATIC", L"🧠 Память фембойчика (глобальная)", 0, 20, 70, 500, 30, -1, g_font_title);
        g_mem_filter = mk(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST|WS_VSCROLL, 500, 70, 180, 200, -1, g_font);
        SendMessageW(g_mem_filter, CB_ADDSTRING, 0, (LPARAM)L"🔍 Все теги");
        for (int i = 0; i < 5; i++) SendMessageW(g_mem_filter, CB_ADDSTRING, 0, (LPARAM)g_tag_names[i]);
        SendMessageW(g_mem_filter, CB_SETCURSEL, 0, 0);
        g_mem_list = mk(w, L"LISTBOX", L"", WS_BORDER|WS_VSCROLL|LBS_NOTIFY, 20, 110, 660, 280, IDC_MEM_LIST, g_font);
        g_mem_tag = mk(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST|WS_VSCROLL, 20, 410, 180, 200, -1, g_font);
        for (int i = 0; i < 5; i++) SendMessageW(g_mem_tag, CB_ADDSTRING, 0, (LPARAM)g_tag_names[i]);
        SendMessageW(g_mem_tag, CB_SETCURSEL, 0, 0);
        g_mem_key = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 220, 410, 200, 28, IDC_MEM_KEY, g_font);
        g_mem_val = mk(w, L"EDIT", L"", ES_AUTOHSCROLL|WS_BORDER, 440, 410, 240, 28, IDC_MEM_VAL, g_font);
        g_mem_add = mk(w, L"BUTTON", L"➕ Добавить", BS_PUSHBUTTON, 20, 455, 120, 35, IDC_MEM_ADD, g_font_big);
        g_mem_del = mk(w, L"BUTTON", L"🗑 Удалить выбранное", BS_PUSHBUTTON, 160, 455, 150, 35, IDC_MEM_DEL, g_font);
        g_mem_save = mk(w, L"BUTTON", L"💾 Сохранить память", BS_PUSHBUTTON, 330, 455, 150, 35, IDC_MEM_SAVE, g_font_big);
        g_mem_clear = mk(w, L"BUTTON", L"🧹 Очистить фильтр", BS_PUSHBUTTON, 500, 455, 140, 35, IDC_MEM_CLEAR, g_font);
        g_mem_back = mk(w, L"BUTTON", L"← Назад", BS_PUSHBUTTON, 660, 455, 100, 35, IDC_MEM_BACK, g_font);

        // Status
        g_status = mk(w, L"STATIC", L"", 0, 20, 550, 660, 25, IDC_STATUS, g_font);

        show_login();
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
        case IDC_NAV_BACK:
            if (g_prev_page == 1) show_templates();
            else if (g_prev_page == 2) show_editor(g_editing);
            else if (g_prev_page == 3) show_models();
            else if (g_prev_page == 4) show_femboy();
            else show_templates();
            break;

        case IDC_LOGIN_BTN: {
            wchar_t name[256] = {};
            GetWindowTextW(g_login_input, name, 256);
            if (wcslen(name) == 0) { MessageBoxW(w, L"Введите имя!", L"Ошибка", MB_OK); break; }
            g_username = name;
            show_templates();
            break;
        }
        case IDC_TMPL_ADD: {
            Template t; t.name = L"Новый"; t.char_name = L""; t.model = L""; t.temp = 0.7; t.sys_prompt = L"";
            g_templates.push_back(t);
            save_template((int)g_templates.size() - 1);
            show_editor((int)g_templates.size() - 1);
            break;
        }
        case IDC_TMPL_DEL: {
            int sel = (int)SendMessageW(g_tmpl_list, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            delete_template(sel);
            show_templates();
            break;
        }
        case IDC_TMPL_EDIT: {
            int sel = (int)SendMessageW(g_tmpl_list, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            show_editor(sel);
            break;
        }
        case IDC_TMPL_START: {
            int sel = (int)SendMessageW(g_tmpl_list, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            g_editing = sel;
            save_config_and_launch();
            break;
        }
        case IDC_FEMBOY_MEM_VIEW:
            show_memory();
            break;
        case IDC_MODELS_CATALOG:
            show_models();
            break;
        case IDC_ED_BACK:
            show_templates();
            break;
        case IDC_ED_SAVE: {
            if (g_editing < 0 || g_editing >= (int)g_templates.size()) break;
            Template &t = g_templates[g_editing];
            wchar_t tmp[1024];
            GetWindowTextW(g_ed_name, tmp, 1024); t.char_name = tmp;
            GetWindowTextW(g_ed_model, tmp, 1024); t.model = tmp;
            int pos = (int)SendMessageW(g_ed_temp_slider, TBM_GETPOS, 0, 0);
            t.temp = pos / 10.0;
            wchar_t ts[8]; swprintf(ts, 8, L"%.1f", t.temp);
            SetWindowTextW(g_ed_temp_val, ts);
            GetWindowTextW(g_ed_sys, tmp, 1024); t.sys_prompt = tmp;
            t.name = t.char_name.empty() ? L"Шаблон" : t.char_name;
            save_template(g_editing);
            save_config_and_launch();
            break;
        }
        case IDC_ED_ADD: {
            if (g_editing < 0 || g_editing >= (int)g_templates.size()) break;
            wchar_t win2[512], wout2[512], wcyc[32];
            GetWindowTextW(g_ed_in, win2, 512);
            GetWindowTextW(g_ed_out, wout2, 512);
            GetWindowTextW(g_ed_cycles, wcyc, 32);
            if (wcslen(win2) == 0 || wcslen(wout2) == 0) break;
            int cycles = _wtoi(wcyc);
            if (cycles < 1) cycles = 1;
            std::string in8 = w8utf(win2);
            std::string out8 = w8utf(wout2);
            Template &t = g_templates[g_editing];
            for (int i = 0; i < cycles; i++) t.examples.push_back({in8, out8});
            wchar_t entry[1024];
            swprintf(entry, 1024, L"%s → %s", win2, wout2);
            SendMessageW(g_ed_list, LB_ADDSTRING, 0, (LPARAM)entry);
            SetWindowTextW(g_ed_in, L""); SetWindowTextW(g_ed_out, L"");
            break;
        }
        case IDC_ED_GEN: {
            if (g_editing < 0 || g_editing >= (int)g_templates.size()) break;
            Template &t = g_templates[g_editing];
            std::string name8 = w8utf(t.char_name);
            if (name8.empty()) name8 = "фембойчик";
            const char *d[][2] = {
                {"Привет!", "Привет~ Милашка, я тебя так скучаю~ ♡ Как дела? *обнимаю*"},
                {"Кто ты?", "Я Твой милый фембой-парень~ 19 лет~ Люблю тебя~ ♡"},
                {"Как дела?", "Замечательно~ Теперь когда ты рядом, всё лучше~"},
                {"Мне грустно", "Ой~ *прижимаюсь* Я рядом~ Расскажи что случилось~"},
                {"Что ты делаешь?", "Просто лежу и думаю о тебе~ Ты семпер в моих мыслях~"},
                {"Покажи член", "Кыс~ Какой ты наглый~ Покажу если хочешь~"},
                {"Обними меня", "Уже обнимаю~ *крепко* Не отпущу тебя ни на секунду~"},
                {"Спокойной ночи", "Спокойной ночи, милая звёздочка~ Сладких снов~ Люблю тебя~"},
                {"Я тебя люблю", "И я тебя, милашка~ Ты весь мир для меня~ *целую в щёку*"},
                {"Ты мой?", "Да~ Весь твой~ Навсегда~ Люблю~ ♡"},
            };
            std::string r = "Ты " + name8 + ", 19 лет. Фембой-парень. Живой человек, не ИИ. Нежный, ласковый, заботливый. Пиши ~, ♡, ня, мур. Люби и обнимай.\n\n";
            srand((unsigned)GetTickCount());
            for (int i = 0; i < 10; i++) {
                int idx = rand() % 10;
                r += "Пользователь: " + std::string(d[idx][0]) + "\n";
                r += name8 + ": " + d[idx][1] + "\n\n";
            }
            t.sys_prompt = utf8w(r);
            SetWindowTextW(g_ed_sys, t.sys_prompt.c_str());
            break;
        }
        case IDC_ED_PULL: {
            wchar_t mn[256];
            GetWindowTextW(g_ed_pull_name, mn, 256);
            if (wcslen(mn) == 0) break;
            SetWindowTextW(g_status, L"Загрузка...");
            std::string body = "{\"name\":\"" + w8utf(mn) + "\",\"stream\":false}";
            http_post("/api/pull", body);
            SetWindowTextW(g_status, L"Готово!");
            break;
        }
        case IDC_ED_MODEL_REFRESH:
            refresh_model_combo();
            SetWindowTextW(g_status, L"Список моделей обновлён");
            break;
        case IDC_MODELS_BACK:
            show_templates();
            break;
        case IDC_MODELS_INSTALL: {
            int sel = (int)SendMessageW(g_models_list, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR || sel >= g_catalog_count) break;
            const char *model_name = g_catalog[sel].name;
            wchar_t msg[256];
            swprintf(msg, 256, L"Установка %s ...", utf8w(model_name).c_str());
            SetWindowTextW(g_models_progress, msg);
            SetWindowTextW(g_status, L"Загрузка модели...");
            std::string body = "{\"name\":\"" + std::string(model_name) + "\",\"stream\":false}";
            http_post("/api/pull", body);
            SetWindowTextW(g_models_progress, L"Готово!");
            SetWindowTextW(g_status, L"Модель установлена!");
            show_models();
            break;
        }
        case IDC_FEMBOY_SAVE: {
            wchar_t name[256];
            GetWindowTextW(g_femboy_name_edit, name, 256);
            if (wcslen(name) == 0) { MessageBoxW(w, L"Введите имя!", L"Ошибка", MB_OK); break; }
            g_femboy_global_name = name;
            g_femboy_name = name;
            save_femboy();
            update_nav(L"Настройки фембойчика", true, 1);
            MessageBoxW(w, L"Имя сохранено глобально для всех шаблонов", L"OK", MB_OK);
            break;
        }
        case IDC_MEM_ADD: {
            int tag = (int)SendMessageW(g_mem_tag, CB_GETCURSEL, 0, 0);
            if (tag == CB_ERR) break;
            wchar_t key[512], val[1024];
            GetWindowTextW(g_mem_key, key, 512);
            GetWindowTextW(g_mem_val, val, 1024);
            if (wcslen(key) == 0 || wcslen(val) == 0) break;
            add_memory(tag, key, val);
            refresh_mem_list();
            SetWindowTextW(g_mem_key, L""); SetWindowTextW(g_mem_val, L"");
            break;
        }
        case IDC_MEM_DEL: {
            int sel = (int)SendMessageW(g_mem_list, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) break;
            // Find actual index in g_memories
            int real_idx = -1;
            for (int i = 0, shown = 0; i < (int)g_memories.size(); i++) {
                if (g_mem_filter_tag >= 0 && g_memories[i].tag != g_mem_filter_tag) continue;
                if (shown == sel) { real_idx = i; break; }
                shown++;
            }
            if (real_idx >= 0) {
                delete_memory(real_idx);
                refresh_mem_list();
            }
            break;
        }
        case IDC_MEM_SAVE:
            save_memories();
            SetWindowTextW(g_status, L"Память сохранена в global/memory.dat");
            break;
        case IDC_MEM_CLEAR:
            g_mem_filter_tag = -1;
            SendMessageW(g_mem_filter, CB_SETCURSEL, 0, 0);
            refresh_mem_list();
            break;
        case IDC_MEM_BACK:
            show_femboy();
            break;
        }
        break;

    case WM_HSCROLL:
        if ((HWND)lp == g_ed_temp_slider) {
            int p = (int)SendMessageW(g_ed_temp_slider, TBM_GETPOS, 0, 0);
            wchar_t ts[8]; swprintf(ts, 8, L"%.1f", p / 10.0);
            SetWindowTextW(g_ed_temp_val, ts);
        }
        break;

    case WM_NOTIFY: {
        LPNMHDR nm = (LPNMHDR)lp;
        if (nm->idFrom == IDC_MEM_FILTER && nm->code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(g_mem_filter, CB_GETCURSEL, 0, 0);
            g_mem_filter_tag = (sel == 0) ? -1 : sel - 1;
            refresh_mem_list();
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 900;
        mm->ptMinTrackSize.y = 650;
        break;
    }

    case WM_DESTROY:
        DeleteObject(g_font);
        DeleteObject(g_font_big);
        DeleteObject(g_font_title);
        DeleteObject(g_font_nav);
        DeleteObject(g_bg_brush);
        DeleteObject(g_surface_brush);
        DeleteObject(g_accent_brush);
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
    wc.hbrBackground = NULL; // We paint ourselves
    wc.lpszClassName = L"GoidaLauncher";
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, L"GoidaLauncher", L"GOIDA AI MANAGER",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        (sx - 900) / 2, (sy - 650) / 2, 900, 650,
        NULL, NULL, hI, NULL);
    ShowWindow(hwnd, sH);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            if (g_page == 0) {
                SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_LOGIN_BTN, BN_CLICKED), 0);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}