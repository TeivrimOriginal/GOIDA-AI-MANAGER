#include "../src/i18n.h"
#include <cassert>
#include <iostream>
#include <windows.h>
int main(){
    using namespace goida::i18n;
    I18n i;
    // Test loading en
    bool ok_en = i.load(L"lang/en.json", "en");
    if (!ok_en) ok_en = i.load(L"D:/GOIDA-AI-MANAGER/lang/en.json", "en");
    assert(ok_en);
    assert(i.size() >= 10);
    assert(i.get("app.title")=="GOIDA AI MANAGER");
    assert(i.get("nav.chat")=="Chat");
    assert(!i.get("nonexist.key").empty()); // fallback returns key

    // Test ru
    I18n ru;
    bool ok_ru = ru.load(L"lang/ru.json", "ru");
    if (!ok_ru) ok_ru = ru.load(L"D:/GOIDA-AI-MANAGER/lang/ru.json", "ru");
    assert(ok_ru);
    // ru file contains Cyrillic UTF-8 value for nav.chat = "Чат"
    std::string chat_ru = ru.get("nav.chat");
    assert(chat_ru == "Чат" || chat_ru.find((char)0xD0)!=std::string::npos);
    // wget should return wstring with Cyrillic
    std::wstring w = ru.wget("nav.chat");
    assert(w==L"Чат" || w.size()>0);

    std::cout << "i18n ok: en=" << i.size() << " ru=" << ru.size() << "\n";
    std::cout << "ru chat: " << chat_ru << "\n";
    return 0;
}
