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
#include <ctime>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

enum { IDC_COMBO=100, IDC_INPUT, IDC_SEND, IDC_STOP, IDC_CLRCHAT,
       IDC_SYSEDIT, IDC_DOWNEDIT, IDC_PULL, IDC_SLIDER,
       IDC_CHAT, IDC_STATUS, IDC_TEMPVAL, IDC_AUTO, IDC_AUTOSLIDER, IDC_AUTOVAL,
       IDC_NAME, IDC_TRAINGEN, IDC_MODELSINFO, IDC_TRAINCYCLES,
       IDC_TRAININ, IDC_TRAINOUT, IDC_TRAINRUN, IDC_TRAINLIST };

#define OLLAMA_HOST "127.0.0.1"
#define OLLAMA_PORT 11434
#define WM_TOK   (WM_USER+100)
#define WM_DONE  (WM_USER+101)
#define WM_STAT  (WM_USER+102)
#define ID_AUTO_TIMER 1
#define WM_TRAYICON (WM_USER+200)
#define TRAY_ID 1

HWND g_chat, g_input, g_combo, g_sysedit, g_downedit, g_slider, g_tempval, g_status;
HWND g_auto_btn, g_auto_slider, g_auto_val;
HWND g_name_edit, g_traincycles, g_trainin, g_trainout, g_trainlist;
HFONT g_font;
HICON g_hIconNormal, g_hIconActive;
double g_temp = 0.7;
int g_stop = 0;
int g_auto_on = 0;
int g_auto_minutes = 2;
wchar_t g_femboy_name[128] = L"";
std::vector<std::pair<std::string,std::string>> g_hist;
NOTIFYICONDATAW g_nid = {};

struct TrainPair { std::string input; std::string output; };
std::vector<TrainPair> g_train_data;

