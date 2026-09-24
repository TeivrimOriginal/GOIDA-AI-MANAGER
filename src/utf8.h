// GOIDA UTF-8 helpers — centralizes Wide<->UTF8 conversions, RAII-safe
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace goida::utf8 {

inline std::string w2utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string r; r.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), r.data(), n, nullptr, nullptr);
    return r;
}
inline std::string w2utf8(const wchar_t* w) {
    if (!w || !*w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string r; r.resize(n-1);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, r.data(), n, nullptr, nullptr);
    return r;
}
inline std::wstring utf8_to_w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring r; r.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), r.data(), n);
    return r;
}
inline std::wstring utf8_to_w(const char* s, size_t len) {
    if (!s || len==0) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, (int)len, nullptr, 0);
    if (n <= 0) return {};
    std::wstring r; r.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s, (int)len, r.data(), n);
    return r;
}
// File I/O helpers: read UTF-8 file with BOM stripping
inline std::string read_file_utf8(const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD sz = GetFileSize(h, nullptr);
    if (sz==INVALID_FILE_SIZE || sz==0) { CloseHandle(h); return {}; }
    std::string buf; buf.resize(sz);
    DWORD read=0;
    ReadFile(h, buf.data(), sz, &read, nullptr);
    CloseHandle(h);
    buf.resize(read);
    // strip BOM
    if (buf.size()>=3 && (unsigned char)buf[0]==0xEF && (unsigned char)buf[1]==0xBB && (unsigned char)buf[2]==0xBF)
        buf.erase(0,3);
    return buf;
}
} // namespace goida::utf8
