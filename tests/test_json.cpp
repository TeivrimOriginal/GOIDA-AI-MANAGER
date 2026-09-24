#include <cassert>
#include <string>
#include <iostream>
// Test JSON helpers from goida.cpp logic replicated
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
int main(){
    std::wstring s = L"hello \"world\"\n";
    std::wstring esc = JEsc(s);
    assert(esc.find(L"\\\"")!=std::wstring::npos);
    assert(esc.find(L"\\n")!=std::wstring::npos);
    std::wstring json = L"{\"response\":\"hi\\nthere\",\"done\":true}";
    auto r = JStr(json, L"response");
    assert(r==L"hi\nthere");
    std::wstring empty = JStr(json, L"missing");
    assert(empty.empty());
    std::cout << "json ok\n";
    return 0;
}