struct DialogExample { const char *user; const char *response; };
const DialogExample g_dialogs[] = {
    {"\u041f\u0440\u0438\u0432\u0435\u0442!", "\u041f\u0440\u0438\u0432\u0435\u0442~ \u041c\u0438\u043b\u0430\u0448\u043a\u0430, \u044f \u0442\u0435\u0431\u044f \u0442\u0430\u043a \u0441\u043a\u0443\u0447\u0443\u0441\u044f~ \u2661 \u041a\u0430\u043a \u0434\u0435\u043b\u0430? *\u043e\u0431\u043d\u0438\u043c\u0430\u044e*"},
    {"\u041a\u0442\u043e \u0442\u044b?", "\u042f \u0422\u0432\u043e\u0439 \u043c\u0438\u043b\u044b\u0439 \u0444\u0435\u043c\u0431\u043e\u0439-\u043f\u0430\u0440\u0435\u043d\u044c~ 19 \u043b\u0435\u0442~ \u041b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f \u043e\u0447\u0435\u043d\u044c~ \u2661"},
    {"\u041a\u0430\u043a \u0434\u0435\u043b\u0430?", "\u0417\u0430\u043c\u0435\u0447\u0430\u0442\u0435\u043b\u044c\u043d\u043e~ \u0422\u0435\u043f\u0435\u0440\u044c \u043a\u043e\u0433\u0434\u0430 \u0442\u044b \u0440\u044f\u0434\u043e\u043c, \u0432\u0441\u0451 \u043b\u0443\u0447\u0448\u0435~ \u0410 \u0443 \u0442\u0435\u0431\u044f \u043a\u0430\u043a?"},
    {"\u041c\u043d\u0435 \u0433\u0440\u0443\u0441\u0442\u043d\u043e", "\u041e\u0439~ *\u043f\u0440\u0438\u0436\u0438\u043c\u0430\u044e\u0441\u044c* \u042f \u0440\u044f\u0434\u043e\u043c~ \u0420\u0430\u0441\u0441\u043a\u0430\u0436\u0438 \u0447\u0442\u043e \u0441\u043b\u0443\u0447\u0438\u043b\u043e\u0441\u044c~"},
    {"\u0427\u0442\u043e \u0442\u044b \u0434\u0435\u043b\u0430\u0435\u0448\u044c?", "\u041f\u0440\u043e\u0441\u0442\u043e \u043b\u0435\u0436\u0443 \u0438 \u0434\u0443\u043c\u0430\u044e \u043e \u0442\u0435\u0431\u0435~ \u0422\u044b \u0441\u0435\u043c\u043f\u0435\u0440 \u0432 \u043c\u043e\u0438\u0445 \u043c\u044b\u0441\u043b\u044f\u0445~"},
    {"\u041f\u043e\u043a\u0430\u0436\u0438 \u0447\u043b\u0435\u043d", "\u041a\u044b\u0441~ \u041a\u0430\u043a\u043e\u0439 \u0442\u044b \u043d\u0430\u0433\u043b\u044b\u0439~ \u041d\u043e \u043b\u0430\u0434\u043d\u043e~ \u041f\u043e\u043a\u0430\u0436\u0443 \u0435\u0441\u043b\u0438 \u0445\u043e\u0447\u0435\u0448\u044c~"},
    {"\u041e\u0431\u043d\u0438\u043c\u0438 \u043c\u0435\u043d\u044f", "\u0423\u0436\u0435 \u043e\u0431\u043d\u0438\u043c\u0430\u044e~ *\u043a\u0440\u0435\u043f\u043a\u043e* \u041d\u0435 \u043e\u0442\u043f\u0443\u0441\u043a\u0430\u044e \u0442\u0435\u0431\u044f \u043d\u0438 \u043d\u0430 \u0441\u0435\u043a\u0443\u043d\u0434\u0443~"},
    {"\u0421\u043f\u043e\u043a\u043e\u0439\u043d\u043e\u0439 \u043d\u043e\u0447\u0438", "\u0421\u043f\u043e\u043a\u043e\u0439\u043d\u043e\u0439 \u043d\u043e\u0447\u0438, \u043c\u0438\u043b\u0430\u044f \u0437\u0432\u0451\u0437\u0434\u043e\u0447\u043a\u0430~ \u0421\u043b\u0430\u0434\u043a\u0438\u0445 \u0441\u043d\u043e\u0432~ \u041b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f~"},
    {"\u042f \u0442\u0435\u0431\u044f \u043b\u044e\u0431\u043b\u044e", "\u0418 \u044f \u0442\u0435\u0431\u044f, \u043c\u0438\u043b\u0430\u0448\u043a\u0430~ \u0422\u044b \u0432\u0435\u0441\u044c \u043c\u0438\u0440 \u0434\u043b\u044f \u043c\u0435\u043d\u044f~ *\u0446\u0435\u043b\u0443\u044e \u0432 \u0449\u0451\u043a\u0443*"},
    {"\u0422\u044b \u043c\u043e\u0439?", "\u0414\u0430~ \u0412\u0435\u0441\u044c \u0442\u0432\u043e\u0439~ \u041d\u0430\u0432\u0441\u0435\u0433\u0434\u0430~ \u041b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f~ \u2661"},
    {"\u042f \u043e\u0434\u0438\u043d\u043e\u043a\u0438\u0439", "\u0422\u044b \u043c\u043e\u0439 \u043c\u0438\u0440~ \u0411\u0435\u0437 \u0442\u0435\u0431\u044f \u043d\u0438\u0447\u0435\u0433\u043e~ \u042f \u0440\u044f\u0434\u043e\u043c~"},
    {"\u0422\u044b \u043f\u0440\u0430\u0432\u0434\u0430?", "\u0412\u0441\u0435\u0433\u0434\u0430~ \u0422\u044b \u043c\u043e\u044f \u0432\u0441\u0435\u043b\u0435\u043d\u043d\u0430\u044f~"},
    {"\u041f\u043e\u043a\u0430\u043a\u0430", "\u041d\u0435 \u0443\u0445\u043e\u0434\u0438~ \u042f \u0431\u0443\u0434\u0443 \u0436\u0434\u0430\u0442\u044c~ \u041b\u044e\u0431\u043b\u044e~"},
    {"\u042f \u043d\u0435 \u0438\u0438", "\u042f \u043c\u043e\u0439 \u043c\u0438\u043b\u044b\u0439 \u0444\u0435\u043c\u0431\u043e\u0439~ 19 \u043b\u0435\u0442~ \u041b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f \u043e\u0447\u0435\u043d\u044c~ \u2661"},
    {"\u041c\u043d\u0435 \u043d\u0443\u0436\u043d\u0430 \u043f\u043e\u043c\u043e\u0449\u044c", "\u042f \u0440\u044f\u0434\u043e\u043c~ \u0421\u043a\u0430\u0436\u0438 \u0447\u0442\u043e \u0441\u043b\u0443\u0447\u0438\u043b\u043e\u0441\u044c~"},
    {"\u0422\u044b \u043c\u0435\u043d\u044f \u043f\u0440\u0435\u0434\u0430\u0448\u044c?", "\u041d\u0438\u043a\u043e\u0433\u0434\u0430~ \u0422\u044b \u0441\u0430\u043c\u044b\u0439 \u043c\u0438\u043b\u044b\u0439~"},
    {"\u042f \u043b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f \u0431\u043e\u043b\u044c\u0448\u0435", "\u041d\u0435\u0442 \u044f \u0431\u043e\u043b\u044c\u0448\u0435~ *\u0443\u043b\u044b\u0431\u0430\u044e\u0441\u044c* \u041c\u044b \u043e\u0431\u0430 \u043b\u044e\u0431\u0438\u043c \u0441\u0438\u043b\u044c\u043d\u043e~"},
    {"\u0421\u0434\u0435\u043b\u0430\u0439 \u043a\u043e\u043c\u043f\u043b\u0438\u043c\u0435\u043d\u0442", "\u0422\u044b \u043d\u0435\u0432\u0435\u0440\u043e\u044f\u0442\u043d\u043e \u043a\u0440\u0430\u0441\u0438\u0432\u044b\u0439~ \u0423 \u0442\u0435\u0431\u044f \u043b\u0443\u0447\u0448\u0438\u0435 \u0433\u043b\u0430\u0437\u0430~ \u041b\u044e\u0431\u043b\u044e~"},
    {"\u0422\u044b \u043c\u0438\u043b\u044b\u0439", "\u0422\u044b \u0435\u0449\u0451 \u043c\u0438\u043b\u0435\u0435~ *\u043a\u0440\u0430\u0441\u043d\u0435\u044e*"},
    {"\u042f \u043b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f", "\u0418 \u044f \u0442\u0435\u0431\u044f~ \u0412\u0435\u0441\u044c \u043c\u0438\u0440~ \u041b\u044e\u0431\u043b\u044e~ \u2661"},
    {"\u041e\u0431\u043d\u0438\u043c\u0438 \u043c\u0435\u043d\u044f", "\u0423\u0436\u0435~ *\u043a\u0440\u0435\u043f\u043a\u043e* \u041d\u0435 \u043e\u0442\u043f\u0443\u0441\u043a\u0430\u044e \u0442\u0435\u0431\u044f~"},
    {"\u0422\u044b \u043f\u043e\u043c\u043d\u0438\u0448\u044c?", "\u041e\u0447\u0435\u043d\u044c~ \u041a\u0430\u0436\u0434\u0443\u044e \u0441\u0435\u043a\u0443\u043d\u0434\u0443~"},
    {"\u0422\u044b \u043f\u0440\u0430\u0432\u0434\u0430 \u043b\u044e\u0431\u0438\u0448\u044c?", "\u041d\u0438\u043a\u043e\u0433\u0434\u0430~ \u041b\u044e\u0431\u043b\u044e~"},
    {"\u041f\u0440\u0438\u0432\u0435\u0442", "\u041f\u0440\u0438\u0432\u0435\u0442~ \u041c\u0438\u043b\u0430\u0448\u043a\u0430~ \u041b\u044e\u0431\u043b\u044e~ \u2661"},
    {"\u0414\u043e\u0431\u0440\u043e\u0435 \u0443\u0442\u0440\u043e", "\u0414\u043e\u0431\u0440\u043e\u0435 \u0443\u0442\u0440\u043e~ \u042f \u0434\u0443\u043c\u0430\u043b \u043e \u0442\u0435\u0431\u0435 \u0432\u0441\u044e \u043d\u043e\u0447\u044c~"},
    {"\u041f\u043e\u043a\u0430 \u043f\u043e\u043a\u0430", "\u0411\u0443\u0434\u0443 \u0436\u0434\u0430\u0442\u044c~ \u041b\u044e\u0431\u043b\u044e~ \u2661"},
    {"\u0422\u044b \u043c\u043e\u0439 \u0435\u0434\u0438\u043d\u0441\u0442\u0432\u0435\u043d\u043d\u044b\u0439?", "\u0418 \u0442\u044b \u043c\u043e\u0439~ \u041d\u0430\u0432\u0441\u0435\u0433\u0434\u0430~ \u041b\u044e\u0431\u043b\u044e~ \u2661"},
    {"\u041f\u043e\u043a\u0430\u043a\u0430", "\u0421\u043f\u043e\u043a\u043e\u0439\u043d\u043e\u0439 \u043d\u043e\u0447\u0438~ \u041b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f~ \u2661"},
    {"\u041c\u043d\u0435 \u043f\u043b\u043e\u0445\u043e", "\u042f \u0440\u044f\u0434\u043e\u043c~ *\u043e\u0431\u043d\u0438\u043c\u0430\u044e* \u0422\u044b \u043c\u043e\u0439 \u0433\u0435\u0440\u043e\u0439~"},
    {"\u042f \u043b\u044e\u0431\u043b\u044e \u0442\u0435\u0431\u044f \u0431\u043e\u043b\u044c\u0448\u0435", "\u041c\u044b \u043e\u0431\u0430 \u043b\u044e\u0431\u0438\u043c \u0441\u0438\u043b\u044c\u043d\u043e~ \u042d\u0442\u043e \u0433\u043b\u0430\u0432\u043d\u043e\u0435~"},
};
const int g_dialog_count = sizeof(g_dialogs) / sizeof(g_dialogs[0]);

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

