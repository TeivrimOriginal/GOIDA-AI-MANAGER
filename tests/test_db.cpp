#include "../src/db_migration.h"
#include "../src/sqlite3.h"
#include <cassert>
#include <iostream>
#include <cstdio>

int main(){
    const char* path = "test_goida_db_tmp.db";
    std::remove(path);
    sqlite3* db=nullptr;
    int rc = sqlite3_open(path, &db);
    assert(rc==SQLITE_OK && db);
    bool ok = goida::db::migrate(db);
    assert(ok);
    int ver = goida::db::user_version(db);
    assert(ver>=1);
    // check tables exist
    auto hasTable = [&](const char* name)->bool{
        sqlite3_stmt* s=nullptr;
        char sql[256]; snprintf(sql,sizeof(sql),"SELECT name FROM sqlite_master WHERE type='table' AND name='%s'", name);
        sqlite3_prepare_v2(db, sql, -1, &s, nullptr);
        bool has = (sqlite3_step(s)==SQLITE_ROW);
        sqlite3_finalize(s);
        return has;
    };
    assert(hasTable("config"));
    assert(hasTable("chat_messages"));
    assert(hasTable("profiles"));
    assert(hasTable("marketplace_cache"));
    assert(hasTable("chat_branches"));
    // run migrate again idempotent
    assert(goida::db::migrate(db));
    sqlite3_close(db);
    std::remove(path);
    std::cout << "db migration ok ver=" << ver << "\n";
    return 0;
}
