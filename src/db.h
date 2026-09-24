// GOIDA database — SQLite access layer extracted from goida.cpp
#pragma once
#include <string>
#include "sqlite3.h"
#include "raii.h"
#include "utf8.h"
#include "db_migration.h"

extern sqlite3* g_db;

inline bool DBExec(const wchar_t* sql) {
    if (!g_db || !sql) return false;
    char buf[4096];
    int n = WideCharToMultiByte(CP_UTF8, 0, sql, -1, buf, sizeof(buf), NULL, NULL);
    if (n <= 0) return false;
    char* err = NULL;
    if (sqlite3_exec(g_db, buf, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

inline bool DBExec(const char* sql) {
    if (!g_db || !sql) return false;
    char* err = NULL;
    if (sqlite3_exec(g_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

inline sqlite3_stmt* DBPrep(const char* sql) {
    sqlite3_stmt* stmt = NULL;
    if (!g_db || !sql || sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    return stmt;
}

inline std::wstring DBGet(const wchar_t* key, const wchar_t* def = L"") {
    goida::raii::Stmt st(DBPrep("SELECT value FROM config WHERE key=?1"));
    if (!st) return def;
    std::string k = goida::utf8::w2utf8(key);
    sqlite3_bind_text(st.get(), 1, k.c_str(), -1, SQLITE_TRANSIENT);
    std::wstring result = def;
    if (sqlite3_step(st.get()) == SQLITE_ROW) {
        const char* value = reinterpret_cast<const char*>(sqlite3_column_text(st.get(), 0));
        if (value) result = goida::utf8::utf8_to_w(value);
    }
    return result;
}

inline void DBSet(const wchar_t* key, const wchar_t* val) {
    goida::raii::Stmt st(DBPrep("INSERT OR REPLACE INTO config(key,value) VALUES(?1,?2)"));
    if (!st) return;
    std::string k = goida::utf8::w2utf8(key);
    std::string v = goida::utf8::w2utf8(val);
    sqlite3_bind_text(st.get(), 1, k.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.get(), 2, v.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st.get());
}

inline void DBLog(const wchar_t* src, const wchar_t* msg) {
    goida::raii::Stmt st(DBPrep("INSERT INTO logs(source,message) VALUES(?1,?2)"));
    if (!st) return;
    std::string source = goida::utf8::w2utf8(src);
    std::string message = goida::utf8::w2utf8(msg);
    sqlite3_bind_text(st.get(), 1, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.get(), 2, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(st.get());
}

inline void DBInit() {
    CreateDirectoryW(L"global", NULL);
    std::string dbPath = goida::utf8::w2utf8(L"global/goida.db");
    sqlite3_open(dbPath.c_str(), &g_db);
    goida::db::migrate(g_db);
}
