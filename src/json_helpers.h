// GOIDA json helpers — вынесено из goida.cpp (JEsc/JStr)
#pragma once
#include <string>

namespace goida::json {

inline std::wstring JEsc(const std::wstring& s) {
    std::wstring r; r.reserve(s.size()+8);
    for (wchar_t c : s) {
        if (c == L'"') r += L"\\\""; else if (c == L'\\') r += L"\\\\";
        else if (c == L'\n') r += L"\\n"; else if (c == L'\r') r += L"\\r";
        else if (c == L'\t') r += L"\\t"; else r += c;
    }
    return r;
}
inline std::wstring JStr(const std::wstring& j, const std::wstring& k) {
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

} // namespace goida::json
