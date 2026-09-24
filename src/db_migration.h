// GOIDA DB migration — versioned schema, WAL, FK
#pragma once
#include "sqlite3.h"
#include <string>

namespace goida::db {

inline bool exec(sqlite3* db, const char* sql) {
    char* err=nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc!=SQLITE_OK) { if(err) sqlite3_free(err); return false; }
    return true;
}

inline int user_version(sqlite3* db) {
    sqlite3_stmt* s=nullptr;
    if (sqlite3_prepare_v2(db,"PRAGMA user_version",-1,&s,nullptr)!=SQLITE_OK) return 0;
    int v=0;
    if (sqlite3_step(s)==SQLITE_ROW) v=sqlite3_column_int(s,0);
    sqlite3_finalize(s);
    return v;
}
inline bool set_user_version(sqlite3* db, int v) {
    char sql[64]; snprintf(sql,sizeof(sql),"PRAGMA user_version=%d",v);
    return exec(db, sql);
}

// Migrations list — incrementally applied
inline bool migrate(sqlite3* db) {
    if (!db) return false;
    exec(db,"PRAGMA journal_mode=WAL");
    exec(db,"PRAGMA foreign_keys=ON");
    int ver = user_version(db);
    if (ver < 1) {
        exec(db,"CREATE TABLE IF NOT EXISTS config (key TEXT PRIMARY KEY, value TEXT)");
        exec(db,"CREATE TABLE IF NOT EXISTS chat_messages (id INTEGER PRIMARY KEY AUTOINCREMENT, role TEXT NOT NULL, content TEXT NOT NULL, created_at TEXT DEFAULT (datetime('now','localtime')))");
        exec(db,"CREATE TABLE IF NOT EXISTS prompts (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT DEFAULT '', system_prompt TEXT DEFAULT '', prompt TEXT NOT NULL, output TEXT DEFAULT '', temp REAL DEFAULT 0.7, max_tokens INTEGER DEFAULT 4096, created_at TEXT DEFAULT (datetime('now','localtime')))");
        exec(db,"CREATE TABLE IF NOT EXISTS memory (id INTEGER PRIMARY KEY AUTOINCREMENT, tag TEXT NOT NULL, value TEXT NOT NULL, created_at TEXT DEFAULT (datetime('now','localtime')))");
        exec(db,"CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, source TEXT NOT NULL, message TEXT NOT NULL, created_at TEXT DEFAULT (datetime('now','localtime')))");
        exec(db,"CREATE TABLE IF NOT EXISTS femboy (key TEXT PRIMARY KEY, value TEXT)");
        // v1 adds profiles + keep_alive
        exec(db,"CREATE TABLE IF NOT EXISTS profiles (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, avatar TEXT DEFAULT '', system_prompt TEXT DEFAULT '', temperature REAL DEFAULT 0.7, max_tokens INTEGER DEFAULT 4096, keep_alive TEXT DEFAULT '5m', created_at TEXT DEFAULT (datetime('now','localtime')), updated_at TEXT DEFAULT (datetime('now','localtime')))");
        exec(db,"CREATE INDEX IF NOT EXISTS idx_profiles_name ON profiles(name)");
        // memory: add parent_id for hierarchy later, keep_alive level
        exec(db,"CREATE TABLE IF NOT EXISTS _mig_tmp_check (x INT)");
        exec(db,"DROP TABLE _mig_tmp_check");
        set_user_version(db,1);
        ver=1;
    }
    if (ver < 2) {
        // v2: marketplace cache, model metadata
        exec(db,"CREATE TABLE IF NOT EXISTS marketplace_cache (id INTEGER PRIMARY KEY, name TEXT UNIQUE, size INTEGER, modified TEXT, tags TEXT, pulled INTEGER DEFAULT 0, json TEXT)");
        exec(db,"CREATE TABLE IF NOT EXISTS model_progress (model TEXT PRIMARY KEY, total INTEGER, completed INTEGER, status TEXT, updated_at TEXT DEFAULT (datetime('now','localtime')))");
        set_user_version(db,2);
        ver=2;
    }
    if (ver < 3) {
        // v3: chat branches, regenerate
        exec(db,"CREATE TABLE IF NOT EXISTS chat_branches (id INTEGER PRIMARY KEY AUTOINCREMENT, parent_id INTEGER, role TEXT, content TEXT, created_at TEXT DEFAULT (datetime('now','localtime')), FOREIGN KEY(parent_id) REFERENCES chat_messages(id) ON DELETE CASCADE)");
        // ensure chat_messages has branch support
        set_user_version(db,3);
        ver=3;
    }
    return true;
}

} // namespace goida::db
