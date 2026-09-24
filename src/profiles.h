// GOIDA profiles — вынесено из goida.cpp 2439 строк (этап1 рефактор)
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "sqlite3.h"
#include "utf8.h"
#include "raii.h"

namespace goida::profiles {
struct Profile {
    std::wstring name;
    std::wstring avatar;
    std::wstring system_prompt;
    float temp = 0.7f;
    int max_tokens = 4096;
    std::wstring keep_alive = L"5m"; // L1=5m L2=30m L3=1h/0
};
} // namespace goida::profiles

// forward for DB helpers (defined in goida.cpp)
sqlite3_stmt* DBPrep(const char* sql);
std::wstring DBGet(const wchar_t* key, const wchar_t* def);
void DBSet(const wchar_t* key, const wchar_t* val);

namespace goida::profiles {

inline bool EnsureDefaultProfile(sqlite3* db, const std::wstring& f_name, const std::wstring& sys_prompt, float temp, int max_tokens);
inline std::vector<Profile> LoadAllProfiles(sqlite3* db);
inline bool SaveProfile(sqlite3* db, const Profile& p);
inline bool DeleteProfile(sqlite3* db, const std::wstring& name);
inline int MemoryCount(sqlite3* db);
inline std::wstring KeepAliveFromLevel(int lvl);
inline int LevelFromKeepAlive(const std::wstring& ka);
inline std::wstring GetActiveKeepAlive(sqlite3* db);

} // namespace goida::profiles

// ---- inline impl ----
#include <string>

namespace goida::profiles {

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
inline int MemoryCount(sqlite3* db) {
    (void)db;
    goida::raii::Stmt s(DBPrep("SELECT COUNT(*) FROM memory"));
    if (!s) return 0;
    if (sqlite3_step(s.get())==SQLITE_ROW) return sqlite3_column_int(s.get(),0);
    return 0;
}
inline std::vector<Profile> LoadAllProfiles(sqlite3* db) {
    (void)db;
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
inline bool SaveProfile(sqlite3* db, const Profile& p) {
    (void)db;
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
inline bool DeleteProfile(sqlite3* db, const std::wstring& name) {
    (void)db;
    goida::raii::Stmt s(DBPrep("DELETE FROM profiles WHERE name=?1"));
    if (!s) return false;
    std::string n = goida::utf8::w2utf8(name);
    sqlite3_bind_text(s.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    return sqlite3_step(s.get())==SQLITE_DONE;
}
inline std::wstring GetActiveKeepAlive(sqlite3* db) {
    (void)db;
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

inline bool EnsureDefaultProfile(sqlite3* db, const std::wstring& f_name, const std::wstring& sys_prompt_raw, float temp, int max_tokens) {
    (void)db;
    goida::raii::Stmt cnt(DBPrep("SELECT COUNT(*) FROM profiles"));
    if (!cnt) return false;
    int c=0;
    if (sqlite3_step(cnt.get())==SQLITE_ROW) c=sqlite3_column_int(cnt.get(),0);
    if (c>0) return true;
    std::wstring sys = sys_prompt_raw;
    if (sys.empty()) sys = L"You are a helpful AI assistant.";
    // Embed femboy personality for migration (keep compat)
    std::wstring fem = L"Name: " + f_name + L", Voice: soft, Eyes: round";
    std::wstring merged = sys + L" [" + fem + L"]";
    goida::raii::Stmt ins(DBPrep("INSERT INTO profiles(name, avatar, system_prompt, temperature, max_tokens, keep_alive) VALUES(?1,?2,?3,?4,?5,?6)"));
    if (!ins) return false;
    std::string n = goida::utf8::w2utf8(f_name.empty()? L"default" : f_name);
    std::string av = goida::utf8::w2utf8(L"");
    std::string sp = goida::utf8::w2utf8(merged);
    std::string ka = "5m";
    sqlite3_bind_text(ins.get(),1,n.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins.get(),2,av.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins.get(),3,sp.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_double(ins.get(),4,temp);
    sqlite3_bind_int(ins.get(),5,max_tokens);
    sqlite3_bind_text(ins.get(),6,ka.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_step(ins.get());
    DBSet(L"active_profile", f_name.empty()? L"default" : f_name.c_str());
    return true;
}

} // namespace goida::profiles
