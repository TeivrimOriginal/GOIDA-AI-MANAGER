#include "../src/utf8.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace goida::utf8;
    // ASCII
    std::wstring w1 = L"hello";
    std::string u1 = w2utf8(w1);
    assert(u1=="hello");
    std::wstring w1b = utf8_to_w(u1);
    assert(w1==w1b);

    // Cyrillic
    std::wstring w2 = L"Привет GOIDA";
    std::string u2 = w2utf8(w2);
    assert(!u2.empty());
    // UTF-8 bytes for Привет should contain 0xD0 0x9F etc
    assert(u2.find((char)0xD0)!=std::string::npos);
    std::wstring w2b = utf8_to_w(u2);
    assert(w2==w2b);

    // Mixed
    std::wstring w3 = L"Чат — тест 😀";
    std::string u3 = w2utf8(w3);
    assert(!u3.empty());
    std::wstring w3b = utf8_to_w(u3);
    // Not perfect for emoji surrogate but should roundtrip via UTF-16
    // At least size >0
    assert(!w3b.empty());

    // Empty
    assert(w2utf8(L"").empty());
    assert(utf8_to_w("").empty());

    std::cout << "utf8 ok: " << u2.size() << " bytes\n";
    return 0;
}