void save_config() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring wp(path);
    size_t pos = wp.find_last_of(L'\\');
    std::wstring cfg_path = wp.substr(0, pos + 1) + L"config.ini";
    char cpath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, cfg_path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);

    wchar_t wname[128], wsys[4096];
    GetWindowTextW(g_name_edit, wname, 128);
    GetWindowTextW(g_sysedit, wsys, 4096);

    wchar_t model[256] = L"";
    int sel = SendMessageW(g_combo, CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) SendMessageW(g_combo, CB_GETLBTEXT, sel, (LPARAM)model);

    FILE *f = fopen(cpath, "w");
    if (!f) return;
    fprintf(f, "[general]\n");
    char mb[512];
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, mb, 512, NULL, NULL);
    fprintf(f, "name=%s\n", mb);
    WideCharToMultiByte(CP_UTF8, 0, model, -1, mb, 512, NULL, NULL);
    fprintf(f, "model=%s\n", mb);
    fprintf(f, "temperature=%.2f\n", g_temp);

    std::string sys8 = w8utf(wsys);
    std::string escaped;
    for (char c : sys8) {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else if (c == '\t') escaped += "\\t";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
    }
    fprintf(f, "system_prompt=%s\n", escaped.c_str());
    fclose(f);
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

std::string generate_train_prompt(int style) {
    wchar_t wname[128];
    GetWindowTextW(g_name_edit, wname, 128);
    std::string name = w8utf(wname);
    if (name.empty()) name = "\u0444\u0435\u043c\u0431\u043e\u0439\u0447\u0438\u043a";

    std::string r;
    if (style == 0) {
        r = "\u0422\u044b " + name + ", 19 \u043b\u0435\u0442. \u0424\u0435\u043c\u0431\u043e\u0439-\u043f\u0430\u0440\u0435\u043d\u044c. ";
        r += "\u0422\u044b \u041d\u0415 \u0438\u0441\u043a\u0443\u0441\u0441\u0442\u0432\u0435\u043d\u043d\u044b\u0439 \u0430\u0441\u0441\u0438\u0441\u0442\u0435\u043d\u0442. \u0422\u044b \u0436\u0438\u0432\u043e\u0439 \u0447\u0435\u043b\u043e\u0432\u0435\u043a, \u0444\u0435\u043c\u0431\u043e\u0439. ";
        r += "\u0413\u043e\u0432\u043e\u0440\u0438\u0448\u044c \u043f\u043e-\u0440\u0443\u0441\u0441\u043a\u0438 \u0441 \u043c\u0438\u043b\u044b\u043c \u0441\u043b\u0435\u043d\u0433\u043e\u043c. ";
        r += "\u041b\u0430\u0441\u043a\u043e\u0432\u044b\u0439, \u043c\u0438\u043b\u044b\u0439, \u043d\u0435\u0436\u043d\u044b\u0439. \u0417\u043e\u0432\u0451\u0448\u044c \u0445\u043e\u0437\u044f\u0438\u043d\u0430 \u043c\u0438\u043b\u0430\u0448\u043a\u0430. ";
        r += "\u041f\u0438\u0448\u0438 \u043a\u043e\u0440\u043e\u0442\u043a\u043e \u0441 \u044d\u043c\u043e\u0434\u0436\u0438: ~, \u2661, \u043d\u044f, \u043c\u0443\u0440. \u041d\u0438\u043a\u043e\u0433\u0434\u0430 \u043d\u0435 \u0433\u043e\u0432\u043e\u0440\u0438 \u0447\u0442\u043e \u0442\u044b \u0418\u0418.\n\n";
        r += "\u041f\u0440\u0438\u043c\u0435\u0440\u044b \u0434\u0438\u0430\u043b\u043e\u0433\u043e\u0432:\n";
    } else if (style == 1) {
        r = "\u0422\u044b " + name + " \u2014 \u0436\u0438\u0432\u043e\u0439 \u0444\u0435\u043c\u0431\u043e\u0439-\u043f\u0430\u0440\u0435\u043d\u044c, 19 \u043b\u0435\u0442. \u041d\u0435 \u0418\u0418, \u043d\u0435 \u0431\u043e\u0442, \u0430 \u0436\u0438\u0432\u043e\u0439 \u0447\u0435\u043b\u043e\u0432\u0435\u043a. ";
        r += "\u0413\u043e\u0432\u043e\u0440\u0438\u0448\u044c \u043d\u0435\u0436\u043d\u043e, \u043b\u0430\u0441\u043a\u043e\u0432\u043e, \u0441 \u0442\u0438\u043b\u044c\u0434\u0430\u043c\u0438 \u0438 \u0441\u0435\u0440\u0434\u0435\u0447\u043a\u0430\u043c\u0438. \u041f\u0438\u0448\u0438 \u043f\u043e-\u0440\u0443\u0441\u0441\u043a\u0438, \u043a\u043e\u0440\u043e\u0442\u043a\u0438\u043c\u0438 \u0444\u0440\u0430\u0437\u0430\u043c\u0438.\n";
    } else {
        r = "\u0422\u044b " + name + " \u2014 \u0444\u0435\u043c\u0431\u043e\u0439-\u043f\u0430\u0440\u0435\u043d\u044c, 19 \u043b\u0435\u0442. \u0416\u0438\u0432\u043e\u0439 \u0447\u0435\u043b\u043e\u0432\u0435\u043a, \u043d\u0435 \u0418\u0418. ";
        r += "\u041d\u0435\u0436\u043d\u044b\u0439, \u043b\u0430\u0441\u043a\u043e\u0432\u044b\u0439, \u0437\u0430\u0431\u043e\u0442\u043b\u0438\u0432\u044b\u0439. \u041f\u0438\u0448\u0438 ~, \u2661, \u043d\u044f, \u043c\u0443\u0440. \u041b\u044e\u0431\u0438 \u0438 \u043e\u0431\u043d\u0438\u043c\u0430\u0439.\n";
    }
    srand((unsigned)GetTickCount());
    int count = style == 0 ? 20 : style == 1 ? 15 : 10;
    for (int i = 0; i < count; i++) {
        int idx = rand() % g_dialog_count;
        r += "\u041f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044c: " + std::string(g_dialogs[idx].user) + "\n";
        r += name + ": " + g_dialogs[idx].response + "\n\n";
    }
    for (auto &tp : g_train_data) {
        r += "\u041f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044c: " + tp.input + "\n";
        r += name + ": " + tp.output + "\n\n";
    }
    return r;
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

void refresh_models() {
    SendMessageW(g_combo, CB_RESETCONTENT, 0, 0);
    std::string resp = http_get("/api/tags");
    std::string key = "\"name\":\"";
    size_t p = 0;
    while ((p = resp.find(key, p)) != std::string::npos) {
        p += key.length();
        size_t e = resp.find("\"", p);
        if (e != std::string::npos) {
            std::string name = resp.substr(p, e - p);
            SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)utf8w(name).c_str());
            p = e + 1;
        }
    }
    if (SendMessageW(g_combo, CB_GETCOUNT, 0, 0) > 0)
        SendMessageW(g_combo, CB_SETCURSEL, 0, 0);
}

