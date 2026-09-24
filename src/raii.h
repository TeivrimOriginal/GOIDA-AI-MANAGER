// GOIDA RAII helpers — handles, WinHTTP, SQLite
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <memory>
#include <functional>
#include "sqlite3.h"

namespace goida::raii {

// Generic handle wrapper
template<typename H, typename Closer>
class Handle {
    H h_{};
    Closer closer_{};
public:
    Handle() = default;
    explicit Handle(H h): h_(h) {}
    Handle(H h, Closer c): h_(h), closer_(c) {}
    ~Handle() { reset(); }
    Handle(Handle&& o) noexcept : h_(o.h_), closer_(std::move(o.closer_)) { o.h_={}; }
    Handle& operator=(Handle&& o) noexcept { if(this!=&o){reset(); h_=o.h_; closer_=std::move(o.closer_); o.h_={};} return *this; }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    H get() const { return h_; }
    H* ptr() { return &h_; }
    explicit operator bool() const { return h_ && h_!=INVALID_HANDLE_VALUE; }
    void reset(H nh={}) { if(h_ && h_!=INVALID_HANDLE_VALUE) closer_(h_); h_=nh; }
    H release(){ H t=h_; h_={}; return t; }
};

struct WinHttpCloser { void operator()(HINTERNET h) const { if(h) WinHttpCloseHandle(h); } };
using WinHttpHandle = Handle<HINTERNET, WinHttpCloser>;
struct HandleCloser { void operator()(HANDLE h) const { if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h); } };
using FileHandle = Handle<HANDLE, HandleCloser>;
struct RegCloser { void operator()(HKEY h) const { if(h) RegCloseKey(h); } };
using RegHandle = Handle<HKEY, RegCloser>;

// SQLite statement RAII
class Stmt {
    sqlite3_stmt* s_=nullptr;
public:
    Stmt()=default;
    explicit Stmt(sqlite3_stmt* s): s_(s) {}
    ~Stmt(){ if(s_) sqlite3_finalize(s_); }
    Stmt(Stmt&& o) noexcept : s_(o.s_) { o.s_=nullptr; }
    Stmt& operator=(Stmt&& o) noexcept { if(s_) sqlite3_finalize(s_); s_=o.s_; o.s_=nullptr; return *this; }
    Stmt(const Stmt&)=delete; Stmt& operator=(const Stmt&)=delete;
    sqlite3_stmt* get() const { return s_; }
    sqlite3_stmt* operator->() const { return s_; }
    explicit operator bool() const { return s_!=nullptr; }
    void reset(sqlite3_stmt* ns=nullptr){ if(s_) sqlite3_finalize(s_); s_=ns; }
    sqlite3_stmt* release(){ auto t=s_; s_=nullptr; return t; }
};

// Scoped GDI
template<typename T, typename D>
class GdiHandle {
    T h_;
public:
    explicit GdiHandle(T h): h_(h) {}
    ~GdiHandle(){ if(h_) DeleteObject(h_); }
    GdiHandle(GdiHandle&& o) noexcept: h_(o.h_) { o.h_=nullptr; }
    GdiHandle(const GdiHandle&)=delete; GdiHandle& operator=(const GdiHandle&)=delete;
    T get() const { return h_; }
    operator T() const { return h_; }
};

} // namespace goida::raii
