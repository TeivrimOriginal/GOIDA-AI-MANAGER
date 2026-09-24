#include <cassert>
#include <iostream>
#include "ollama_client.h"

int main() {
    auto base = goida::ollama::ParseUrl(L"http://localhost:11434/api/chat");
    assert(base.ok);
    assert(!base.https);
    assert(base.host == L"localhost");
    assert(base.port == 11434);
    assert(base.path == L"/api/chat");

    auto secure = goida::ollama::ParseUrl(L"https://ollama.example:443/api/generate");
    assert(secure.ok);
    assert(secure.https);
    assert(secure.host == L"ollama.example");
    assert(secure.port == 443);
    assert(secure.path == L"/api/generate");

    assert(!goida::ollama::ParseUrl(L"not-a-url").ok);
    assert(goida::ollama::BuildPullUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/pull");
    assert(goida::ollama::BuildDeleteUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/delete");
    assert(goida::ollama::BuildShowUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/show");
    assert(goida::ollama::BuildEmbedUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/embed");
    assert(goida::ollama::BuildPsUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/ps");
    assert(goida::ollama::BuildVersionUrl(L"http://localhost:11434/api/chat") == L"http://localhost:11434/api/version");

    std::cout << "ollama client ok\n";
    return 0;
}