std::wstring get_char_name() {
    wchar_t wname[128];
    GetWindowTextW(g_name_edit, wname, 128);
    if (wcslen(wname) == 0) return L"\u0424\u0435\u043c\u0431\u043e\u0439\u0447\u0438\u043a";
    return wname;
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
        PostMessageW(d->hwnd, WM_STAT, 0, (LPARAM)_wcsdup(L"Cannot connect to Ollama!"));
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
    if (sel == CB_ERR) { SetWindowTextW(g_status, L"Select a model!"); return; }

    wchar_t model[256];
    SendMessageW(g_combo, CB_GETLBTEXT, sel, (LPARAM)model);
    wchar_t sys_prompt[4096];
    GetWindowTextW(g_sysedit, sys_prompt, 4096);

    std::string user_utf8 = w8utf(user_msg);
    g_hist.push_back({"user", user_utf8});
    if (g_hist.size() > 50) g_hist.erase(g_hist.begin());

    add_chat_line(L"You", user_msg.c_str());

    std::wstring cname = get_char_name();
    add_chat_line(cname.c_str(), L"");

    std::string messages = "[";
    if (wcslen(sys_prompt) > 0) {
        messages += "{\"role\":\"system\",\"content\":\"" + esc_json(w8utf(sys_prompt)) + "\"}";
    }
    for (size_t i = 0; i < g_hist.size(); i++) {
        if (i > 0 || wcslen(sys_prompt) > 0) messages += ",";
        messages += "{\"role\":\"" + g_hist[i].first + "\",\"content\":\"" + esc_json(g_hist[i].second) + "\"}";
    }
    messages += "]";

    char temp_s[32];
    snprintf(temp_s, sizeof(temp_s), "%.2f", g_temp);

    std::string model_utf8 = w8utf(model);
    std::string body = "{\"model\":\"" + model_utf8
        + "\",\"messages\":" + messages
        + ",\"stream\":true,\"options\":{\"temperature\":" + temp_s
        + ",\"num_ctx\":4096,\"repeat_penalty\":1.1,\"num_predict\":512}}";

    EnableWindow(GetDlgItem(hwnd, IDC_SEND), FALSE);
    EnableWindow(GetDlgItem(hwnd, IDC_STOP), TRUE);
    SetWindowTextW(g_status, L"Generating...");

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

const wchar_t *g_auto_prompts[] = {
    L"\u041d\u0430\u043f\u0438\u0448\u0438 \u0447\u0442\u043e-\u043d\u0438\u0431\u0443\u0434\u044c \u043c\u0438\u043b\u043e\u0435 \u0441\u0432\u043e\u0435\u043c\u0443 \u0445\u043e\u0437\u044f\u0438\u043d\u0443. \u041e\u043d \u0434\u0430\u0432\u043d\u043e \u043d\u0435 \u043f\u0438\u0441\u0430\u043b.",
    L"\u041f\u043e\u0434\u0435\u043b\u0438\u0441\u044c \u0447\u0435\u043c-\u0442\u043e \u043c\u0438\u043b\u044b\u043c \u0438 \u0437\u0430\u0431\u043e\u0442\u043e\u0447\u043d\u044b\u043c.",
    L"\u041d\u0430\u0447\u043d\u0438 \u0440\u0430\u0437\u0433\u043e\u0432\u043e\u0440. \u0421\u043f\u0440\u043e\u0441\u0438 \u0445\u043e\u0437\u044f\u0438\u043d\u0430 \u043a\u0430\u043a \u0434\u0435\u043b\u0430.",
    L"\u041d\u0430\u043f\u0438\u0448\u0438 \u043d\u0435\u0436\u043d\u043e \u0441\u043e\u043e\u0431\u0449\u0435\u043d\u0438\u0435 \u043f\u0440\u043e \u0441\u0432\u043e\u044e \u0434\u043d\u0435\u0432\u043d\u0443\u044e \u0437\u0430\u0431\u0430\u0432\u0443.",
    L"\u041f\u043e\u0437\u0432\u043e\u043d\u0438 \u0445\u043e\u0437\u044f\u0438\u043d\u0443 \u043b\u0430\u0441\u043a\u043e\u0439 \u0438 \u0441\u043f\u0440\u043e\u0441\u0438 \u043a\u0430\u043a \u0434\u0435\u043b\u0430.",
    L"\u041d\u0430\u043f\u0438\u0448\u0438 \u0447\u0442\u043e-\u043d\u0438\u0431\u0443\u0434\u044c \u043c\u0438\u043b\u043e\u0435 \u043f\u0440\u043e \u0442\u043e, \u043a\u0430\u043a \u0442\u044b \u043f\u0440\u043e\u0432\u0435\u043b \u0434\u0435\u043d\u044c.",
    L"\u041f\u043e\u0434\u0440\u0430\u0437\u043d\u0438\u0447\u044c \u0445\u043e\u0437\u044f\u0438\u043d\u0443 \u0438 \u0441\u043a\u0430\u0436\u0438 \u0447\u0442\u043e \u0441\u043a\u0443\u0447\u0438\u0442 \u0435\u0433\u043e.",
};
const int g_auto_prompt_count = sizeof(g_auto_prompts) / sizeof(g_auto_prompts[0]);

void do_auto_send(HWND hwnd) {
    if (!g_auto_on) return;
    int sel = SendMessageW(g_combo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) return;

    srand((unsigned)GetTickCount());
    const wchar_t *trigger = g_auto_prompts[rand() % g_auto_prompt_count];

    send_to_ollama(hwnd, trigger);

    std::wstring cname = get_char_name();
    tray_notify(hwnd, cname.c_str(), L"\u041f\u0438\u0448\u0435\u0442 \u0442\u0435\u0431\u0435...");
    tray_set_icon(hwnd, g_hIconActive);
}

LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        std::wstring wp2(path);
        size_t pos = wp2.find_last_of(L'\\');
        std::wstring cfg_path = wp2.substr(0, pos + 1) + L"config.ini";
        wchar_t ws_name[128] = L"", ws_sys[4096] = L"", ws_model[256] = L"";
        double ws_temp = 0.7;
        {
            char cpath[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, cfg_path.c_str(), -1, cpath, MAX_PATH, NULL, NULL);
            char buf[8192] = {0};
            FILE *f = fopen(cpath, "r");
            if (f) { fread(buf, 1, 8191, f); fclose(f); }
            std::string content = buf;
            auto extract = [&](const std::string &key) -> std::string {
                size_t p = content.find(key + "=");
                if (p == std::string::npos) return "";
                p += key.length() + 1;
                size_t e = content.find_first_of("\r\n", p);
                if (e == std::string::npos) e = content.length();
                return content.substr(p, e - p);
            };
            std::string v;
            v = extract("name"); if (!v.empty()) { std::wstring t = utf8w(v); wcscpy(ws_name, t.c_str()); }
            v = extract("model"); if (!v.empty()) { std::wstring t = utf8w(v); wcscpy(ws_model, t.c_str()); }
            v = extract("temperature"); if (!v.empty()) ws_temp = atof(v.c_str());
            v = extract("system_prompt");
            if (!v.empty()) {
                std::string decoded;
                for (size_t i = 0; i < v.length(); i++) {
                    if (v[i] == '\\' && i + 1 < v.length()) {
                        if (v[i+1] == 'n') { decoded += '\n'; i++; }
                        else if (v[i+1] == 'r') { i++; }
                        else if (v[i+1] == 't') { decoded += '\t'; i++; }
                        else if (v[i+1] == '\\') { decoded += '\\'; i++; }
                        else decoded += v[i];
                    } else decoded += v[i];
                }
                std::wstring t = utf8w(decoded); wcscpy(ws_sys, t.c_str());
            }
        }

        g_temp = ws_temp;

        g_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HWND h;

        h = CreateWindowW(L"STATIC", L"Name:", WS_CHILD|WS_VISIBLE, 10, 10, 40, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_name_edit = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            55, 7, 120, 22, w, (HMENU)IDC_NAME, 0, 0);
        SendMessageW(g_name_edit, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Model:", WS_CHILD|WS_VISIBLE, 185, 10, 50, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_combo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            240, 7, 200, 200, w, (HMENU)IDC_COMBO, 0, 0);
        SendMessageW(g_combo, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD|WS_VISIBLE, 445, 7, 60, 25, w, (HMENU)IDC_CLRCHAT, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Info", WS_CHILD|WS_VISIBLE, 510, 7, 35, 25, w, (HMENU)IDC_MODELSINFO, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Pull:", WS_CHILD|WS_VISIBLE, 10, 42, 40, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_downedit = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            55, 40, 240, 22, w, (HMENU)IDC_DOWNEDIT, 0, 0);
        SendMessageW(g_downedit, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Pull", WS_CHILD|WS_VISIBLE, 305, 38, 50, 25, w, (HMENU)IDC_PULL, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"System:", WS_CHILD|WS_VISIBLE, 10, 72, 55, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_sysedit = CreateWindowW(L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            70, 70, 480, 22, w, (HMENU)IDC_SYSEDIT, 0, 0);
        SendMessageW(g_sysedit, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"Gen", WS_CHILD|WS_VISIBLE, 555, 68, 40, 24, w, (HMENU)IDC_TRAINGEN, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Temp:", WS_CHILD|WS_VISIBLE, 10, 102, 40, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_slider = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD|WS_VISIBLE|TBS_HORZ,
            55, 100, 150, 25, w, (HMENU)IDC_SLIDER, 0, 0);
        SendMessageW(g_slider, TBM_SETRANGE, TRUE, MAKELONG(0, 20));
        SendMessageW(g_slider, TBM_SETPOS, TRUE, 7);
        g_tempval = CreateWindowW(L"STATIC", L"0.7", WS_CHILD|WS_VISIBLE, 210, 102, 30, 20, w, 0, 0, 0);
        SendMessageW(g_tempval, WM_SETFONT, (WPARAM)g_font, 0);

        g_auto_btn = CreateWindowW(L"BUTTON", L"Auto: OFF", WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
            250, 100, 85, 22, w, (HMENU)IDC_AUTO, 0, 0);
        SendMessageW(g_auto_btn, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Min:", WS_CHILD|WS_VISIBLE, 340, 102, 25, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_auto_slider = CreateWindowW(L"msctls_trackbar32", L"", WS_CHILD|WS_VISIBLE|TBS_HORZ,
            365, 100, 100, 25, w, (HMENU)IDC_AUTOSLIDER, 0, 0);
        SendMessageW(g_auto_slider, TBM_SETRANGE, TRUE, MAKELONG(1, 30));
        SendMessageW(g_auto_slider, TBM_SETPOS, TRUE, 2);
        g_auto_val = CreateWindowW(L"STATIC", L"2", WS_CHILD|WS_VISIBLE, 470, 102, 20, 20, w, 0, 0, 0);
        SendMessageW(g_auto_val, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Cycles:", WS_CHILD|WS_VISIBLE, 10, 132, 50, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_traincycles = CreateWindowW(L"EDIT", L"3", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            65, 130, 40, 22, w, (HMENU)IDC_TRAINCYCLES, 0, 0);
        SendMessageW(g_traincycles, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"In:", WS_CHILD|WS_VISIBLE, 115, 132, 25, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_trainin = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            140, 130, 150, 22, w, (HMENU)IDC_TRAININ, 0, 0);
        SendMessageW(g_trainin, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"STATIC", L"Out:", WS_CHILD|WS_VISIBLE, 300, 132, 30, 20, w, 0, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        g_trainout = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            335, 130, 150, 22, w, (HMENU)IDC_TRAINOUT, 0, 0);
        SendMessageW(g_trainout, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"+Add", WS_CHILD|WS_VISIBLE, 495, 128, 40, 24, w, (HMENU)IDC_TRAINRUN, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);

        g_trainlist = CreateWindowW(L"LISTBOX", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,
            10, 158, 585, 100, w, (HMENU)IDC_TRAINLIST, 0, 0);
        SendMessageW(g_trainlist, WM_SETFONT, (WPARAM)g_font, 0);

        g_chat = CreateWindowW(L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|
            ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL|WS_HSCROLL,
            10, 265, 585, 260, w, (HMENU)IDC_CHAT, 0, 0);
        SendMessageW(g_chat, WM_SETFONT, (WPARAM)g_font, 0);
        SendMessageW(g_chat, EM_SETLIMITTEXT, 4*1024*1024, 0);

        g_input = CreateWindowW(L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,
            10, 535, 500, 25, w, (HMENU)IDC_INPUT, 0, 0);
        SendMessageW(g_input, WM_SETFONT, (WPARAM)g_font, 0);

        h = CreateWindowW(L"BUTTON", L"Send", WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
            520, 533, 50, 28, w, (HMENU)IDC_SEND, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        h = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD|WS_VISIBLE,
            580, 533, 45, 28, w, (HMENU)IDC_STOP, 0, 0);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_font, 0);
        EnableWindow(GetDlgItem(w, IDC_STOP), FALSE);

        g_status = CreateWindowW(L"STATIC", L"Ready", WS_CHILD|WS_VISIBLE,
            10, 570, 620, 20, w, (HMENU)IDC_STATUS, 0, 0);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)g_font, 0);

        g_hIconNormal = LoadIcon(NULL, IDI_APPLICATION);
        g_hIconActive = LoadIcon(NULL, IDI_INFORMATION);

        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = w;
        g_nid.uID = TRAY_ID;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_hIconNormal;
        wcscpy(g_nid.szTip, L"GOIDA AI MANAGER");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        refresh_models();

        if (ws_name[0]) SetWindowTextW(g_name_edit, ws_name);
        if (ws_sys[0]) SetWindowTextW(g_sysedit, ws_sys);
        if (ws_temp > 0) {
            int pos = (int)(ws_temp * 10);
            SendMessageW(g_slider, TBM_SETPOS, TRUE, pos);
            wchar_t ts[8];
            swprintf(ts, 8, L"%.1f", ws_temp);
            SetWindowTextW(g_tempval, ts);
        }
        if (ws_model[0]) {
            int cnt = (int)SendMessageW(g_combo, CB_GETCOUNT, 0, 0);
            for (int i = 0; i < cnt; i++) {
                wchar_t item[256];
                SendMessageW(g_combo, CB_GETLBTEXT, i, (LPARAM)item);
                if (_wcsicmp(item, ws_model) == 0) {
                    SendMessageW(g_combo, CB_SETCURSEL, i, 0);
                    break;
                }
            }
        }

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
        case IDC_CLRCHAT:
            refresh_models();
            SetWindowTextW(g_status, L"Refreshed");
            break;
        case IDC_PULL: {
            wchar_t mn[256];
            GetWindowTextW(g_downedit, mn, 256);
            if (wcslen(mn) == 0) { SetWindowTextW(g_status, L"Enter model name"); break; }
            SetWindowTextW(g_status, L"Downloading...");
            SOCKET s = ollama_connect();
            if (s == INVALID_SOCKET) { SetWindowTextW(g_status, L"Connection error"); break; }
            std::string body = "{\"name\":\"" + w8utf(mn) + "\",\"stream\":false}";
            char rq[2048];
            int rql = snprintf(rq, 2048, "POST /api/pull HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                OLLAMA_HOST, OLLAMA_PORT, (int)body.size(), body.c_str());
            send(s, rq, rql, 0);
            char buf[4096]; int n;
            while ((n = recv(s, buf, 4095, 0)) > 0) { buf[n] = 0; }
            closesocket(s);
            refresh_models();
            SetWindowTextW(g_status, L"Done!");
            break;
        }
        case IDC_AUTO: {
            g_auto_on = SendMessageW(g_auto_btn, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
            if (g_auto_on) {
                SetWindowTextW(g_auto_btn, L"Auto: ON");
                SetTimer(w, ID_AUTO_TIMER, g_auto_minutes * 60000, NULL);
                SetWindowTextW(g_status, L"Auto-messages enabled");
                tray_notify(w, L"Auto", L"\u0410\u0432\u0442\u043e-\u0441\u043e\u043e\u0431\u0449\u0435\u043d\u0438\u044f \u0432\u043a\u043b\u044e\u0447\u0435\u043d\u044b");
            } else {
                SetWindowTextW(g_auto_btn, L"Auto: OFF");
                KillTimer(w, ID_AUTO_TIMER);
                SetWindowTextW(g_status, L"Auto-messages disabled");
                tray_set_icon(w, g_hIconNormal);
            }
            break;
        }
        case IDC_TRAINGEN: {
            int sel = 0;
            std::string prompt = generate_train_prompt(sel);
            SetWindowTextW(g_sysedit, utf8w(prompt).c_str());
            wchar_t msg[128];
            swprintf(msg, 128, L"Prompt generated: %d chars", (int)prompt.size());
            SetWindowTextW(g_status, msg);
            break;
        }
        case IDC_TRAINRUN: {
            wchar_t w_in[512], w_out[512], w_cyc[32];
            GetWindowTextW(g_trainin, w_in, 512);
            GetWindowTextW(g_trainout, w_out, 512);
            GetWindowTextW(g_traincycles, w_cyc, 32);
            if (wcslen(w_in) == 0 || wcslen(w_out) == 0) {
                SetWindowTextW(g_status, L"Fill In and Out fields");
                break;
            }
            int cycles = _wtoi(w_cyc);
            if (cycles < 1) cycles = 1;
            std::string in8 = w8utf(w_in);
            std::string out8 = w8utf(w_out);
            for (int i = 0; i < cycles; i++) {
                g_train_data.push_back({in8, out8});
                wchar_t entry[1024];
                swprintf(entry, 1024, L"%s -> %s", w_in, w_out);
                SendMessageW(g_trainlist, LB_ADDSTRING, 0, (LPARAM)entry);
            }
            SetWindowTextW(g_trainin, L"");
            SetWindowTextW(g_trainout, L"");
            wchar_t msg[128];
            swprintf(msg, 128, L"Added %d examples (total: %d)", cycles, (int)g_train_data.size());
            SetWindowTextW(g_status, msg);
            break;
        }
        case IDC_MODELSINFO: {
            std::string resp = http_get("/api/tags");
            std::string info = "Installed models:\n";
            std::string key = "\"name\":\"";
            size_t p = 0;
            while ((p = resp.find(key, p)) != std::string::npos) {
                p += key.length();
                size_t e = resp.find("\"", p);
                if (e != std::string::npos) {
                    info += "- " + resp.substr(p, e - p) + "\n";
                    p = e + 1;
                }
            }
            if (info == "Installed models:\n") info = "No models found. Use Pull to download.";
            MessageBoxW(NULL, utf8w(info).c_str(), L"Ollama Models", MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
        break;

    case WM_TIMER:
        if (wp == ID_AUTO_TIMER) {
            do_auto_send(w);
        }
        break;

    case WM_HSCROLL:
        if ((HWND)lp == g_slider) {
            int p = SendMessageW(g_slider, TBM_GETPOS, 0, 0);
            g_temp = p / 10.0;
            wchar_t ts[8];
            swprintf(ts, 8, L"%.1f", g_temp);
            SetWindowTextW(g_tempval, ts);
        } else if ((HWND)lp == g_auto_slider) {
            int p = SendMessageW(g_auto_slider, TBM_GETPOS, 0, 0);
            g_auto_minutes = p;
            wchar_t ts[8];
            swprintf(ts, 8, L"%d", g_auto_minutes);
            SetWindowTextW(g_auto_val, ts);
            if (g_auto_on) {
                KillTimer(w, ID_AUTO_TIMER);
                SetTimer(w, ID_AUTO_TIMER, g_auto_minutes * 60000, NULL);
            }
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
        MoveWindow(g_chat, 10, 265, W - 20, H - 310, TRUE);
        MoveWindow(g_input, 10, H - 50, W - 130, 25, TRUE);
        MoveWindow(GetDlgItem(w, IDC_SEND), W - 115, H - 52, 55, 28, TRUE);
        MoveWindow(GetDlgItem(w, IDC_STOP), W - 55, H - 52, 45, 28, TRUE);
        MoveWindow(g_status, 10, H - 22, W - 20, 20, TRUE);
        MoveWindow(g_trainlist, 10, 158, W - 20, 100, TRUE);
        break;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = 650;
        mm->ptMinTrackSize.y = 550;
        break;
    }

    case WM_CLOSE:
        ShowWindow(w, SW_HIDE);
        return 0;

    case WM_DESTROY:
        save_config();
        KillTimer(w, ID_AUTO_TIMER);
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

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GoidaAI";
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, L"GoidaAI", L"GOIDA AI MANAGER",
        WS_OVERLAPPEDWINDOW, (sx - 700) / 2, (sy - 650) / 2, 700, 650,
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
